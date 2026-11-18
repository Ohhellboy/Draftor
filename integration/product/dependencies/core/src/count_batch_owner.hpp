// Included after the original BoundedUserState methods and Impl.
CountTicket BoundedUserState::begin_counts(const StateView& view,const void* keys,std::size_t stride,std::size_t n,CountBatchAllocator a){
 auto& p=*impl_;
 if(!valid()||!view_is_current(view)||!keys||!n||n>StateBatch::Limit||stride<32||p.rank.ticket.revision==max_version)return {};
 // Build before replacement: failed local admission must not poison the owner.
 std::shared_ptr<StateBatch> next;
 try{next=std::make_shared<StateBatch>(keys,stride,n,a);}catch(const std::bad_alloc&){return {};}catch(const std::invalid_argument&){return {};}
 p.file.cancel_read(p.rank.io);const auto rev=p.rank.ticket.revision+1;
 p.rank={};p.rank.ticket={view,rev};p.rank.active=true;p.rank.batch=std::move(next);
 return p.rank.ticket;
}
CountResult BoundedUserState::advance_counts(const CountTicket& ticket,const WorkBudget& b){
 auto& p=*impl_;auto batch=p.rank.batch;CountResult out;out.ticket=ticket;out.status=CountStatus::Pending;
 struct Account {StateCost& total;CountResult& r;~Account(){total.add({r.cost.read_calls,0,r.cost.read_bytes,0,0,r.cost.work_items});}} account{p.costs,out};
 auto owned=[&]{return batch&&p.owns(ticket)&&p.rank.batch==batch;};
 // Calls are serial, with no recursive scheduling. Validate the dynamic port
 // once on entry and after lower-layer work, not once per RAM slot. Local view
 // fields are still checked after every stop callback and before each step.
 auto same_view=[&]{return owned()&&!p.exhausted&&!p.unavailable&&ticket.view.count_revision==p.count_revision&&ticket.view.availability_revision==p.availability;};
 auto validate=[&]{
  if(!owned()||!view_is_current(ticket.view)){out.status=CountStatus::Stale;return false;}
  // view_is_current already refreshes storage_current and availability.
  if(p.unavailable){out.status=CountStatus::Unavailable;return false;}
  return true;
 };
 if(!validate())return out;
 p.rank.batch_stop="work_budget";
 while(out.cost.work_items<b.max_work_items&&!b.should_stop()){
  if(!same_view()){out.status=CountStatus::Stale;return out;}
  if(p.rank.batch_member<batch->size()){
   ++out.cost.work_items;out.progressed=true;
   if(p.rank.scan<p.cfg.slots){
    const auto& e=p.slots[p.rank.scan++];
    if(known(e.kind)&&e.key==batch->key(p.rank.batch_member)){batch->ram(p.rank.batch_member,e.count);++p.rank.batch_member;p.rank.scan=0;}
   }else{++p.rank.batch_member;p.rank.scan=0;}
   continue;
  }
  if(!p.rank.loading){
   ++out.cost.work_items;
   if(p.file.status()==StateStatus::Pending)break;
   if(!p.file.ready()||p.unavailable){p.read_failed();out.status=CountStatus::Unavailable;return out;}
   p.rank.io=p.file.begin_batch(*batch);p.rank.loading=true;out.progressed=true;continue;
  }
  p.rank.batch_stop="state";
  const auto state=p.file.advance_batch(p.rank.io,*batch,{b.max_read_calls-out.cost.read_calls,0,b.max_read_bytes-out.cost.read_bytes,0,0,b.max_work_items-out.cost.work_items,b.stop_requested,b.stop_context});
  out.cost.read_calls+=state.cost.read_calls;out.cost.read_bytes+=state.cost.read_bytes;out.cost.work_items+=state.cost.work_items;out.progressed|=state.progressed;
  if(!validate())return out;
  if(state.status==StateStatus::Ready){p.rank.batch_done=true;out.status=CountStatus::Found;return out;}
  if(state.status==StateStatus::Stale){
   if(state.restart_lookup&&p.rank.io.activation==p.file.activation()&&p.file.ready()){
    // Same logical owner view, new confirmed physical cutoff: restart ALL disk
    // members, keeping only that view's authoritative RAM captures.
    p.file.cancel_read(p.rank.io);p.rank.io={};p.rank.loading=false;out.progressed=true;break;
   }
   out.status=CountStatus::Stale;return out;
  }
  if(state.status!=StateStatus::Pending&&state.status!=StateStatus::InvalidBudget){p.read_failed();out.status=CountStatus::Unavailable;return out;}
  // StateV3 already used the remaining slice. Return to the product loop;
  // re-entering here would pay another full validation without new budget.
  return out;
 }
 if(owned()&&b.should_stop())p.rank.batch_stop="deadline";
 validate();
 return out;
}
CountResult BoundedUserState::counts_result(const CountTicket& t,std::size_t i)const{
 auto& p=*impl_;CountResult out;out.ticket=t;
 if(!p.owns(t)||!p.rank.batch||!p.rank.batch_done||!view_is_current(t.view))return out;
 if(!p.file.storage_current()){p.read_failed();out.status=CountStatus::Unavailable;return out;}
 bool found=false;std::uint32_t value=0;
 if(!p.rank.batch->result(i,value,found))return out;
 out.status=found?CountStatus::Found:CountStatus::NotFound;out.count=value;return out;
}
std::size_t BoundedUserState::counts_memory()const{return impl_->rank.batch?impl_->rank.batch->memory_bytes():0;}
CountBatchProgress BoundedUserState::counts_progress()const{
 const auto& r=impl_->rank;if(!r.batch)return {};
 auto out=r.batch->progress();
 if(r.batch_member<r.batch->size()){out.phase="ram";out.cursor=r.batch_member;}
 else if(!r.loading){out.phase="begin";out.cursor=r.scan;}
 if(r.batch_stop!=std::string_view("state"))out.stop=r.batch_stop;
 return out;
}
