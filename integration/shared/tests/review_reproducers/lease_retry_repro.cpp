// Review-only reproducer. Includes the submitted code; changes no submitted file.
#include "model_support.hpp"
#include <cstdio>

static bool run_case(bool retry_while_closing) {
  ModelOwner owner("review-current-document");
  TrackingAllocator allocator;
  dx::ExportSession session(owner, allocator, {65536});
  CHECK(session.begin() == dx::SessionError::None);
  ready(session);
  dx::Lease lease;
  CHECK(session.acquire(lease) == dx::ReaderStatus::Ok);
  const auto recovery_token = lease.token; // Solely to clean up this reproducer safely.
  const auto* original_data = lease.data;
  if (retry_while_closing) session.cancel();
  const auto retry = session.acquire(lease); // Re-use the same output object.
  const bool output_erased = lease.data == nullptr && lease.size == 0 &&
      lease.token.session == 0 && lease.token.generation == 0;
  session.cancel();
  const auto usual_release = session.release(lease.token);
  const bool stranded = session.phase() == dx::Phase::Closing &&
      !session.canDestroy() && allocator.live == 1 && owner.frozen;
  std::printf("case=%s retry=%s original_data_nonnull=%d output_erased=%d "
              "release_via_current_lease=%s closing_with_live_allocation=%d\n",
      retry_while_closing ? "retry_while_closing" : "retry_while_ready",
      retry == dx::ReaderStatus::Busy ? "Busy" :
      retry == dx::ReaderStatus::NotReady ? "NotReady" : "other",
      original_data != nullptr, output_erased,
      usual_release == dx::ReaderStatus::Stale ? "Stale" : "other", stranded);
  const bool correct = !output_erased && usual_release == dx::ReaderStatus::Ok &&
      session.phase() == dx::Phase::Closed && allocator.live == 0 && !owner.frozen;
  if (session.readerActive()) {
    CHECK(session.release(recovery_token) == dx::ReaderStatus::Ok);
  }
  CHECK(session.phase() == dx::Phase::Closed && allocator.live == 0 && !owner.frozen);
  std::printf("expected=failed_acquire_preserves_existing_lease_and_normal_release_closes result=%s\n",
      correct ? "PASS" : "FAIL");
  std::puts("cleanup=Closed; no crash induced");
  return correct;
}
int main() {
  const bool ready_case = run_case(false);
  const bool closing_case = run_case(true);
  return ready_case && closing_case ? 0 : 1;
}
