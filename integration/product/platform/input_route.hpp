#pragma once
#include "hid_keyboard.hpp"
#include <algorithm>
namespace s3hid {
// A notification belongs to the last fully presented UI context. Main loop
// publishes a new epoch only after the destination frame has been drawn.
// Queued keys from the old context are counted and discarded, never replayed.
class InputRoute {
 uint32_t epoch_=1;
 uint64_t focus_=0;
 public:
 uint32_t discarded=0, max_discarded_age_ms=0;
 uint32_t epoch()const noexcept{return epoch_;}
 bool needsFrame(uint64_t focus)const noexcept{return focus!=focus_;}
 void framePresented(uint64_t focus)noexcept{
  if(focus==focus_)return;
  focus_=focus;if(++epoch_==0)++epoch_;
 }
 bool accept(const Event& event,uint32_t now)noexcept{
  if(event.route_epoch==epoch_)return true;
  ++discarded;max_discarded_age_ms=std::max(max_discarded_age_ms,now-event.ms);return false;
 }
};
}
