#include "segment_planner.hpp"
#include "hanime/base_order.hpp"
#include <algorithm>
#include <vector>

namespace hanime_extension {
using namespace hanime;
namespace {
#include "navigation_syllables.inc"
constexpr std::uint16_t infinity=65535;
struct Edge {
  std::uint16_t begin{0}, end{0}, units{0};
  bool known{false}, matched{false};
  RankItem item;
};
struct Node {
  bool reached{false};
  std::uint16_t units{0}, words{0}, first{0};
  std::int32_t score{0};
};
std::int32_t ordinal(std::int32_t w) {
  std::uint32_t x=static_cast<std::uint32_t>(std::max<std::int32_t>(0,w))+1U;
  std::int32_t q=0;
  while(x>1U&&q<20){x>>=1U;++q;}
  return q;
}
Status failure(RankStatus s) {
  if(s==RankStatus::Stale)return Status::Stale;
  if(s==RankStatus::Cancelled)return Status::Cancelled;
  if(s==RankStatus::CapacityExceeded)return Status::CapacityExceeded;
  if(s==RankStatus::InvalidBudget)return Status::InvalidBudget;
  return Status::ResourceError;
}
}
struct SegmentPlanner::Impl {
  enum class Phase { Idle, Reset, Encoding, Distances, Whole, Enumerate, Lookup, Query, Solve, Done, CachedReset, CachedRanges, CodeLookup, PrefixReset, PrefixLookup };
  PagedSchemeIndex& scheme; PagedSharedResource& shared; CountSource& counts;
  Config cfg; SchemeRules rules{SchemeRules::FullPinyin};
  std::vector<char> raw, form;
  std::vector<std::uint8_t> links, tails;
  std::vector<std::uint16_t> units;
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::unique_ptr<ExactRanker> ranker;
  RankItem rank_work, rank_result;
  RankTicket ticket;
  SharedActivationHandle activation;
  std::uint64_t scheme_epoch{0}, context{0};
  OutputInvocation invocation{OutputInvocation::Profile};
  std::size_t n{0}, focus{0}, form_n{0}, used{0}, width{0};
  std::size_t pos{0}, table{0}, begin_pos{0}, end_pos{0}, span{1}, solve_edge{0}, setup{0};
  bool direct_pending{false}, facts_valid{false}, secondary{false}, query_limited{false}, fresh{true};
  std::size_t complete_end{0}, legal_ranges{0}, lookup{0}, empty_slot{0}, offered_begin{0}, offered_end{0};
  bool ordered{false};
  int complement_q{-2}; // -2 unknown, -1 proved absent, otherwise q in [0,20].
  bool geometry_ready{false};
  std::size_t code_offset{0};
  static constexpr std::size_t prefix_slots=8;
  struct PrefixSlot {
    BaseOrderPrefixState state;
    std::uint16_t begin{0}, age{0};
  };
  std::array<PrefixSlot,prefix_slots> prefixes;
  std::size_t prefix_scan{0}, prefix_slot{0};
  std::uint16_t prefix_clock{0}; // At most 64 actual queries in this graph.
  bool prefixes_live{false};
  Phase after_prefix_reset{Phase::Idle};
  Phase phase{Phase::Idle};
  PlanningReport report;
  explicit Impl(PagedSchemeIndex& s,PagedSharedResource& h,CountSource& c,Config config)
    :scheme(s),shared(h),counts(c),cfg(config),raw(cfg.raw_bytes+1),form(cfg.name_bytes),
     links(cfg.raw_bytes+1),tails(cfg.raw_bytes+1),units((cfg.raw_bytes+1)*(cfg.raw_bytes+1)),
     nodes(cfg.raw_bytes+1),edges(cfg.planning.ranges),width(cfg.raw_bytes+1) {}
  std::uint16_t& at(std::size_t i,std::size_t j){return units[i*width+j];}
  bool current() const {
    return shared.activation_is_current(activation)&&scheme.ready()&&scheme.index_epoch()==scheme_epoch;
  }
  void stop_query(){if(ranker){ranker->abandon_range(ticket);ranker.reset();}ticket={};}
  void reset_prefixes_before(Phase next) {
    prefixes_live=false;prefix_scan=0;prefix_clock=0;
    after_prefix_reset=next;phase=Phase::PrefixReset;
  }
  void reset_prefix_one() {
    auto& slot=prefixes[prefix_scan++];slot.state.reset();slot.age=0;
    if(prefix_scan==prefix_slots){prefixes_live=true;phase=after_prefix_reset;}
  }
  void finish_queries(){stop_query();reset_prefixes_before(Phase::Solve);pos=focus;solve_edge=0;nodes[focus].reached=true;}
  bool whole_bound() const {
    return complete_end==n&&used&&edges[0].known&&edges[0].matched&&
      edges[0].begin==focus&&edges[0].end==n&&
      units[focus*width+n]!=infinity&&edges[0].units==units[focus*width+n];
  }
  void offer(std::size_t i,std::size_t j) {
    if(used==edges.size()){report.limited=query_limited=true;finish_queries();return;}
    offered_begin=i;offered_end=j;lookup=whole_bound()?0:used;complement_q=-2;empty_slot=edges.size();phase=Phase::Lookup;
  }
  void lookup_one() {
    if(lookup<edges.size()) {
      const auto index=lookup++;const auto& e=edges[index];
      if(!e.known&&empty_slot==edges.size())empty_slot=index;
      if(e.known&&((offered_begin==focus&&e.begin==offered_end&&e.end==n)||
         (offered_end==n&&e.begin==focus&&e.end==offered_begin)))
        complement_q=e.matched?ordinal(e.item.scheme_weight):-1;
      if(e.known&&e.begin==offered_begin&&e.end==offered_end) {
        std::swap(edges[used],edges[index]);edges[used].units=at(offered_begin,offered_end);
        ++used;++report.cache_hits;phase=Phase::Enumerate;
      }
      return;
    }
    // A completed whole-input fact is offered first and stays at edges[0].
    // At minimal syllable count its score is >= -32; >=3 words score <= -36.
    // A strictly interior edge requires >=3 words to cover the same input.
    // Test only after cache lookup: keep known facts, never cache a skip as miss.
    if(complete_end==n&&used&&edges[0].known&&edges[0].matched&&
       edges[0].begin==focus&&edges[0].end==n&&
       at(focus,n)!=infinity&&edges[0].units==at(focus,n)&&
       offered_begin>focus&&offered_end<n) {
      phase=Phase::Enumerate;return;
    }
    // With a minimal-syllable whole word, only two-member paths can win.
    // A proved missing/weak complement cannot be rescued by any q <= 20.
    if(whole_bound()&&((offered_begin==focus&&offered_end<n)||
       (offered_begin>focus&&offered_end==n))&&
       (ordinal(edges[0].item.scheme_weight)>=8||complement_q==-1||
        (complement_q>=0&&complement_q+20-64<=ordinal(edges[0].item.scheme_weight)-32))) {
      phase=Phase::Enumerate;return;
    }
    lookup=0;code_offset=0;phase=Phase::CodeLookup;
  }
  void code_lookup_one() {
    // Only [0,used) has been admitted to this planning round. Retained slots
    // beyond it do not authorize a same-code conclusion at another position.
    if(lookup==used){prefix_scan=0;prefix_slot=0;phase=Phase::PrefixLookup;return;}
    const auto& source=edges[lookup];
    const auto length=offered_end-offered_begin;
    if(!source.known||static_cast<std::size_t>(source.end-source.begin)!=length) {
      ++lookup;code_offset=0;return;
    }
    // One byte per charged local step; cancellation/resource checks occur at
    // every advance boundary. No string allocation or uncharged bulk scan.
    if(code_offset<length) {
      if(raw[source.begin+code_offset]!=raw[offered_begin+code_offset]) {
        ++lookup;code_offset=0;
      } else ++code_offset;
      return;
    }
    const auto order=scheme.base_order();
    if(order&&!order->ready()){phase=Phase::Idle;return;}
    // Preserve the active source and old unactivated facts where possible.
    // Dictionary metadata is copied; geometry and the DP path remain local.
    if(empty_slot<edges.size())std::swap(edges[used],edges[empty_slot]);
    auto& target=edges[used];
    target=source;
    target.begin=static_cast<std::uint16_t>(offered_begin);
    target.end=static_cast<std::uint16_t>(offered_end);
    target.units=at(offered_begin,offered_end);
    ++used;++report.cache_hits;phase=Phase::Enumerate;
  }
  void prefix_lookup_one() {
    // Each label/LRU comparison uses one ordinary local-work step. An empty
    // slot has age zero; source states are never synthesized from fact hits.
    if(prefix_scan<prefix_slots) {
      const auto index=prefix_scan++;
      const auto& slot=prefixes[index];
      if(slot.age&&slot.begin==offered_begin){prefix_slot=index;prefix_scan=prefix_slots;}
      else if(slot.age<prefixes[prefix_slot].age)prefix_slot=index;
      return;
    }
    start_query();
  }
  void start_query() {
    if(report.core_cost.work_items>=cfg.planning.core_work||report.core_cost.read_bytes>=cfg.planning.read_bytes){report.limited=query_limited=true;finish_queries();return;}
    // Preserve unused old facts where possible; never evict the active graph.
    if(empty_slot<edges.size())std::swap(edges[used],edges[empty_slot]);
    auto& e=edges[used];e={};e.begin=static_cast<std::uint16_t>(offered_begin);
    e.end=static_cast<std::uint16_t>(offered_end);e.units=at(offered_begin,offered_end);
    ranker=std::make_unique<ExactRanker>(scheme,shared,counts,&rank_work,1,cfg.raw_batch,cfg.raw_bytes);
    auto s=ranker->begin_input(std::string(raw.data()+offered_begin,offered_end-offered_begin),activation.mount_epoch(),{},ScoringMode::BaseOnly);
    if(s!=RankStatus::InProgress){stop_query();phase=Phase::Idle;return;}
    auto& chain=prefixes[prefix_slot];
    if(!prefixes_live||!chain.age||chain.begin!=offered_begin)chain.state.reset();
    chain.begin=static_cast<std::uint16_t>(offered_begin);chain.age=++prefix_clock;
    s=ranker->request_first_prefix(chain.state,1,&rank_result,1,&ticket);
    if(s!=RankStatus::InProgress){stop_query();phase=Phase::Idle;return;}
    report.query_begin=offered_begin;report.query_end=offered_end;
    ++report.queries;phase=Phase::Query;
  }
  void solve_one() {
    if(solve_edge<used) {
      const auto& e=edges[solve_edge++];
      if(e.begin!=pos||!e.known||!e.matched||!nodes[pos].reached)return;
      auto candidate=nodes[pos];candidate.units+=e.units;++candidate.words;
      candidate.score+=ordinal(e.item.scheme_weight)-32;
      if(!candidate.first)candidate.first=e.end;
      auto& best=nodes[e.end];
      if(!best.reached||candidate.units<best.units||
         (candidate.units==best.units&&(candidate.score>best.score||
          (candidate.score==best.score&&(candidate.words<best.words||
           (candidate.words==best.words&&candidate.first>best.first))))))best=candidate;
      return;
    }
    solve_edge=0;
    if(nodes[pos].reached)report.covered_end=pos;
    if(pos<n&&at(focus,pos)!=infinity&&tails[pos])report.incomplete_tail=true;
    if(pos<n){++pos;return;}
    const auto& best=nodes[report.covered_end];report.end=best.first;
    report.syllables=best.units;report.words=best.words;report.score=best.score;
    // A suffix is incomplete only when it is a proper prefix of a supported
    // spelling and can follow a complete encoding path. Unknown stays unknown.
    const bool complete=at(focus,n)!=infinity;
    report.incomplete_tail=!complete&&report.incomplete_tail;
    report.unknown_tail=!complete&&!report.incomplete_tail;
    if(!secondary&&!query_limited&&(report.covered_end!=complete_end||best.units!=at(focus,complete_end))) {
      secondary=true;span=1;begin_pos=focus;end_pos=focus+1;phase=Phase::Enumerate;
      report.incomplete_tail=false;return;
    }
    report.checked_ranges=used;report.valid=true;phase=Phase::Done;
  }
};
SegmentPlanner::SegmentPlanner(PagedSchemeIndex& s,PagedSharedResource& h,CountSource& c,const Config& cfg)
 :p_(std::make_unique<Impl>(s,h,c,cfg)){}
SegmentPlanner::~SegmentPlanner(){p_->stop_query();}
Status SegmentPlanner::begin(const RawInput& input,std::size_t focus,OutputRequest request,std::uint64_t context) {
  auto& p=*p_;p.stop_query();p.report.valid=false;
  if(request.form.size()>p.form.size())return Status::CapacityExceeded;
  const bool reuse=p.facts_valid&&p.current()&&p.report.input_id==input.stamp.input_id()&&
    p.rules==input.rules&&p.context==context&&p.invocation==request.invocation&&
    p.form_n==request.form.size()&&std::equal(request.form.begin(),request.form.end(),p.form.begin());
  std::size_t prefix=0;
  if(reuse)while(prefix<std::min(p.n,input.raw.size())&&p.raw[prefix]==input.raw[prefix])++prefix;
  // Reuse only the completely solved encoding-distance subproblem.
  const bool cached_geometry=reuse&&p.geometry_ready&&prefix==p.n&&p.n==input.raw.size()&&
    focus>=p.focus&&(input.rules!=SchemeRules::R1Xiaohe||(focus-p.focus)%2==0);
  p.geometry_ready=cached_geometry;
  // Eager bounded retirement before copying raw: cancellation during Reset must
  // never leave facts bound to the previous spelling but the new raw snapshot.
  for(auto& e:p.edges)if(!reuse||e.end>prefix)e.known=false;
  p.direct_pending=false;p.facts_valid=true;p.used=0;p.n=input.raw.size();p.focus=focus;p.rules=input.rules;p.context=context;
  std::copy(input.raw.begin(),input.raw.end(),p.raw.begin());
  std::copy(request.form.begin(),request.form.end(),p.form.begin());p.form_n=request.form.size();p.invocation=request.invocation;
  p.activation=p.shared.activation_handle();p.scheme_epoch=p.scheme.index_epoch();
  p.setup=p.n+p.form_n+prefix+p.edges.size();
  if(p.fresh){p.setup+=p.raw.size()+p.form.size()+p.links.size()+p.tails.size()+p.units.size()*sizeof(std::uint16_t)+p.nodes.size()*sizeof(Node)+p.edges.size()*sizeof(Edge);p.fresh=false;}
  p.report={};p.report.input_id=input.stamp.input_id();p.report.revision=input.stamp.revision();
  p.report.context_revision=context;p.report.begin=focus;
  p.pos=0;p.table=0;p.begin_pos=focus;p.end_pos=focus+1;p.span=1;p.secondary=false;p.query_limited=false;p.complete_end=focus;p.legal_ranges=0;
  p.ordered=false;
  p.reset_prefixes_before(cached_geometry?Impl::Phase::CachedReset:Impl::Phase::Reset);return Status::Working;
}
Step SegmentPlanner::advance(const WorkBudget& budget) {
  auto& p=*p_;Step r;r.status=Status::Working;
  if(!p.current()){invalidate();r.status=Status::Stale;return r;}
  if(p.phase==Impl::Phase::Idle){r.status=Status::Cancelled;return r;}
  if(p.phase==Impl::Phase::Done){r.status=Status::Ready;return r;}
  if(p.phase==Impl::Phase::Query) {
    WorkBudget b=budget;
    b.max_work_items=std::min(b.max_work_items,p.cfg.planning.core_work-p.report.core_cost.work_items);
    b.max_read_bytes=std::min(b.max_read_bytes,p.cfg.planning.read_bytes-p.report.core_cost.read_bytes);
    if(!b.max_work_items||!b.max_read_bytes){p.report.limited=p.query_limited=true;p.finish_queries();return r;}
    const auto s=p.ranker->advance(p.ticket,b);r.core_cost=s.cost;
    p.report.core_cost.read_calls+=s.cost.read_calls;p.report.core_cost.read_bytes+=s.cost.read_bytes;p.report.core_cost.work_items+=s.cost.work_items;
    if(s.status==RankStatus::Ready||s.status==RankStatus::NoMatches) {
      // Optional checkpoint: never consume an extra tick or overrun either
      // the current slice or the unchanged total planning work budget.
      if(s.cost.work_items<budget.max_work_items&&p.report.core_cost.work_items<p.cfg.planning.core_work&&
         p.scheme.base_order()) {
        WorkBudget remaining{};remaining.max_work_items=1;
        const auto saved=p.ranker->save_prefix(p.ticket,p.prefixes[p.prefix_slot].state,remaining);
        r.core_cost.work_items+=saved.cost.work_items;
        p.report.core_cost.work_items+=saved.cost.work_items;
        if(saved.status!=RankStatus::Ready){r.status=failure(saved.status);invalidate();return r;}
      }
      auto& e=p.edges[p.used];e.known=true;e.matched=s.status==RankStatus::Ready;
      if(e.matched)e.item=p.rank_result;
      ++p.used;p.ranker->release(p.ticket);p.stop_query();
      p.phase=Impl::Phase::Enumerate;
    } else if(s.status==RankStatus::InvalidBudget) {
      // A smaller remainder may be below the core's atomic read minimum.
      if(b.max_read_bytes<budget.max_read_bytes||b.max_work_items<budget.max_work_items){p.report.limited=p.query_limited=true;p.finish_queries();}
      else r.status=Status::InvalidBudget;
    } else if(s.status!=RankStatus::InProgress&&s.status!=RankStatus::WaitingState) {
      r.status=failure(s.status);invalidate();
    }
    return r;
  }
  while(r.local_work<budget.max_work_items&&p.phase!=Impl::Phase::Query&&p.phase!=Impl::Phase::Done) {
    if(p.report.local_work>=p.cfg.planning.local_work) {
      p.prefixes_live=false;p.stop_query();p.report.limited=true;p.report.valid=true;p.report.end=0;
      p.report.covered_end=p.focus;p.report.checked_ranges=p.used;p.phase=Impl::Phase::Done;
      break;
    }
    ++r.local_work;++p.report.local_work;
    using Phase=Impl::Phase;
    if(p.phase==Phase::PrefixReset)p.reset_prefix_one();
    else if(p.phase==Phase::PrefixLookup)p.prefix_lookup_one();
    else if(p.phase==Phase::CachedReset) {
      p.nodes[p.pos]={};
      if(++p.pos==p.width){p.begin_pos=p.focus;p.end_pos=p.focus;p.phase=Phase::CachedRanges;}
    } else if(p.phase==Phase::CachedRanges) {
      const auto value=p.at(p.begin_pos,p.end_pos);
      if(p.begin_pos==p.focus&&value!=infinity)p.complete_end=p.end_pos;
      if(p.end_pos>p.begin_pos&&value!=infinity&&p.at(p.focus,p.begin_pos)!=infinity)
        p.legal_ranges=std::min<std::size_t>(2,p.legal_ranges+1);
      if(++p.end_pos>p.n){++p.begin_pos;p.end_pos=p.begin_pos;}
      if(p.begin_pos>p.n){p.phase=Phase::Whole;p.begin_pos=p.focus;p.end_pos=p.focus+1;}
    } else if(p.phase==Phase::Reset) {
      // Incremental clearing, including stale nodes from a longer old input.
      p.units[p.pos]=infinity;
      if(p.pos<p.width){p.nodes[p.pos]={};p.links[p.pos]=p.tails[p.pos]=0;}
      if(++p.pos==p.units.size()){p.pos=p.focus;p.phase=Phase::Encoding;}
    } else if(p.phase==Phase::Encoding) {
      if(p.pos==p.n){p.begin_pos=p.focus;p.end_pos=p.focus;p.phase=Phase::Distances;continue;}
      if(p.rules==SchemeRules::R1Xiaohe) {
        if((p.pos-p.focus)%2==0){if(p.pos+2<=p.n)p.links[p.pos]=2;else p.tails[p.pos]=1;}
        ++p.pos;
      } else {
        const auto& syllable=kSyllables[p.table];std::size_t j=0;
        while(j<6&&syllable[j]&&p.pos+j<p.n&&syllable[j]==p.raw[p.pos+j])++j;
        if(!syllable[j])p.links[p.pos]|=static_cast<std::uint8_t>(1U<<(j-1));
        else if(p.pos+j==p.n)p.tails[p.pos]=1;
        if(++p.table==sizeof(kSyllables)/sizeof(kSyllables[0])){p.table=0;++p.pos;}
      }
    } else if(p.phase==Phase::Distances) {
      auto& value=p.at(p.begin_pos,p.end_pos);
      if(p.end_pos==p.begin_pos)value=0;
      else for(std::size_t len=1;len<=6&&len<=p.end_pos-p.begin_pos;++len) {
        const auto prev=p.end_pos-len;
        if((p.links[prev]&(1U<<(len-1)))&&p.at(p.begin_pos,prev)!=infinity)
          value=std::min<std::uint16_t>(value,static_cast<std::uint16_t>(p.at(p.begin_pos,prev)+1));
      }
      if(p.begin_pos==p.focus&&value!=infinity)p.complete_end=p.end_pos;
      if(p.end_pos>p.begin_pos&&value!=infinity&&p.at(p.focus,p.begin_pos)!=infinity)
        p.legal_ranges=std::min<std::size_t>(2,p.legal_ranges+1);
      if(++p.end_pos>p.n){++p.begin_pos;p.end_pos=p.begin_pos;}
      if(p.begin_pos>p.n){p.geometry_ready=true;p.phase=Phase::Whole;p.begin_pos=p.focus;p.end_pos=p.focus+1;}
    } else if(p.phase==Phase::Whole) {
      p.phase=Phase::Enumerate;
      if(p.legal_ranges==1) {
        // Count ALL reachable encoding intervals, including alternate syllable
        // splits. With one interval there is no score comparison to perform.
        p.direct_pending=true;p.report.direct_range=true;p.report.valid=true;p.report.end=p.complete_end;
        p.report.incomplete_tail=p.complete_end<p.n&&(p.tails[p.focus]||p.tails[p.complete_end]);
        p.report.unknown_tail=p.complete_end<p.n&&!p.report.incomplete_tail;
        p.phase=Phase::Done;
      } else if(p.complete_end>p.focus)p.offer(p.focus,p.complete_end);
    } else if(p.phase==Phase::Enumerate) {
      const bool descending=p.whole_bound();
      if(!p.ordered){p.ordered=true;if(descending)p.span=p.cfg.planning.span_syllables;}
      if((descending&&p.span==0)||(!descending&&p.span>p.cfg.planning.span_syllables)){p.finish_queries();continue;}
      const auto i=p.begin_pos,j=p.end_pos,requested_span=p.span;
      if(++p.end_pos>p.n){++p.begin_pos;p.end_pos=p.begin_pos+1;}
      if(p.begin_pos>=p.n){if(descending)--p.span;else ++p.span;p.begin_pos=p.focus;p.end_pos=p.focus+1;}
      if(p.at(i,j)!=infinity&&p.at(p.focus,i)!=infinity) {
        const bool primary=j<=p.complete_end&&p.at(j,p.complete_end)!=infinity&&
          p.at(p.focus,i)+p.at(i,j)+p.at(j,p.complete_end)==p.at(p.focus,p.complete_end);
        const bool eligible=p.secondary?!primary:primary;
        const bool whole=i==p.focus&&j==p.complete_end;
        if(eligible&&p.at(i,j)>p.cfg.planning.span_syllables&&!whole)p.report.limited=true;
        if(eligible&&p.at(i,j)==requested_span&&!whole)p.offer(i,j);
      }
    } else if(p.phase==Phase::Lookup)p.lookup_one();
    else if(p.phase==Phase::CodeLookup)p.code_lookup_one();
    else if(p.phase==Phase::Solve)p.solve_one();
    if(p.phase==Phase::Idle){p.prefixes_live=false;r.status=Status::ResourceError;return r;}
  }
  if(p.phase==Impl::Phase::Done) {
    p.prefixes_live=false;
    r.status=p.report.end?Status::Ready:
      p.report.limited||p.report.incomplete_tail||p.report.unknown_tail?Status::Unresolved:Status::NoPrefixMatch;
  }
  return r;
}
void SegmentPlanner::accept_direct_fact(std::size_t begin,std::size_t end,const RankItem* first) {
  auto& p=*p_;
  if(!p.direct_pending||p.phase!=Impl::Phase::Done||!p.current()||begin!=p.report.begin||end!=p.report.end)return;
  // Only the first formal range completion, never a later After/Before page.
  // Copy metadata under the core result lease; no borrowed Selection survives.
  auto& e=p.edges[0];e={};e.begin=static_cast<std::uint16_t>(begin);e.end=static_cast<std::uint16_t>(end);
  e.units=p.at(begin,end);e.known=true;e.matched=first!=nullptr;if(first)e.item=*first;
  p.direct_pending=false;
}
void SegmentPlanner::cancel(){if(p_->phase!=Impl::Phase::Done)retire();}
void SegmentPlanner::retire(){p_->prefixes_live=false;p_->stop_query();p_->direct_pending=false;p_->report.valid=false;p_->phase=Impl::Phase::Idle;}
void SegmentPlanner::invalidate(){p_->facts_valid=false;retire();}
PlanningReport SegmentPlanner::report() const{return p_->report;}
std::size_t SegmentPlanner::command_work() const{return p_->setup;}
PlanningMemory SegmentPlanner::memory() const {
  const auto& p=*p_;PlanningMemory m;m.owner=sizeof(*this);m.impl=sizeof(Impl);
  m.buffers=p.raw.capacity()+p.form.capacity()+p.links.capacity()+p.tails.capacity()+p.units.capacity()*sizeof(std::uint16_t)+
    p.nodes.capacity()*sizeof(Node)+p.edges.capacity()*sizeof(Edge);m.constant_table=sizeof(kSyllables);
  if(p.ranker)m.rank=p.ranker->memory_usage();
  return m;
}
}
