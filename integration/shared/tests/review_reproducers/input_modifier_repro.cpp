// Review-only repro for the submitted InputHarness model, not a BLE/device test.
#include "input_harness.hpp"
#include <cstdio>
int main() {
  ModelOwner owner("text"); TrackingAllocator allocator;
  dx::ExportSession session(owner,allocator,{65536}); InputHarness input;
  const auto old_route = input.route;
  input.event({4,0,1,old_route,10,true}); // Old A down, before entering modal.
  CHECK(input.enter(session)==dx::SessionError::None);
  session.cancel(); input.exit(session); input.presented(input.route);
  input.event({225,2,1,input.route,20,true}); // New Shift down in current route.
  CHECK(input.modifiers==2 && input.held[225].press==20);
  input.event({4,0,1,old_route,10,false}); // Matching old A Up arrives late.
  std::printf("old_A_released=%d new_Shift_still_held=%d modifiers_after_late_old_Up=%u\n",
      input.held[4].press==0, input.held[225].press==20,input.modifiers);
  const bool correct = input.held[4].press==0 && input.held[225].press==20 && input.modifiers==2;
  std::printf("expected=old_A_released_and_new_Shift_remains_active result=%s\n", correct ? "PASS" : "FAIL");
  return correct ? 0 : 1;
}
