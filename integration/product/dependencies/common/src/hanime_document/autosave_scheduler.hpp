#pragma once

#include <cstdint>

namespace hanime::documents {

struct AutosaveConfig {
  std::uint32_t idle_ms = 5000U;
  std::uint32_t hard_deadline_ms = 60000U;
  std::uint32_t retry_ms = 5000U;
};

enum class AutosaveTrigger : std::uint8_t { None, Idle, HardDeadline };

// Behavior-preserving reuse of Integration v0.9 autosave_scheduler. The
// document service adds identity/revision checks around this pure policy.
class AutosaveScheduler {
 public:
  explicit AutosaveScheduler(AutosaveConfig config = {}) noexcept : config_(config) {}
  void onUserActivity(std::uint32_t now_ms) noexcept;
  void onDocumentMutation(std::uint32_t now_ms) noexcept;
  AutosaveTrigger due(std::uint32_t now_ms, bool can_save) const noexcept;
  void onSaveResult(std::uint32_t now_ms, bool success,
                    bool newer_dirty = false,
                    std::uint32_t newer_first_dirty_ms = 0U) noexcept;
  void clearPending() noexcept;
  bool pending() const noexcept { return pending_; }
  std::uint32_t firstDirtyMs() const noexcept { return first_dirty_ms_; }
 private:
  static std::uint32_t elapsed(std::uint32_t now, std::uint32_t then) noexcept { return now - then; }
  AutosaveConfig config_{};
  bool pending_ = false;
  bool retry_gate_active_ = false;
  std::uint32_t first_dirty_ms_ = 0;
  std::uint32_t last_activity_ms_ = 0;
  std::uint32_t last_attempt_ms_ = 0;
};

}  // namespace hanime::documents
