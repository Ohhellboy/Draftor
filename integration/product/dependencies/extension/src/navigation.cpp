#include "hanime_extension/navigation.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hanime_extension {
namespace {
#include "navigation_syllables.inc"
NavigationConfig checked(NavigationConfig c) {
  if(!c.raw_bytes||c.raw_bytes>256||!c.units||c.units>c.raw_bytes)
    throw std::invalid_argument("Invalid bounded navigation configuration");
  return c;
}
struct Node {
  std::uint16_t unknown{0}, incomplete{0}, units{0}, next{0};
  NavigationKind kind{NavigationKind::Unknown};
  std::uint8_t ways{1};
};
}
struct SyllableNavigator::Impl {
  NavigationConfig cfg;
  std::vector<Node> work;
  std::vector<NavigationSpan> spans;
  std::vector<std::uint16_t> boundaries;
  std::vector<std::uint16_t> prefix_ends;
  std::size_t focus{0}, prefix_count{0};
  std::shared_ptr<const OwnerIdentity> owner{std::make_shared<OwnerIdentity>()};
  InputStamp source;
  std::uint64_t generation{0};
  std::size_t n{0}, count{0};
  bool published{false}, ambiguous{false}, unknown{false}, incomplete{false};
  NavigationCost cost;
  explicit Impl(NavigationConfig c):cfg(checked(c)),work(cfg.raw_bytes+1),
    spans(cfg.units),boundaries(cfg.units+1),prefix_ends(cfg.raw_bytes+1) {}
  bool current(const NavigationHandle& h,const Composition& c) const {
    return published&&h.owner_.lock()==owner&&h.generation_==generation&&c.input_is_current(source);
  }
  void offer(std::size_t i,std::size_t end,NavigationKind kind,Node& best) {
    Node candidate=work[end];candidate.next=static_cast<std::uint16_t>(end);candidate.kind=kind;
    ++candidate.units;
    if(kind==NavigationKind::Unknown)candidate.unknown+=static_cast<std::uint16_t>(end-i);
    if(kind==NavigationKind::IncompleteTail)candidate.incomplete+=static_cast<std::uint16_t>(end-i);
    const bool better=candidate.unknown<best.unknown||
      (candidate.unknown==best.unknown&&candidate.incomplete<best.incomplete);
    if(better){best=candidate;return;}
    if(candidate.unknown!=best.unknown||candidate.incomplete!=best.incomplete)return;
    const auto ways=static_cast<std::uint8_t>(std::min<unsigned>(2,best.ways+candidate.ways));
    if(candidate.units<best.units||(candidate.units==best.units&&candidate.next>best.next))best=candidate;
    best.ways=ways;
  }
  void full_pinyin(std::string_view raw, std::size_t begin, std::size_t end) {
    work[end]=Node{};
    for(std::size_t i=end;i-- >begin;) {
      Node best;best.unknown=std::numeric_limits<std::uint16_t>::max();
      offer(i,i+1,NavigationKind::Unknown,best);
      bool tail=false;
      for(const auto& syllable:kSyllables) {
        ++cost.table_entries;
        std::size_t j=0;
        while(j<6&&syllable[j]&&i+j<end) {
          ++cost.character_comparisons;
          if(syllable[j]!=raw[i+j])break;
          ++j;
        }
        if(!syllable[j]) {
          offer(i,i+j,NavigationKind::CompleteSyllable,best);
          prefix_ends[i]|=static_cast<std::uint16_t>(1U<<(j-1));
        }
        else if(i+j==end)tail=true;
      }
      if(tail)offer(i,end,NavigationKind::IncompleteTail,best);
      work[i]=best;
    }
    count+=work[begin].units;ambiguous|=work[begin].ways>1;
  }
  void complete_prefixes(SchemeRules rules) {
    // Low six bits hold the syllable lengths already found by full_pinyin.
    // Propagate reachability, then compact absolute endpoints in the same array.
    // Compacting writes only behind the unread indices.
    constexpr std::uint16_t reached=1U<<6;
    prefix_count=0;
    prefix_ends[focus]|=reached;
    if(rules==SchemeRules::R1Xiaohe) {
      for(std::size_t end=focus+2;end<=n;end+=2)prefix_ends[end]|=reached;
    } else {
      for(std::size_t begin=focus;begin<n;++begin) {
        if(!(prefix_ends[begin]&reached))continue;
        for(std::size_t length=1;length<=6&&begin+length<=n;++length)
          if(prefix_ends[begin]&(1U<<(length-1)))prefix_ends[begin+length]|=reached;
      }
    }
    for(std::size_t end=focus+1;end<=n;++end) {
      if(prefix_ends[end]&reached)prefix_ends[prefix_count++]=static_cast<std::uint16_t>(end);
    }
  }
};
SyllableNavigator::SyllableNavigator(NavigationConfig c):p_(std::make_unique<Impl>(c)) {}
SyllableNavigator::~SyllableNavigator()=default;
void SyllableNavigator::reset(){p_->published=false;}
NavigationResult SyllableNavigator::analyze(const Composition& c) {
  auto& p=*p_;p.published=false;p.cost={};p.ambiguous=p.unknown=p.incomplete=false;p.count=p.prefix_count=0;
  NavigationResult r;
  if(p.generation==UINT64_MAX){r.status=NavigationStatus::CapacityExceeded;r.capacity=NavigationCapacity::Generation;return r;}
  ++p.generation;
  const auto input=c.raw_input();
  if(!c.input_is_current(input.stamp))return r;
  p.n=input.raw.size();
  if(p.n>p.cfg.raw_bytes){r.status=NavigationStatus::CapacityExceeded;r.capacity=NavigationCapacity::RawBytes;r.required=p.n;return r;}
  for(char ch:input.raw)if(ch<'a'||ch>'z'){r.status=NavigationStatus::InvalidRequest;return r;}
  const auto snapshot=c.snapshot();
  p.focus=snapshot.focus;
  std::fill(p.prefix_ends.begin(),p.prefix_ends.end(),0);
  // Exact codes have no syllable segmentation. Preserve a current, empty
  // presentation; raw cursor movement remains byte-oriented below.
  if(input.rules==SchemeRules::DirectExact) {
    p.boundaries[0]=0;p.source=input.stamp;p.published=true;
    r.status=NavigationStatus::Ready;r.handle.owner_=p.owner;r.handle.generation_=p.generation;return r;
  }
  if(input.rules==SchemeRules::FullPinyin) {
    // Preserve every confirmed member boundary, then parse the unselected
    // remainder independently. A trailing g cannot migrate into an old member.
    for(std::size_t i=0;i<snapshot.member_count;++i) {
      const auto& member=snapshot.members[i];
      if(!member.valid||member.end>p.focus)break;
      p.full_pinyin(input.raw,member.begin,member.end);
    }
    p.full_pinyin(input.raw,p.focus,p.n);
  }
  else if(input.rules==SchemeRules::R1Xiaohe)p.count=(p.n+1)/2;
  else {r.status=NavigationStatus::InvalidRequest;return r;}
  if(p.count>p.cfg.units){r.status=NavigationStatus::CapacityExceeded;r.capacity=NavigationCapacity::Units;r.required=p.count;return r;}
  p.boundaries[0]=0;
  for(std::size_t i=0,index=0;i<p.n;++index) {
    const auto end=input.rules==SchemeRules::FullPinyin?p.work[i].next:std::min(p.n,i+2);
    const auto kind=input.rules==SchemeRules::FullPinyin?p.work[i].kind:
      end-i==2?NavigationKind::CodePair:NavigationKind::IncompleteTail;
    p.spans[index]={static_cast<std::uint16_t>(i),static_cast<std::uint16_t>(end),kind};
    p.boundaries[index+1]=static_cast<std::uint16_t>(end);
    p.unknown|=kind==NavigationKind::Unknown;p.incomplete|=kind==NavigationKind::IncompleteTail;i=end;
  }
  p.complete_prefixes(input.rules);
  p.source=input.stamp;p.published=true;
  r.status=NavigationStatus::Ready;r.handle.owner_=p.owner;r.handle.generation_=p.generation;return r;
}
NavigationView SyllableNavigator::view(const NavigationHandle& h,const Composition& c) const {
  const auto& p=*p_;if(!p.current(h,c))return {};
  return {NavigationStatus::Ready,p.boundaries.data(),p.count+1,p.spans.data(),p.count,p.ambiguous,p.unknown,p.incomplete};
}
NavigationMove SyllableNavigator::neighbor(const NavigationHandle& h,const Composition& c,std::size_t pos,NavigationDirection direction) const {
  const auto v=view(h,c);if(v.status!=NavigationStatus::Ready)return {};
  if(pos>p_->n||(direction!=NavigationDirection::Left&&direction!=NavigationDirection::Right))return {NavigationStatus::InvalidRequest,pos};
  if(c.raw_input().rules==SchemeRules::DirectExact) {
    const auto next=direction==NavigationDirection::Left?(pos?pos-1:0):std::min(p_->n,pos+1);
    return {next==pos?NavigationStatus::AtEdge:NavigationStatus::Ready,next};
  }
  if(direction==NavigationDirection::Left) {
    for(std::size_t i=v.boundary_count;i-- >0;)if(v.boundaries[i]<pos)return {NavigationStatus::Ready,v.boundaries[i]};
  } else {
    for(std::size_t i=0;i<v.boundary_count;++i)if(v.boundaries[i]>pos)return {NavigationStatus::Ready,v.boundaries[i]};
  }
  return {NavigationStatus::AtEdge,pos};
}
NavigationPrefixView SyllableNavigator::prefix_endpoints(const NavigationHandle& h,const Composition& c) const {
  const auto& p=*p_;
  if(!p.current(h,c))return {};
  return {NavigationStatus::Ready,p.focus,p.prefix_ends.data(),p.prefix_count};
}
NavigationCost SyllableNavigator::cost() const {return p_->cost;}
NavigationMemory SyllableNavigator::memory_usage() const {
  const auto& p=*p_;return {sizeof(*this),sizeof(Impl),p.work.capacity()*sizeof(Node),
    p.spans.capacity()*sizeof(NavigationSpan),p.boundaries.capacity()*sizeof(std::uint16_t),sizeof(kSyllables),
    p.prefix_ends.capacity()*sizeof(std::uint16_t)};
}
} // namespace hanime_extension
