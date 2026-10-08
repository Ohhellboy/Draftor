#pragma once

#include "find_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hanime::find {

struct FindConfig {
  std::size_t query_capacity_bytes = kHardMaximumQueryBytes;
  std::size_t default_step_work = 256U;
  bool default_case_sensitive = false;
};

enum class QueryEditResult : std::uint8_t {
  Applied,
  NoChange,
  InvalidUtf8,
  CapacityExceeded,
  NotOpen,
};

enum class StepResult : std::uint8_t {
  Idle,
  MoreWork,
  Found,
  NotFound,
  Stale,
  AdapterFailure,
};

class FindSession {
 public:
  explicit FindSession(FindConfig config = {}) noexcept;

  bool open(DocumentPort& port, bool body_composition_active) noexcept;
  bool close(DocumentPort& port, bool find_composition_active) noexcept;
  void cancel() noexcept;
  void documentSwitched(DocumentPort& port) noexcept;

  QueryEditResult setQuery(std::string_view utf8) noexcept;
  QueryEditResult insertQueryUtf8(std::string_view utf8) noexcept;
  QueryEditResult eraseQueryBackward() noexcept;
  QueryEditResult eraseQueryForward() noexcept;
  bool moveQueryCursorLeft() noexcept;
  bool moveQueryCursorRight() noexcept;
  void moveQueryCursorHome() noexcept { query_cursor_ = 0U; }
  void moveQueryCursorEnd() noexcept { query_cursor_ = query_size_; }
  void setCaseSensitive(bool enabled) noexcept;
  void toggleCaseSensitive() noexcept { setCaseSensitive(!case_sensitive_); }

  bool canInsertQuery(std::string_view utf8) const noexcept;
  void start(Direction direction) noexcept;
  StepResult step(DocumentPort& port, std::size_t work_budget = 0U) noexcept;

  ViewModel view() const noexcept;
  MemoryBudget memoryBudget(std::size_t ime_staging_bytes = 0U) const noexcept;
  std::uint32_t sessionId() const noexcept { return session_id_; }
  const FindConfig& config() const noexcept { return config_; }
  std::size_t lastStepWork() const noexcept { return last_step_work_; }
  std::size_t maximumStepWork() const noexcept { return maximum_step_work_; }

 private:
  struct ScanTask {
    SourceStamp stamp{};
    std::uint32_t session_id = 0;
    std::uint32_t query_version = 0;
    Direction direction = Direction::Forward;
    std::size_t first_lo = 0;
    std::size_t first_hi = 0;
    std::size_t second_lo = 0;
    std::size_t second_hi = 0;
    std::size_t position = 0;
    std::size_t compare_offset = 0;
    bool second_segment = false;
    bool candidate_active = false;
    bool active = false;
  };

  static bool validUtf8(std::string_view text) noexcept;
  static bool isBoundary(std::string_view text, std::size_t offset) noexcept;
  static std::size_t nextBoundary(std::string_view text,
                                  std::size_t offset) noexcept;
  static std::size_t previousBoundary(std::string_view text,
                                      std::size_t offset) noexcept;
  static unsigned char folded(unsigned char byte,
                              bool case_sensitive) noexcept;

  bool bindingCurrent(const DocumentPort& port) const noexcept;
  void invalidateQueryResult(Status new_status) noexcept;
  void queryChanged() noexcept;
  bool initializeSegment(std::string_view text) noexcept;
  bool advanceCandidate(std::string_view text) noexcept;
  bool advanceOrWrap(std::string_view text) noexcept;
  void finishNotFound(DocumentPort& port) noexcept;

  FindConfig config_{};
  DocumentPort* open_port_ = nullptr;
  std::array<char, kHardMaximumQueryBytes> query_{};
  std::size_t query_size_ = 0;
  std::size_t query_cursor_ = 0;
  SourceStamp opened_stamp_{};
  std::size_t opened_cursor_ = 0;
  std::size_t last_match_begin_ = 0;
  MatchRange match_{};
  ScanTask task_{};
  Status status_ = Status::Closed;
  Direction direction_ = Direction::Forward;
  std::uint32_t query_version_ = 1;
  std::uint32_t session_id_ = 0;
  std::uint32_t highlight_version_ = 0;
  std::size_t last_step_work_ = 0;
  std::size_t maximum_step_work_ = 0;
  bool case_sensitive_ = false;
  bool wrapped_ = false;
  bool last_match_exists_ = false;
  bool open_ = false;
};

}  // namespace hanime::find
