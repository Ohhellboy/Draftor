#pragma once
#include "input_harness.hpp"

inline bool sameLeaseFields(const dx::Lease& a, const dx::Lease& b) {
  return a.data == b.data && a.size == b.size && a.token == b.token &&
      a.identity == b.identity && a.source_bytes == b.source_bytes;
}

inline dx::Lease sentinelLease(const char* data) {
  dx::Lease result{data, 3, {91, 92}, {}, 93};
  result.identity.collection.fill(0xa5);
  result.identity.document.fill(0x5a);
  result.identity.mount = 94;
  result.identity.document_session = 95;
  result.identity.revision_epoch = 96;
  result.identity.body_revision = 97;
  return result;
}

inline void leaseOutputRegressions() {
  for (bool closing : {false, true}) {
    ModelOwner owner(bom + "review\r\n" + semantic);
    TrackingAllocator allocator;
    dx::ExportSession session(owner, allocator, {65536, 7});
    CHECK(session.begin() == dx::SessionError::None);
    ready(session);
    dx::Lease lease;
    CHECK(session.acquire(lease) == dx::ReaderStatus::Ok);
    const auto expected_fields = lease; // Comparison oracle, NEVER a release credential.
    const auto expected_bytes = bom + "review\n" + semantic;
    CHECK(bytesEqual(leaseBytes(lease), expected_bytes));
    if (closing) session.cancel();
    CHECK(session.acquire(lease) == (closing ? dx::ReaderStatus::NotReady : dx::ReaderStatus::Busy));
    CHECK(sameLeaseFields(lease, expected_fields));
    CHECK(bytesEqual(leaseBytes(lease), expected_bytes));
    CHECK(allocator.calls == 1 && allocator.peak_live == 1);
    session.cancel();
    CHECK(session.phase() == dx::Phase::Closing && owner.frozen && allocator.live == 1);
    // The exact output object passed to the failed acquire supplies release.
    CHECK(session.release(lease.token) == dx::ReaderStatus::Ok);
    CHECK(session.phase() == dx::Phase::Closed && !owner.frozen);
    CHECK(!allocator.live && !allocator.bytes && allocator.calls == allocator.frees);
    CHECK(session.release(lease.token) == dx::ReaderStatus::Stale);
    std::printf("R1_A same_lease_%s all_fields_preserved=1 current_lease_release=Ok Closed=1 live=0\n",
                closing ? "Closing" : "Ready");
  }
  {
    ModelOwner owner; TrackingAllocator allocator;
    dx::ExportSession session(owner, allocator, {65536});
    const char payload[] = "out";
    dx::Lease other = sentinelLease(payload);
    const auto expected_other = other;
    CHECK(session.acquire(other) == dx::ReaderStatus::NotReady); // Idle
    CHECK(sameLeaseFields(other, expected_other));
    session.begin();
    CHECK(session.acquire(other) == dx::ReaderStatus::NotReady); // Saving
    CHECK(sameLeaseFields(other, expected_other));
    CHECK(session.tick() == dx::Phase::Copying);
    CHECK(session.acquire(other) == dx::ReaderStatus::NotReady);
    CHECK(sameLeaseFields(other, expected_other));
    ready(session);
    dx::Lease lease;
    CHECK(session.acquire(lease) == dx::ReaderStatus::Ok);
    CHECK(session.acquire(other) == dx::ReaderStatus::Busy);
    CHECK(sameLeaseFields(other, expected_other) && allocator.calls == 1);
    const auto previous = lease.token;
    const auto* data = lease.data;
    CHECK(session.release(lease.token) == dx::ReaderStatus::Ok);
    CHECK(session.acquire(lease) == dx::ReaderStatus::Ok); // same variable, sequential success
    CHECK(lease.data == data && lease.token.session == previous.session);
    CHECK(lease.token.generation == previous.generation + 1 && allocator.calls == 1);
    CHECK(session.release(previous) == dx::ReaderStatus::Stale && session.readerActive());
    session.cancel();
    CHECK(session.release(lease.token) == dx::ReaderStatus::Ok);
    CHECK(session.acquire(other) == dx::ReaderStatus::NotReady); // Closed
    CHECK(sameLeaseFields(other, expected_other) && !allocator.live && !allocator.bytes);
  }
  for (bool uncertain : {false, true}) {
    ModelOwner owner; TrackingAllocator allocator;
    dx::ExportSession session(owner, allocator, {65536});
    session.begin(); ready(session);
    dx::Lease lease;
    CHECK(session.acquire(lease) == dx::ReaderStatus::Ok);
    const auto expected = lease;
    if (uncertain) owner.state.revision_reliable = false;
    else ++owner.state.identity.document[0];
    CHECK(session.acquire(lease) == dx::ReaderStatus::NotReady);
    CHECK(sameLeaseFields(lease, expected) && leaseBytes(lease) == "abc");
    CHECK(session.error() == (uncertain ? dx::SessionError::RevisionUncertain : dx::SessionError::IdentityChanged));
    CHECK(session.phase() == dx::Phase::Closing && allocator.live == 1 && owner.frozen);
    CHECK(session.release(lease.token) == dx::ReaderStatus::Ok);
    CHECK(session.phase() == dx::Phase::Closed && !allocator.live && !allocator.bytes && !owner.frozen);
  }
  {
    ModelOwner owner; TrackingAllocator allocator;
    dx::ExportSession session(owner, allocator, {65536, 256, 0, UINT64_MAX});
    const char payload[] = "out";
    dx::Lease out = sentinelLease(payload);
    const auto expected = out;
    session.begin(); ready(session);
    CHECK(session.acquire(out) == dx::ReaderStatus::Exhausted);
    CHECK(sameLeaseFields(out, expected));
    CHECK(session.error() == dx::SessionError::GenerationExhausted);
    CHECK(session.phase() == dx::Phase::Closed && !allocator.live && !allocator.bytes && !owner.frozen);
  }
  std::puts("R1_A failure_preserves_sentinel=1 sequential_generation=1 invalid_identity_retires=1 exhaustion_retires=1");
}

