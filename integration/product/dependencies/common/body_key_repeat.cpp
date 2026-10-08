#include "body_key_repeat.hpp"

#include <algorithm>

namespace hanime::body_extension {

namespace {

AxisRepeatConfig sanitize(AxisRepeatConfig value) noexcept {
  if (value.initial_delay_ms == 0U) value.initial_delay_ms = 1U;
  if (value.minimum_interval_ms == 0U) value.minimum_interval_ms = 1U;
  if (value.initial_interval_ms < value.minimum_interval_ms) {
    value.initial_interval_ms = value.minimum_interval_ms;
  }
  return value;
}

}  // namespace

BodyKeyRepeater::BodyKeyRepeater(BodyKeyRepeatConfig config) noexcept
    : config_(config) {
  config_.horizontal = sanitize(config_.horizontal);
  config_.vertical = sanitize(config_.vertical);
  config_.deletion = sanitize(config_.deletion);
}

RepeatKey BodyKeyRepeater::fromKeyKind(
    const editor::input::KeyKind key) noexcept {
  using editor::input::KeyKind;
  switch (key) {
    case KeyKind::Left: return RepeatKey::Left;
    case KeyKind::Right: return RepeatKey::Right;
    case KeyKind::Up: return RepeatKey::Up;
    case KeyKind::Down: return RepeatKey::Down;
    case KeyKind::Backspace: return RepeatKey::Backspace;
    case KeyKind::DeleteForward: return RepeatKey::DeleteForward;
    default: return RepeatKey::None;
  }
}

editor::input::KeyKind BodyKeyRepeater::toKeyKind(
    const RepeatKey key) noexcept {
  using editor::input::KeyKind;
  switch (key) {
    case RepeatKey::Left: return KeyKind::Left;
    case RepeatKey::Right: return KeyKind::Right;
    case RepeatKey::Up: return KeyKind::Up;
    case RepeatKey::Down: return KeyKind::Down;
    case RepeatKey::Backspace: return KeyKind::Backspace;
    case RepeatKey::DeleteForward: return KeyKind::DeleteForward;
    case RepeatKey::None: return KeyKind::Character;
  }
  return KeyKind::Character;
}

const AxisRepeatConfig& BodyKeyRepeater::axisConfig(
    const RepeatKey key) const noexcept {
  if(key==RepeatKey::Backspace||key==RepeatKey::DeleteForward)return config_.deletion;
  return key == RepeatKey::Left || key == RepeatKey::Right
      ? config_.horizontal : config_.vertical;
}

bool BodyKeyRepeater::due(const std::uint32_t now,
                          const std::uint32_t deadline) noexcept {
  return static_cast<std::int32_t>(now - deadline) >= 0;
}

bool BodyKeyRepeater::keyDown(const RepeatKey key,
                              const std::uint32_t now_ms,
                              const std::uint32_t document_session) noexcept {
  if (key == RepeatKey::None) return false;
  if (active_ && key_ == key) return false;
  if (active_) cancel(RepeatCancelReason::DirectionChanged);
  key_ = key;
  pressed_ms_ = now_ms;
  next_due_ms_ = now_ms + axisConfig(key).initial_delay_ms;
  document_session_ = document_session;
  active_ = true;
  boundary_latched_ = false;
  last_cancel_ = RepeatCancelReason::None;
  return true;
}

void BodyKeyRepeater::keyUp(const RepeatKey key) noexcept {
  if (active_ && key_ == key) cancel(RepeatCancelReason::Released);
}

void BodyKeyRepeater::cancel(const RepeatCancelReason reason) noexcept {
  active_ = false;
  boundary_latched_ = false;
  key_ = RepeatKey::None;
  last_cancel_ = reason;
}

std::uint32_t BodyKeyRepeater::intervalAt(
    const std::uint32_t held_ms) const noexcept {
  const AxisRepeatConfig& axis = axisConfig(key_);
  if (held_ms <= axis.initial_delay_ms || axis.acceleration_ms == 0U) {
    return held_ms <= axis.initial_delay_ms
        ? axis.initial_interval_ms : axis.minimum_interval_ms;
  }
  const std::uint32_t accelerating = std::min(
      held_ms - axis.initial_delay_ms, axis.acceleration_ms);
  const std::uint32_t span =
      axis.initial_interval_ms - axis.minimum_interval_ms;
  const std::uint64_t reduction =
      static_cast<std::uint64_t>(span) * accelerating /
      axis.acceleration_ms;
  return axis.initial_interval_ms - static_cast<std::uint32_t>(reduction);
}

RepeatEmission BodyKeyRepeater::service(
    const std::uint32_t now_ms,
    const std::uint8_t current_modifiers,
    const RepeatContext& context) noexcept {
  if (!active_) return {};
  if (!context.connected) {
    cancel(RepeatCancelReason::Disconnected);
    return {};
  }
  if (!context.body_focused) {
    cancel(RepeatCancelReason::FocusChanged);
    return {};
  }
  if (context.document_session != document_session_) {
    cancel(RepeatCancelReason::DocumentChanged);
    return {};
  }
  if (context.composition_active) {
    cancel(RepeatCancelReason::CompositionStarted);
    return {};
  }
  if (boundary_latched_ || !due(now_ms, next_due_ms_)) return {};

  const std::uint32_t held = now_ms - pressed_ms_;
  const std::uint32_t interval = intervalAt(held);
  // No catch-up backlog: the next deadline is based on the service time, not
  // on a possibly long-expired deadline.
  next_due_ms_ = now_ms + interval;
  return {editor::input::NormalizedKey::special(
              toKeyKind(key_), current_modifiers),
          held, interval, true};
}

void BodyKeyRepeater::noteActionResult(const bool changed,
                                       const bool at_boundary) noexcept {
  if (!active_) return;
  if (!changed && at_boundary) {
    boundary_latched_ = true;
    last_cancel_ = RepeatCancelReason::BoundaryReached;
  }
}

}  // namespace hanime::body_extension
