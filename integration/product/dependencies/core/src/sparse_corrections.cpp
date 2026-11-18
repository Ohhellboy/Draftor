#include "hanime/sparse_corrections.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <cstdlib>
namespace hanime {
namespace {
constexpr std::uint32_t none=0xffffffffU;
std::uint32_t hash_code(const std::array<char,32>& c){std::uint32_t h=2166136261U;for(auto x:c)h=(h^std::uint8_t(x))*16777619U;return h;}
std::uint32_t hash_member(const StableLexemeKey& k,const std::array<char,32>& c){auto h=hash_code(c);for(auto x:k.bytes)h=(h^x)*16777619U;return h;}
bool same(const StateView& a,const StateView& b){return a.owner==b.owner&&a.registry_namespace==b.registry_namespace&&a.availability_revision==b.availability_revision;}
std::uint64_t get(const std::uint8_t* p,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(p[i])<<(i*8);return v;}
}
struct SparseCorrections::Impl {
 struct Node { RankItem item;std::array<char,32> code{};std::uint32_t next_code{none},next_member{none}; };
 BoundedUserState& owner;SparseResourcePort& port;SparseConfig cfg;SparseStatistics stats;
 std::shared_ptr<VerifiedResourceReader> reader;ResourceReadContext context{std::make_shared<int>(0),1};
 Node* nodes{nullptr};std::uint32_t* codes{nullptr};std::uint32_t* members{nullptr};
 std::size_t used{0},clear_at{0},ram_n{0},ram_at{0};std::array<StateEntry,64> ram{};
 StateExportCursor exported;StateView target{},covered{};std::uint64_t generation{0},revision{0};
 std::array<char,32> query_code{};std::uint32_t query_next{none},member_next{none};StableLexemeKey member_key;bool member_active{false};
 std::array<std::uint8_t,128> header{};std::array<std::uint8_t,96> row{};
 std::uint32_t rows{0},lo{0},hi{0},row_at{0},chain{none};
 CountChange change;Node pending;bool complete{false},building{false},active{false};
 enum Phase { Open, Bootstrap, Header, Clear, Export, Ram, Journal, Lower, Rows, Upsert, Query, Done, Failed } phase{Open},resume{Export};
 const char* reason="none";
 Impl(BoundedUserState& o,SparseResourcePort& p,SparseConfig c):owner(o),port(p),cfg(c){owner.enable_change_journal();generation=port.sparse_generation();}
 ~Impl(){auto free=cfg.release?cfg.release:std::free;free(nodes);free(codes);free(members);}
 SparseStep fallback(const char* why,WorkCost cost={}){reason=why;phase=Failed;complete=false;++stats.fallbacks;return {SparseStatus::Fallback,cost,{}};}
 bool current()const{return active&&generation==port.sparse_generation()&&owner.view_is_current(target);}
 void start_key(const StableLexemeKey& key,std::uint32_t count,Phase next){
  change.key=key;change.count=count;resume=next;lo=0;hi=rows;phase=count?Lower:next;
 }
 SparseStep read_row(std::uint32_t at,const WorkBudget& b){
  auto s=reader->read_budgeted(128+std::size_t(at)*96,row.data(),96,b,context);
  stats.static_reads+=s.cost.read_calls;stats.static_bytes+=s.cost.read_bytes;
  if(s.status==ResourceReadStatus::Error)return fallback("reverse authentication/read",s.cost);
  return {s.status==ResourceReadStatus::Ready?SparseStatus::Ready:SparseStatus::Pending,s.cost,{}};
 }
 bool decode(){
  std::copy_n(row.begin(),32,pending.item.key.bytes.begin());
  pending.item.score=static_cast<std::int64_t>(get(row.data()+32,8));
  pending.item.ref=static_cast<std::uint32_t>(get(row.data()+40,4));
  pending.item.scheme_weight=static_cast<std::int32_t>(get(row.data()+44,4));
  pending.item.path_count=static_cast<std::uint32_t>(get(row.data()+48,4));
  pending.item.representative_ordinal=static_cast<std::uint32_t>(get(row.data()+52,4));
  std::memcpy(pending.code.data(),row.data()+64,32);pending.item.effective_count=change.count;
  return pending.item.path_count&&pending.item.ref&&pending.item.ref<=reader->spec().runtime_ref_count&&pending.code[0]&&get(row.data()+56,8)==0;
 }
 SparseStep one(const WorkBudget& b){
  if(!current())return {SparseStatus::Stale,{},{}};
  if(phase==Failed)return {SparseStatus::Fallback,{},{}};
  if(!b.max_work_items||b.should_stop())return {};
  SparseStep out;out.cost.work_items=1;
  switch(phase){
   case Open:{auto r=port.open_sparse(b);out.cost=r.cost;if(r.status==SparseStatus::Fallback)return fallback("reverse missing/binding",out.cost);
    if(r.status==SparseStatus::Ready){reader=r.reader;if(!reader||reader->spec().resource_format!=106||!reader->prepare_bootstrap())return fallback("reverse pin/bootstrap",out.cost);phase=Bootstrap;}return out;}
   case Bootstrap:{auto r=reader->advance_bootstrap(b);out.cost=r.cost;stats.static_reads+=r.cost.read_calls;stats.static_bytes+=r.cost.read_bytes;
    if(r.status==ResourceReadStatus::Error)return fallback("reverse bootstrap authentication",out.cost);if(r.status==ResourceReadStatus::Ready)phase=Header;return out;}
   case Header:{auto r=reader->read_budgeted(0,header.data(),128,b,context);out.cost=r.cost;stats.static_reads+=r.cost.read_calls;stats.static_bytes+=r.cost.read_bytes;
    if(r.status==ResourceReadStatus::Error)return fallback("reverse header authentication",out.cost);
    if(r.status==ResourceReadStatus::Ready){
     if(std::memcmp(header.data(),"BGSREV1!",8)||get(header.data()+8,4)!=96||get(header.data()+16,8)!=reader->size()||
        128+get(header.data()+12,4)*96!=reader->size())return fallback("reverse header dimensions",out.cost);
     rows=static_cast<std::uint32_t>(get(header.data()+12,4));phase=Clear;clear_at=0;
     if(bool(cfg.allocate)!=bool(cfg.release)||!cfg.entries||cfg.entries>65536||!cfg.code_buckets||cfg.code_buckets>65536||!cfg.member_buckets||cfg.member_buckets>131072)return fallback("capacity configuration",out.cost);
     if(!nodes){auto alloc=cfg.allocate?cfg.allocate:std::malloc;
      nodes=static_cast<Node*>(alloc(cfg.entries*sizeof(Node)));codes=static_cast<std::uint32_t*>(alloc(cfg.code_buckets*4));members=static_cast<std::uint32_t*>(alloc(cfg.member_buckets*4));}
     if(!nodes||!codes||!members){auto free=cfg.release?cfg.release:std::free;free(nodes);free(codes);free(members);nodes=nullptr;codes=members=nullptr;return fallback("allocation capacity",out.cost);}
     stats.allocated_bytes=cfg.entries*sizeof(Node)+(cfg.code_buckets+cfg.member_buckets)*4+sizeof(Impl)+sizeof(SparseCorrections);
    }return out;}
   case Clear:
    if(clear_at<cfg.code_buckets)codes[clear_at]=none;
    else members[clear_at-cfg.code_buckets]=none;
    if(++clear_at>=cfg.code_buckets+cfg.member_buckets){
     used=0;stats.entries=0;complete=false;building=true;++stats.builds;
     if(!owner.state_backend().begin_export(exported))return fallback("authority export unavailable",out.cost);
     ram_n=owner.snapshot_ram(ram.data(),ram.size());ram_at=0;
     if(ram_n>ram.size())return fallback("RAM snapshot capacity",out.cost);phase=Export;
    }break;
   case Export:{auto s=owner.state_backend().advance_export(exported,{b.max_read_calls,0,b.max_read_bytes,0,0,b.max_work_items,b.stop_requested,b.stop_context});
    out.cost={s.step.cost.read_calls,s.step.cost.read_bytes,s.step.cost.work_items};stats.state_reads+=out.cost.read_calls;stats.state_bytes+=out.cost.read_bytes;
    if(s.item){++stats.state_records;start_key(s.key,s.step.count,Export);}
    else if(s.complete)phase=Ram;
    else if(s.step.status!=StateStatus::Pending)return fallback("authority generation changed",out.cost);return out;}
   case Ram:
    if(ram_at<ram_n){const auto& e=ram[ram_at++];++stats.ram_records;
     if(e.kind==StateSlotKind::Dirty||e.kind==StateSlotKind::Clean)start_key(e.key,e.count,Ram);
    }else{covered=target;revision=target.count_revision;complete=true;building=false;phase=Query;}break;
   case Journal:
    if(revision==target.count_revision){covered=target;complete=true;phase=Query;break;}
    if(!owner.change(revision+1,change))return fallback("change journal overflow");
    ++revision;++stats.updates;start_key(change.key,change.count,Journal);break;
   case Lower:
    if(lo<hi){const auto mid=lo+(hi-lo)/2;out=read_row(mid,b);if(out.status==SparseStatus::Fallback)return out;
     if(out.status==SparseStatus::Ready){StableLexemeKey k;std::copy_n(row.begin(),32,k.bytes.begin());++stats.comparisons;++stats.reverse_rows;
      if(k<change.key)lo=mid+1;else hi=mid;out.status=SparseStatus::Pending;}
    }else{row_at=lo;phase=Rows;}return out;
   case Rows:
    if(row_at==rows){phase=resume;break;}
    out=read_row(row_at,b);if(out.status==SparseStatus::Fallback)return out;
    if(out.status==SparseStatus::Ready){++stats.reverse_rows;++stats.comparisons;
     if(!decode())return fallback("reverse row invalid");
     if(pending.item.key!=change.key){phase=resume;out.status=SparseStatus::Pending;return out;}
     ++stats.mapping_rows;chain=members[hash_member(pending.item.key,pending.code)%cfg.member_buckets];phase=Upsert;out.status=SparseStatus::Pending;
    }return out;
   case Upsert:
    if(chain!=none){auto& n=nodes[chain];++stats.comparisons;
     if(n.item.key==pending.item.key&&n.code==pending.code){n.item.effective_count=change.count;++row_at;phase=Rows;}
     else chain=n.next_member;
    }else{
     if(used==cfg.entries)return fallback("dynamic correction capacity",out.cost);
     const auto ch=hash_code(pending.code)%cfg.code_buckets,mh=hash_member(pending.item.key,pending.code)%cfg.member_buckets;
     pending.next_code=codes[ch];pending.next_member=members[mh];new(nodes+used)Node(pending);codes[ch]=members[mh]=static_cast<std::uint32_t>(used++);
     stats.entries=used;stats.peak_entries=std::max(stats.peak_entries,used);++row_at;phase=Rows;
    }break;
   case Query:query_next=codes[hash_code(query_code)%cfg.code_buckets];member_active=false;++stats.queries;phase=Done;out.status=SparseStatus::Ready;break;
   case Done:out.status=SparseStatus::Ready;out.cost={};break;
   case Failed:out.status=SparseStatus::Fallback;break;
  }return out;
 }
};
SparseCorrections::SparseCorrections(BoundedUserState& o,SparseResourcePort& r,SparseConfig c):p_(std::make_unique<Impl>(o,r,c)){}
SparseCorrections::SparseCorrections()=default;
SparseCorrections::~SparseCorrections(){if(p_)cancel();}
void SparseCorrections::begin(const StateView& v,std::string_view code){
 auto& p=*p_;if(p.reader)p.reader->cancel_read_budgeted(p.context);++p.context.revision;p.target=v;p.active=true;p.member_active=false;
 if(code.empty()||code.size()>32){p.fallback("code length");return;}
 p.query_code={};std::copy(code.begin(),code.end(),p.query_code.begin());
 if(p.generation!=p.port.sparse_generation()){p.reader.reset();p.generation=p.port.sparse_generation();p.complete=false;p.phase=Impl::Open;++p.stats.rebuilds;}
 else if(p.complete&&same(p.covered,v)&&v.count_revision>=p.covered.count_revision){p.revision=p.covered.count_revision;p.phase=p.revision==v.count_revision?Impl::Query:Impl::Journal;}
 else if(p.reader&&p.reader->healthy()){p.phase=p.nodes?Impl::Clear:Impl::Header;p.clear_at=0;++p.stats.rebuilds;}
 else p.phase=Impl::Open;
}
SparseStep SparseCorrections::prepare(const WorkBudget& b){
 SparseStep out;auto& p=*p_;
 while(out.cost.work_items<b.max_work_items&&!b.should_stop()){
  WorkBudget left{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,b.max_work_items-out.cost.work_items,b.stop_requested,b.stop_context};
  auto s=p.one(left);out.status=s.status;out.cost.read_calls+=s.cost.read_calls;out.cost.read_bytes+=s.cost.read_bytes;out.cost.work_items+=s.cost.work_items;
  if(s.status!=SparseStatus::Pending||(!s.cost.work_items&&!s.cost.read_calls))break;
  if(out.cost.read_calls==b.max_read_calls||out.cost.read_bytes==b.max_read_bytes)break;
 }
 p.stats.work+=out.cost.work_items;return out;
}
SparseStep SparseCorrections::next(const WorkBudget& b){
 auto& p=*p_;if(!current())return {SparseStatus::Stale,{},{}};
 if(p.phase!=Impl::Done)return {SparseStatus::Fallback,{},{}};
 if(!b.max_work_items||b.should_stop())return {};
 if(p.query_next==none)return {SparseStatus::End,{0,0,1},{}};
 const auto& n=p.nodes[p.query_next];p.query_next=n.next_code;++p.stats.comparisons;++p.stats.work;
 if(n.code==p.query_code&&n.item.effective_count){++p.stats.corrections;return {SparseStatus::Item,{0,0,1},n.item};}
 return {SparseStatus::Pending,{0,0,1},{}};
}
SparseStep SparseCorrections::member(const StableLexemeKey& key,const WorkBudget& b){
 auto& p=*p_;if(!current())return {SparseStatus::Stale,{},{}};
 if(!b.max_work_items||b.should_stop())return {};
 if(!p.member_active||p.member_key!=key){p.member_key=key;p.member_active=true;p.member_next=p.members[hash_member(key,p.query_code)%p.cfg.member_buckets];++p.stats.members;}
 ++p.stats.work;
 if(p.member_next==none){p.member_active=false;return {SparseStatus::End,{0,0,1},{}};}
 const auto& n=p.nodes[p.member_next];p.member_next=n.next_member;++p.stats.comparisons;
 if(n.item.key==key&&n.code==p.query_code){p.member_active=false;return {n.item.effective_count?SparseStatus::Item:SparseStatus::End,{0,0,1},n.item};}
 return {SparseStatus::Pending,{0,0,1},{}};
}
std::uint64_t SparseCorrections::revision()const{return p_->context.revision;}
bool SparseCorrections::current()const{return p_->current();}
void SparseCorrections::cancel(){p_->active=false;if(p_->reader)p_->reader->cancel_read_budgeted(p_->context);}
SparseStatistics SparseCorrections::statistics()const{return p_->stats;}
const char* SparseCorrections::failure_reason()const{return p_->reason;}
}
