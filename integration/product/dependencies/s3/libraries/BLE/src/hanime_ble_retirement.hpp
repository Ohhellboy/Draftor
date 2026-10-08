#pragma once
#include <cstdint>
namespace hanime_ble {
// One client, called only on the NimBLE host queue. No allocation, task or timer.
// A successful terminate/cancel request is NOT proof that callbacks have ended.
class Retirement {
 public:
  template<class Stop, class Reset>
  bool advance(uint32_t now, bool idle, Stop stop, Reset reset) {
    if (idle) return true;
    if (!started_) { started_=true; start_=now; }
    if (!resetSent_ && uint32_t(now-start_)>=10000) {
      resetSent_=true; reset(); // existing NimBLE stalled-controller recovery
    }
    if (requested_ && uint32_t(now-last_)<1000) return false;
    requested_=true; last_=now;
    const bool accepted=stop();
    failures_=accepted ? 0 : (failures_<3 ? failures_+1 : 3);
    if (!resetSent_ && failures_>=3) { resetSent_=true; reset(); }
    return false; // next host-queue turn must observe the delivered terminal event
  }
 private:
  uint32_t start_=0,last_=0,failures_=0;
  bool started_=false,requested_=false,resetSent_=false;
};
} // namespace hanime_ble