inline void modifierOwnershipRegressions() {
  // The independent input_modifier_repro covers old A Up after new Shift Down.
  // Cover the same stale snapshot within one route too: route alone is not freshness.
  {
    InputHarness h;
    h.event({4, 0, 1, h.route, 10, true});
    h.event({225, 2, 1, h.route, 20, true});
    h.event({4, 0, 1, h.route, 10, false});
    CHECK(!h.held[4].press && h.held[225].press == 20 && h.modifiers == 2);
    h.event({225, 0, 1, h.route, 20, false});
    CHECK(!h.modifiers);
  }
  // A pre-gate Shift release owns only Shift, even if its report says all up.
  for (unsigned newer_usage : {224U, 229U}) { // Control or the other Shift
    ModelOwner owner; TrackingAllocator allocator;
    dx::ExportSession session(owner, allocator, {65536});
    InputHarness h;
    const auto old_route = h.route;
    h.event({225, 2, 1, old_route, 10, true});
    CHECK(h.modifiers == 2);
    CHECK(h.enter(session) == dx::SessionError::None && h.held[225].suppressed);
    session.cancel(); h.exit(session); h.presented(h.route);
    const unsigned newer_bit = 1U << (newer_usage - 224);
    h.event({newer_usage, 2 | newer_bit, 1, h.route, 20, true});
    CHECK(h.modifiers == (2 | newer_bit));
    h.event({225, 0, 1, old_route, 10, false});
    CHECK(!h.held[225].press && h.held[newer_usage].press == 20 && h.modifiers == newer_bit);
    // A wrong physical epoch cannot change held or the aggregate.
    h.event({newer_usage, 0, 0, h.route, 20, false});
    CHECK(h.held[newer_usage].press == 20 && h.modifiers == newer_bit);
    h.event({newer_usage, 0, 1, h.route, 20, false});
    CHECK(!h.held[newer_usage].press && h.modifiers == 0);
    h.event({newer_usage, newer_bit, 1, h.route, 30, true});
    h.event({newer_usage, 0, 1, old_route, 20, false}); // same usage, previous press
    CHECK(h.held[newer_usage].press == 30 && h.modifiers == newer_bit);
    h.event({newer_usage, 0, 1, h.route, 20, true, true}); // previous press repeat
    h.event({newer_usage, 0, 1, old_route, 30, true, true}); // old route repeat
    CHECK(h.held[newer_usage].press == 30 && h.modifiers == newer_bit);
    h.event({newer_usage, 0, 1, h.route, 30, false});
    CHECK(!h.modifiers && !allocator.live && !allocator.bytes);
  }
  std::puts("R1_B matched_old_Up_releases_only_own_press=1 newer_modifiers_preserved=1 stale_repeat_epoch_ignored=1 model_only=1");
  std::printf("MODEL_RESOURCE {\"InputHarness\":%zu,\"Held\":%zu,\"Key\":%zu,\"new_heap_allocations\":0}\n",
              sizeof(InputHarness), sizeof(InputHarness::Held), sizeof(Key));
}
