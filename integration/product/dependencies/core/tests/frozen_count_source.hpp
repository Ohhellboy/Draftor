#pragma once
#include "hanime/exact_ranker.hpp"
#include <map>

// TEST ONLY: this full map and deterministic latency injection are not a
// bounded production state cache, persistence format, or a second registry.
namespace hanime::test {
class FrozenCounts final : public CountSource {
 public:
  explicit FrozenCounts(RegistryNamespaceId ns) {
    current={std::make_shared<CountOwnerIdentity>(),ns,1,1};
  }
  StateView view() const override {++view_calls; return current;}
  bool view_is_current(const StateView& v) const override {
    ++view_checks;
    return v.owner==current.owner && v.registry_namespace==current.registry_namespace &&
        v.count_revision==current.count_revision && v.availability_revision==current.availability_revision;
  }
  CountTicket begin_count(const StateView& v,const StableLexemeKey& key) override {
    ++begins;
    if (!view_is_current(v)) return {};
    active={v,++serial}; pending_key=key; remaining=delay;
    return active;
  }
  CountResult advance_count(const CountTicket& t,const WorkBudget&) override {
    ++advances;
    CountResult result; result.ticket=t; result.cost.work_items=1;
    if (!view_is_current(t.view) || t.revision!=active.revision) return result;
    if (inject_old) {result.ticket=old; inject_old=false; result.status=CountStatus::Found; result.count=999; return result;}
    if (unavailable || (fail_on_begin && begins>=fail_on_begin)) {result.status=CountStatus::Unavailable; return result;}
    if (remaining) {--remaining; result.status=CountStatus::Pending; return result;}
    auto it=values.find(pending_key);
    result.status=it==values.end() ? CountStatus::NotFound : CountStatus::Found;
    if (it!=values.end()) result.count=it->second;
    return result;
  }
  void cancel_count(const CountTicket& t) override {
    if (t.revision && t.revision==active.revision && t.view.owner==active.view.owner &&
        t.view.registry_namespace==active.view.registry_namespace) {active={}; ++cancels;}
  }
  void set(const StableLexemeKey& key,std::uint32_t value) {values[key]=value; ++current.count_revision;}
  void reopen() {current.owner=std::make_shared<CountOwnerIdentity>(); ++current.availability_revision;}
  void recover() {unavailable=false; fail_on_begin=0; ++current.availability_revision;}
  void same_value_flush() {} // No semantic version change.
  std::map<StableLexemeKey,std::uint32_t> values;
  StateView current;
  CountTicket active, old;
  StableLexemeKey pending_key;
  std::size_t begins{0}, advances{0}, delay{0}, remaining{0}, fail_on_begin{0}, cancels{0};
  mutable std::size_t view_calls{0},view_checks{0};
  bool unavailable{false}, inject_old{false};
 private:
  std::uint64_t serial{0};
};
} // namespace hanime::test
