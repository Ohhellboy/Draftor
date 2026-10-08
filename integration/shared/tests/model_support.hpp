#pragma once
#include "check.hpp"
#include "draftor_exchange/export_session.hpp"
#include <array>
#include <algorithm>
#include <limits>
namespace dx = draftor::exchange;

struct TrackingAllocator : dx::Allocator {
  std::size_t live = 0, bytes = 0, peak_live = 0, peak_bytes = 0, largest = 0, calls = 0, frees = 0;
  int fail_after = -1;
  char* pointer = nullptr;
  char* allocate(std::size_t n) noexcept override {
    ++calls;
    if (fail_after == 0) return nullptr;
    if (fail_after > 0) --fail_after;
    CHECK(n && !live);
    auto* raw = static_cast<char*>(std::malloc(n + 16)); CHECK(raw);
    std::memset(raw, 0x6a, n + 16);
    pointer = raw + 8; live = 1; bytes = n;
    peak_live = std::max(peak_live, live); peak_bytes = std::max(peak_bytes, n);
    largest = std::max(largest, n);
    return pointer;
  }
  void deallocate(char* p, std::size_t n) noexcept override {
    CHECK(live == 1 && p == pointer && n == bytes);
    for (int i = 0; i < 8; ++i) CHECK(static_cast<unsigned char>(p[i-8]) == 0x6a && static_cast<unsigned char>(p[n+i]) == 0x6a);
    std::memset(p, 0xa5, n); std::free(p - 8);
    pointer = nullptr; live = bytes = 0; ++frees;
  }
  ~TrackingAllocator() override { CHECK(!live); }
};
struct ModelOwner : dx::OwnerPort {
  dx::OwnerState state{};
  std::string body;
  bool frozen = false, allow_freeze = true, read_fails = false, mutate_in_borrow = false;
  bool poison = true, bad_borrow = false;
  std::size_t borrows = 0, freeze_calls = 0, unfreeze_calls = 0, starts = 0, services = 0;
  unsigned remaining = 0;
  std::uint32_t saved_revision = 0;
  dx::SaveState next_save = dx::SaveState::Complete;
  bool partial_body_committed = true;
  std::array<char, 1024> scratch{};
  explicit ModelOwner(std::string text = "abc") : body(std::move(text)) {
    state.has_current = true;
    state.identity.collection[0] = 1; state.identity.document[0] = 2;
    state.identity.mount = state.identity.document_session = state.identity.revision_epoch = 1;
    state.identity.body_revision = 1;
  }
  void edit(std::string text) {
    CHECK(!frozen); body = std::move(text); ++state.identity.body_revision; state.body_dirty = true;
  }
  dx::OwnerState probe() noexcept override { state.body_bytes = body.size(); return state; }
  bool freeze() noexcept override {
    ++freeze_calls; if (!allow_freeze) return false; CHECK(!frozen); frozen = true; return true;
  }
  void unfreeze() noexcept override { CHECK(frozen); frozen = false; ++unfreeze_calls; }
  dx::SaveReply beginAutoSave() noexcept override {
    ++starts; CHECK(!state.save_busy);
    state.save_busy = true; remaining = 5; saved_revision = state.identity.body_revision;
    return {dx::SaveState::Pending, 17, false, false};
  }
  dx::SaveReply serviceSave() noexcept override {
    ++services; CHECK(state.save_busy);
    if (--remaining) return {dx::SaveState::Pending, 17, false, false};
    state.save_busy = false;
    const bool committed = next_save == dx::SaveState::Complete ||
        (next_save == dx::SaveState::Partial && partial_body_committed);
    if (committed && saved_revision == state.identity.body_revision) state.body_dirty = false;
    if (next_save == dx::SaveState::Complete) state.position_dirty = false;
    return {next_save, next_save == dx::SaveState::Complete ? 0 : 2, committed,
        next_save == dx::SaveState::Complete};
  }
  dx::BorrowReply borrow(std::size_t offset, std::size_t max, void* context,
                         dx::BorrowConsumer consumer) noexcept override {
    CHECK(frozen); ++borrows;
    if (read_fails) return {};
    const auto n = std::min(max, body.size() - offset);
    CHECK(n <= scratch.size());
    std::copy_n(body.data() + offset, n, scratch.data());
    consumer(context, poison ? std::string_view(scratch.data(), n) : std::string_view(body).substr(offset,n));
    std::fill(scratch.begin(), scratch.end(), static_cast<char>(0xa5));
    if (mutate_in_borrow) ++state.identity.body_revision;
    return {true, bad_borrow ? n+1 : n};
  }
};
inline void ready(dx::ExportSession& s, std::size_t bound = 10000) {
  while (bound-- && (s.phase() == dx::Phase::Saving || s.phase() == dx::Phase::Copying)) s.tick();
  CHECK(s.phase() == dx::Phase::Ready);
}
inline std::string leaseBytes(const dx::Lease& l) {
  return l.size ? std::string(l.data, l.size) : std::string{};
}

