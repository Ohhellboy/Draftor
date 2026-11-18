// Included after StateV3::Impl. Shared traversal remains under original budgets.
StateIoTicket StateV3::begin_batch(StateBatch& batch){
 auto& p=*impl_;auto& m=*batch.p_;
 if(!ready())return {};
 auto t=begin_lookup(StateReadRole::RankLookup,m.members[0].key);
 if(!t.activation)return t;
 m.meta=p.meta[p.active];m.file=p.active;m.physical=p.physical_revision;m.activation=t.activation;m.revision=t.revision;m.restart();
 ++p.counters.batch_begins;p.counters.batch_members+=m.size;
 // begin_lookup charged one identity already; report all necessary identities.
 p.counters.lookup_begins+=m.size-1;
 p.counters.batch_peak_bytes=std::max(p.counters.batch_peak_bytes,batch.memory_bytes());
 return t;
}
StateStep StateV3::advance_batch(const StateIoTicket& t,StateBatch& batch,const StateBudget& budget){
 auto& p=*impl_;auto& m=*batch.p_;StateStep out;using M=StateBatch::Impl;
 struct Observe {M& m;StateStep& out;~Observe(){m.work+=out.cost.work_items;if(!out.progressed)++m.zero_progress_returns;}} observe{m,out};
 m.stop="work_budget";
 auto owned=[&]{return t.role==StateReadRole::RankLookup&&m.activation==t.activation&&m.revision==t.revision&&read_is_owned(t);};
 auto one=[&](const StateBudget& b){
  StateStep r;
  if(!owned()){r.status=StateStatus::Stale;return r;}
  if(!ready()){r.status=StateStatus::Unavailable;return r;}
  // Lifetime validation itself may consume the remaining deadline. Do not
  // admit a new state action or I/O after it; retain the exact continuation.
  if(b.should_stop()){m.stop="deadline";++m.deadline_yields;return r;}
  if(!owned()){r.status=StateStatus::Stale;return r;}
  if(!Impl::work(b,r))return r;
  if(m.physical!=p.physical_revision){
   const auto now=p.meta[p.active];
   if(m.meta.seq+m.meta.tails!=now.seq+now.tails){r.status=StateStatus::Stale;r.restart_lookup=true;return r;}
   m.meta=now;m.file=p.active;m.physical=p.physical_revision;m.restart();++p.counters.batch_restarts;r.progressed=true;return r;
  }
  r.progressed=true;
  switch(m.phase){
   case M::Reset:
    if(m.scan<m.size){auto& v=m.members[m.scan++];if(!v.ram){v.state=0;v.count=0;}}
    else{m.scan=0;m.phase=M::Collect;}break;
   case M::Collect:
    if(m.scan<m.size){if(!m.members[m.scan].ram)m.order[m.n++]=std::uint16_t(m.scan);++m.scan;}
    else{m.width=1;m.base=0;m.phase=M::SortStart;}break;
   case M::SortStart:
    if(m.width>=m.n){m.tail=m.meta.tails;m.transfer.reset();m.phase=M::TailRead;break;}
    if(m.base>=m.n){std::swap(m.order,m.scratch);m.width*=2;m.base=0;break;}
    m.left=m.base;m.middle=std::min(m.base+m.width,m.n);m.right=m.middle;
    m.end=std::min(m.base+2*m.width,m.n);m.out=m.base;m.phase=M::SortMerge;break;
   case M::SortMerge:
    if(m.out==m.end){m.base=m.end;m.phase=M::SortStart;break;}
    if(m.left==m.middle)m.scratch[m.out++]=m.order[m.right++];
    else if(m.right==m.end)m.scratch[m.out++]=m.order[m.left++];
    else{++p.counters.batch_sort_steps;
     if(m.members[m.order[m.left]].key<m.members[m.order[m.right]].key)m.scratch[m.out++]=m.order[m.left++];
     else m.scratch[m.out++]=m.order[m.right++];
    }break;
   case M::TailRead:{
    if(!m.tail||!m.n){m.scan=m.out=0;m.phase=M::Compact;break;}
    const auto pos=m.transfer.pos;
    if(p.transfer(m.transfer,m.file,m.meta.tail_start()+(m.tail-1)*frame_bytes,frame_bytes,false,b,r,false,m.meta.size)){
     ++p.counters.tail_frame_fetches;++p.counters.batch_tail_frames;
     if(!p.checked_frame(m.transfer.b,m.meta.seq+m.tail,&m.pivot)){r.status=StateStatus::TailUncertain;break;}
     m.lo=0;m.hi=m.n;m.phase=M::TailSearch;
    }else r.progressed=m.transfer.pos!=pos;break;
   }
   case M::TailSearch:
    if(m.lo<m.hi){const auto mid=m.lo+(m.hi-m.lo)/2;++p.counters.tail_key_comparisons;if(m.compare(mid,m.pivot.key)<0)m.lo=mid+1;else m.hi=mid;}
    else{m.scan=m.lo;m.phase=M::TailMatch;}break;
   case M::TailMatch:
    if(m.scan<m.n&&m.compare(m.scan,m.pivot.key)==0){
     ++p.counters.tail_key_comparisons;auto& v=m.members[m.order[m.scan++]];
     // Reverse confirmed-frame order: first match is the latest absolute value.
     if(!v.state){v.state=1;v.count=m.pivot.count;}
    }else{--m.tail;m.transfer.reset();m.phase=M::TailRead;}break;
   case M::Compact:
    if(m.scan<m.n){const auto i=m.order[m.scan++];if(!m.members[i].state)m.order[m.out++]=i;}
    else{m.n=m.out;if(m.n){m.nodes[0]={0,m.meta.keys,0,std::uint32_t(m.n)};m.top=1;}m.phase=M::Pop;}break;
   case M::Pop:{
    if(!m.top){m.phase=M::Done;r.status=StateStatus::Ready;break;}
    m.node=m.nodes[--m.top];m.scan=m.node.first;
    if(m.node.lo==m.node.hi){m.phase=M::Empty;break;}
    const auto span=m.node.hi-m.node.lo;
    unsigned depth=0;for(auto x=span;x;x>>=1)++depth;
    m.stream=span<=std::uint64_t(m.node.last-m.node.first)*depth;
    m.row=m.stream?m.node.lo:m.node.lo+span/2;m.transfer.reset();m.phase=M::Read;break;
   }
   case M::Empty:
    if(m.scan<m.node.last){auto& v=m.members[m.order[m.scan++]];v.state=2;v.count=0;}
    else m.phase=M::Pop;break;
   case M::Read:{
    const auto pos=m.transfer.pos;
    if(p.transfer(m.transfer,m.file,header_bytes+m.row*record_bytes,record_bytes,false,b,r,false,m.meta.tail_start(),!m.stream)){
     m.pivot=value(m.transfer.b);++p.counters.snapshot_decodes;
     if(m.stream){++p.counters.batch_stream_records;m.phase=M::StreamMatch;}
     else{++p.counters.batch_pivots;m.lo=m.node.first;m.hi=m.node.last;m.phase=M::Search;}
    }else r.progressed=m.transfer.pos!=pos;break;
   }
   case M::Search:
    if(m.lo<m.hi){const auto mid=m.lo+(m.hi-m.lo)/2;++p.counters.snapshot_key_comparisons;if(m.compare(mid,m.pivot.key)<0)m.lo=mid+1;else m.hi=mid;}
    else{m.split=m.equal_end=m.lo;m.phase=M::Equal;}break;
   case M::Equal:
    if(m.equal_end<m.node.last){++p.counters.snapshot_key_comparisons;
     if(m.compare(m.equal_end,m.pivot.key)==0){auto& v=m.members[m.order[m.equal_end++]];v.state=1;v.count=m.pivot.count;break;}}
    m.phase=M::Push;break;
   case M::Push:
    if(m.equal_end<m.node.last){if(m.top==m.node_cap){r.status=StateStatus::Unavailable;break;}m.nodes[m.top++]={m.row+1,m.node.hi,std::uint32_t(m.equal_end),m.node.last};}
    if(m.split>m.node.first){if(m.top==m.node_cap){r.status=StateStatus::Unavailable;break;}m.nodes[m.top++]={m.node.lo,m.row,m.node.first,std::uint32_t(m.split)};}
    m.phase=M::Pop;break;
   case M::StreamMatch:{
    if(m.scan==m.node.last){m.phase=M::Pop;break;}
    ++p.counters.snapshot_key_comparisons;const auto cmp=m.compare(m.scan,m.pivot.key);
    if(cmp<=0){auto& v=m.members[m.order[m.scan++]];v.state=cmp?2:1;v.count=cmp?0:m.pivot.count;}
    else if(++m.row==m.node.hi)m.phase=M::Empty;
    else{m.transfer.reset();m.phase=M::Read;}break;
   }
   case M::Done:r.status=StateStatus::Ready;break;
  }
  // Never publish I/O results after a replaced rank or retired source.
  if(!owned()){r.status=StateStatus::Stale;r.restart_lookup=false;}
  else if(!ready())r.status=StateStatus::Unavailable;
  return r;
 };
 if(!owned()){out.status=StateStatus::Stale;return out;}
 while(out.cost.work_items<budget.work_items&&!budget.should_stop()){
  StateBudget left{budget.read_calls-out.cost.read_calls,0,budget.read_bytes-out.cost.read_bytes,0,0,budget.work_items-out.cost.work_items,budget.stop_requested,budget.stop_context};
  auto s=one(left);out.status=s.status;out.cost.add(s.cost);out.needs_budget=s.needs_budget;out.restart_lookup=s.restart_lookup;out.progressed|=s.progressed;
  if(s.status!=StateStatus::Pending){m.stop=s.status==StateStatus::Ready?"done":"state_error";break;}
  if(!s.progressed){if(s.needs_budget)m.stop="io_budget";break;}
 }
 if(out.status==StateStatus::Pending&&budget.should_stop()&&std::strcmp(m.stop,"deadline")!=0){m.stop="deadline";++m.deadline_yields;}
 return out;
}
