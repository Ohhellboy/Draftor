#pragma once
#include "hanime/candidate_session.hpp"
#include "frozen_count_source.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <limits>
#include <stdexcept>

namespace hanime::test {
inline void require_session(bool value,const char* message) {if (!value) throw std::runtime_error(message);}
inline StableLexemeKey session_key(std::size_t n) {
  StableLexemeKey key; key.bytes[0]=0x91;
  for (unsigned j=0;j<8;++j) key.bytes[31-j]=static_cast<std::uint8_t>(std::uint64_t(n)>>(8*j));
  return key;
}
// Artificial relations, NOT reviewed regional/variant language data.
inline ResourceBuildInput session_fixture(std::size_t n,bool reverse=false) {
  ResourceBuildInput in; in.registry_namespace=0x53455353494f4eULL;
  in.source_name="synthetic session identities and artificial output forms";
  in.source_version="Step2D.v0.1"; in.relation_basis="TEST declarations only, not linguistic approval";
  for (std::size_t i=1;i<=n;++i) {
    LexemeSourceRecord l; l.status=RegistryStatus::Approved; l.stable_key=session_key(i);
    l.identity_definition="test identity "+std::to_string(i); l.provenance="synthetic";
    l.base_weight=static_cast<std::int32_t>(i); l.default_output_utf8="同文𨿍";
    l.output_relations.push_back({"test:regional","R:地区","","artificial",false,ConditionKind::None,3,OutputAdmission::Ready});
    l.output_relations.push_back({"test:variant","V:𨿍","","artificial",false,ConditionKind::None,3,OutputAdmission::Ready});
    l.output_relations.push_back({"condition","条件","unprovided context","test",false,ConditionKind::WordOrSense,3,OutputAdmission::Ready});
    l.output_relations.push_back({"blocked","禁用","","test",true,ConditionKind::None,3,OutputAdmission::Blocked});
    l.output_relations.push_back({"explicit","许可","","test",false,ConditionKind::None,2,OutputAdmission::Ready});
    l.output_relations.push_back({"multiple","甲","","test",true,ConditionKind::None,3,OutputAdmission::Ready});
    l.output_relations.push_back({"multiple","乙","","test",false,ConditionKind::None,3,OutputAdmission::Ready});
    in.lexemes.push_back(l);
    for (const auto& scheme:{std::string("full_pinyin"),std::string("xiaohe")})
      for (int path=0;path<3;++path) in.mappings.push_back({scheme,scheme=="full_pinyin" ? "same" : "sx",l.stable_key,path-2});
  }
  if (reverse) std::reverse(in.lexemes.begin(),in.lexemes.end());
  return in;
}
class SessionReader final : public ResourceReader {
 public:
  explicit SessionReader(std::vector<std::uint8_t> data):bytes(std::move(data)) {}
  std::size_t size() const override {return bytes.size();}
  bool read(std::size_t off,void* dest,std::size_t n) const override {
    ++calls; requested+=n;
    if (fail || off>bytes.size() || n>bytes.size()-off) return false;
    std::memcpy(dest,bytes.data()+off,n); return true;
  }
  std::vector<std::uint8_t> bytes;
  mutable std::size_t calls{0},requested{0}; bool fail{false};
};
// ONE test map underneath both reads and successful-commit writes. Full-map
// memory and artificial dirty-capacity injection are not production persistence.
class SessionCounts final : public CommitCountSource {
 public:
  explicit SessionCounts(RegistryNamespaceId ns):frozen(ns) {}
  StateView view() const override {return frozen.view();}
  bool view_is_current(const StateView& v) const override {return frozen.view_is_current(v);}
  CountTicket begin_count(const StateView& v,const StableLexemeKey& k) override {return frozen.begin_count(v,k);}
  CountResult advance_count(const CountTicket& t,const WorkBudget& b) override {return frozen.advance_count(t,b);}
  void cancel_count(const CountTicket& t) override {frozen.cancel_count(t);}
  LearningStatus notify_committed(const CommitHandle&,RegistryNamespaceId ns,const StableLexemeKey& key) noexcept override {
    ++notifications; last_namespace=ns; last_key=key;
    if (ns!=frozen.current.registry_namespace || frozen.unavailable || reject_learning) return LearningStatus::PausedUnavailable;
    if (unknown_value) return LearningStatus::PausedUnknown;
    if (!dirty.count(key) && dirty.size()>=dirty_capacity) return LearningStatus::PausedCapacity;
    try {
      const auto old=frozen.values.count(key) ? frozen.values.at(key) : 0;
      dirty.insert(key);
      if (old!=std::numeric_limits<std::uint32_t>::max()) frozen.set(key,old+1);
      return LearningStatus::RecordedDirty;
    } catch (...) {return LearningStatus::PausedCapacity;}
  }
  void flush(bool success) {if (success) dirty.clear(); frozen.same_value_flush();}
  FrozenCounts frozen;
  std::set<StableLexemeKey> dirty;
  std::size_t dirty_capacity{1024},notifications{0};
  RegistryNamespaceId last_namespace{0}; StableLexemeKey last_key;
  bool reject_learning{false},unknown_value{false};
};
class SessionSink final : public TextCommitSink {
 public:
  bool commit(std::string_view text) override {
    const auto start=std::chrono::steady_clock::now(); ++calls;
    last_address=reinterpret_cast<std::uintptr_t>(text.data());
    const std::string before(text);
    if (callback) callback();
    require_session(text==before,"sink borrowed text changed during callback");
    if (throw_unknown) throw std::runtime_error("simulated unknown sink outcome");
    if (accept) accepted.emplace_back(text);
    nanoseconds+=static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
    return accept;
  }
  bool accept{true},throw_unknown{false};
  std::function<void()> callback;
  std::size_t calls{0},nanoseconds{0}; std::uintptr_t last_address{0}; std::vector<std::string> accepted;
};
struct SessionFixture {
  ResourceBuildInput input;
  BuiltResources built;
  PagedSharedResource shared;
  PagedSchemeIndex full,xiaohe;
  SessionCounts counts;
  SessionSink sink;
  std::shared_ptr<SessionReader> lr,fr,xr;
  explicit SessionFixture(std::size_t n,bool reverse=false):input(session_fixture(n,reverse)),counts(input.registry_namespace) {
    ResourceBuildOptions options; options.shared_page_bytes=options.scheme_page_bytes=96;
    auto b=ResourceBuilder().build(input,options); require_session(b.ok,b.error.c_str()); built=std::move(b.resources);
    mount();
  }
  void mount_shared() {
    lr=std::make_shared<SessionReader>(built.shared_bundle); std::string error;
    require_session(shared.open(lr,input.registry_namespace,built.manifest.shared_bundle_id,9,{1,17},&error),error.c_str());
    const auto ticket=shared.validation_cursor();
    for (;;) {auto r=shared.advance_validation(ticket,{1,17,3});
      if (r.status==ValidationStepStatus::Ready) break;
      require_session(r.status==ValidationStepStatus::InProgress,r.error.c_str());}
  }
  void mount() {
    mount_shared(); std::string error;
    fr=std::make_shared<SessionReader>(built.scheme_bundles.at("full_pinyin"));
    xr=std::make_shared<SessionReader>(built.scheme_bundles.at("xiaohe"));
    for (const auto& item:{std::make_pair(&full,fr),std::make_pair(&xiaohe,xr)}) {
      require_session(item.first->open(item.second,input.registry_namespace,built.manifest.shared_bundle_id,
          static_cast<std::uint32_t>(input.lexemes.size()),{1,17},&error),error.c_str());
      for (;;) {const auto r=item.first->advance_validation({1,17,3});
        if (r.status==ValidationStepStatus::Ready) break;
        require_session(r.status==ValidationStepStatus::InProgress,r.error.c_str());}
    }
  }
  std::size_t read_calls() const {return lr->calls+fr->calls+xr->calls;}
  std::size_t read_bytes() const {return lr->requested+fr->requested+xr->requested;}
};
} // namespace hanime::test
