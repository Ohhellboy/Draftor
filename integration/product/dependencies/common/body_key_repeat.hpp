#pragma once

#include "input_router.hpp"

#include <cstdint>

namespace hanime::body_extension {

enum class RepeatKey : std::uint8_t { None = 0, Left, Right, Up, Down, Backspace, DeleteForward };

enum class RepeatCancelReason : std::uint8_t {
  None = 0,
  Released,
  DirectionChanged,
  Disconnected,
  FocusChanged,
  DocumentChanged,
  CompositionStarted,
  BoundaryReached,
  Explicit,
};

struct AxisRepeatConfig {
  std::uint32_t initial_delay_ms = 350U;
  std::uint32_t initial_interval_ms = 130U;
  std::uint32_t minimum_interval_ms = 38U;
  // Acceleration is measured from the physical press. With the defaults the
  // horizontal axis reaches its cap at 2000 ms (350 + 1650).
  std::uint32_t acceleration_ms = 1650U;
};

struct BodyKeyRepeatConfig {
  AxisRepeatConfig horizontal{};
  AxisRepeatConfig vertical{350U, 170U, 62U, 1650U};
  AxisRepeatConfig deletion{350U, 100U, 60U, 800U};
};

struct RepeatContext {
  bool connected = true;
  bool body_focused = true;
  bool composition_active = false;
  std::uint32_t document_session = 0U;
};

struct RepeatEmission {
  editor::input::NormalizedKey key{};
  std::uint32_t held_ms = 0U;
  std::uint32_t next_interval_ms = 0U;
  bool emitted = false;
};

// Device-owned arrow repeat. Physical down/up events are the only arming
// source. Duplicate downs from a host repeat stream are consumed, one service
// call emits at most one action, and missed deadlines are never replayed.
class BodyKeyRepeater {
 public:
  explicit BodyKeyRepeater(BodyKeyRepeatConfig config = {}) noexcept;

  // Returns true only for a new physical direction and therefore tells the
  // caller to execute the tap immediately. Duplicate down reports return
  // false, preventing a second repeat source.
  bool keyDown(RepeatKey key, std::uint32_t now_ms,
               std::uint32_t document_session) noexcept;
  void keyUp(RepeatKey key) noexcept;
  void cancel(RepeatCancelReason reason = RepeatCancelReason::Explicit) noexcept;

  RepeatEmission service(std::uint32_t now_ms,
                         std::uint8_t current_modifiers,
                         const RepeatContext& context) noexcept;

  // Latch a document boundary after a repeated action reports NoAction. This
  // avoids repeated layout scans and damage requests until a new physical
  // press or direction change.
  void noteActionResult(bool changed, bool at_boundary) noexcept;

  bool active() const noexcept { return active_; }
  bool boundaryLatched() const noexcept { return boundary_latched_; }
  RepeatKey key() const noexcept { return key_; }
  RepeatCancelReason lastCancelReason() const noexcept { return last_cancel_; }
  std::uint32_t intervalAt(std::uint32_t held_ms) const noexcept;

  static RepeatKey fromKeyKind(editor::input::KeyKind key) noexcept;
  static editor::input::KeyKind toKeyKind(RepeatKey key) noexcept;

 private:
  const AxisRepeatConfig& axisConfig(RepeatKey key) const noexcept;
  static bool due(std::uint32_t now, std::uint32_t deadline) noexcept;

  BodyKeyRepeatConfig config_{};
  RepeatKey key_ = RepeatKey::None;
  RepeatCancelReason last_cancel_ = RepeatCancelReason::None;
  std::uint32_t pressed_ms_ = 0U;
  std::uint32_t next_due_ms_ = 0U;
  std::uint32_t document_session_ = 0U;
  bool active_ = false;
  bool boundary_latched_ = false;
};

}  // namespace hanime::body_extension
