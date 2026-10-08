#include "autosave_scheduler.hpp"

namespace hanime::documents {

void AutosaveScheduler::onUserActivity(const std::uint32_t now_ms) noexcept {
  if (pending_) last_activity_ms_ = now_ms;
}
void AutosaveScheduler::onDocumentMutation(const std::uint32_t now_ms) noexcept {
  if (!pending_) { pending_ = true; first_dirty_ms_ = now_ms; retry_gate_active_ = false; }
  last_activity_ms_ = now_ms;
}
AutosaveTrigger AutosaveScheduler::due(const std::uint32_t now_ms, const bool can_save) const noexcept {
  if (!pending_ || !can_save) return AutosaveTrigger::None;
  if (retry_gate_active_ && elapsed(now_ms, last_attempt_ms_) < config_.retry_ms) return AutosaveTrigger::None;
  if (elapsed(now_ms, first_dirty_ms_) >= config_.hard_deadline_ms) return AutosaveTrigger::HardDeadline;
  if (elapsed(now_ms, last_activity_ms_) >= config_.idle_ms) return AutosaveTrigger::Idle;
  return AutosaveTrigger::None;
}
void AutosaveScheduler::onSaveResult(const std::uint32_t now_ms,
                                     const bool success,
                                     const bool newer_dirty,
                                     const std::uint32_t newer_first_dirty_ms) noexcept {
  if (success && newer_dirty) {
    pending_ = true;
    retry_gate_active_ = false;
    first_dirty_ms_ = newer_first_dirty_ms;
    last_attempt_ms_ = 0U;
    return;
  }
  if (success) { clearPending(); return; }
  last_attempt_ms_ = now_ms; retry_gate_active_ = true;
}
void AutosaveScheduler::clearPending() noexcept {
  pending_ = false; retry_gate_active_ = false; first_dirty_ms_ = 0; last_activity_ms_ = 0; last_attempt_ms_ = 0;
}

}  // namespace hanime::documents
