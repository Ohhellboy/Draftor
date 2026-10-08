#pragma once
#include "model_support.hpp"
// Test-only owner-side input truth. No code here is part of the shared library.
// press_id is the harness's causal press/Up pairing, NOT an actual HID field.
struct Key {
  // modifiers is the event's report snapshot, not authority to replace the
  // current aggregate. The test owner emits explicit modifier Down/Up too.
  unsigned usage = 4, modifiers = 0;
  std::uint64_t physical_epoch = 1, route = 1, press_id = 1;
  bool down = true, repeat = false, cancel = false, forbidden_action = false;
};
struct InputHarness {
  struct Held { std::uint64_t press = 0, epoch = 0; bool suppressed = false; };
  std::array<Held,256> held{};
  std::array<Key,64> queue{};
  std::size_t head = 0, size = 0, discarded = 0, applied = 0, release_cleanup = 0, cancels = 0;
  std::uint64_t epoch = 1, route = 1;
  unsigned modifiers = 0;
  bool modal = false, awaiting_frame = false, cancel_control_pending = false;
  int layout = 1, cursor = 7, selection = 2, history = 3, viewport = 9;
  // Model mapping: usages 224..231 own bits 0..7. The existing held table is
  // the sole physical truth; no new per-event freshness field or second table.
  static bool isModifier(unsigned usage) noexcept { return usage >= 224 && usage <= 231; }
  unsigned heldModifiers() const noexcept {
    unsigned result = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
      const auto& h = held[224 + bit];
      if (h.press && h.epoch == epoch) result |= 1U << bit;
    }
    return result;
  }
  void push(Key k) { CHECK(size < queue.size()); queue[(head + size++) % queue.size()] = k; }
  void event(Key k, dx::ExportSession* session = nullptr) {
    CHECK(k.usage < held.size());
    if (k.physical_epoch != epoch) { ++discarded; return; }
    auto& h = held[k.usage];
    // An old route may still carry the matching physical release.
    // An old physical epoch or old press ID cannot release a replacement press.
    if (!k.down) {
      if (h.press && h.press == k.press_id && h.epoch == k.physical_epoch) {
        h = {}; ++release_cleanup;
        modifiers = heldModifiers();
      }
      return;
    }
    if (k.route != route) { ++discarded; return; }
    const bool new_press = !h.press && !k.repeat;
    if ((!h.press && k.repeat) || (h.press && h.press != k.press_id)) {
      ++discarded; return;
    }
    if (new_press) h = {k.press_id,k.physical_epoch,modal || awaiting_frame};
    modifiers = heldModifiers();
    if (modal || awaiting_frame || h.suppressed) {
      ++discarded;
      if (modal && k.cancel && new_press && session) { ++cancels; session->cancel(); }
      return;
    }
    if (isModifier(k.usage)) return; // physical state, never a business action
    ++applied; // Abstract owner action count, no fake product-router PASS.
  }
  dx::SessionError enter(dx::ExportSession& session) {
    // The entry event follows all queued editor events in this harness.
    while (size) { auto k=queue[head];head=(head+1)%queue.size();--size;event(k); }
    const auto result = session.begin();
    if (result != dx::SessionError::None) return result;
    modal=true; awaiting_frame=false; ++route;
    for (auto& h:held) if(h.press) h.suppressed=true;
    return result;
  }
  void iteration(dx::ExportSession& session) {
    if (cancel_control_pending) {
      cancel_control_pending=false;
      if(modal){++cancels;session.cancel();}
    }
    for (unsigned i=0;i<8 && size;++i) { auto k=queue[head];head=(head+1)%queue.size();--size;event(k,&session); }
    session.tick(); // Always advances despite further ordinary HID in the queue.
  }
  // A fixed one-bit control mailbox does not depend on text/reader/ordinary-key
  // queue capacity. It models a control channel, not a product HID API.
  void requestCancel() noexcept { cancel_control_pending=true; }
  void exit(dx::ExportSession& session) {
    CHECK(session.phase()==dx::Phase::Closed);
    modal=false;awaiting_frame=true;++route;
    for(auto& h:held) if(h.press)h.suppressed=true;
  }
  void presented(std::uint64_t frame_route) { if(frame_route==route)awaiting_frame=false; }
};
