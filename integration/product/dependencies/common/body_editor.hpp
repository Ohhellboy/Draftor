#include "core_memory.hpp"
#pragma once

#include "document.hpp"
#include "layout.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace editor::visual_nav {
class VisualCursorNavigator;
enum class VisualHorizontalDirection : std::uint8_t;
enum class VisualDirection : std::uint8_t;
}  // namespace editor::visual_nav

namespace editor::body_edit {

inline constexpr std::uint16_t kBodyEditorProtocolVersion = 1;

struct BodyEditorConfig {
  std::size_t clipboard_bytes = 4U * 1024U;
  std::size_t history_bytes = 8U * 1024U;
  std::size_t history_records = 32U;
  std::uint8_t tab_spaces = 4U;
  std::size_t transaction_bytes = 8U * 1024U;
  bool record_history = true;
  bool cross_document_clipboard = true;
};

enum class ResultCode : std::uint8_t {
  Applied,
  NoAction,
  InputStateDisallowed,
  InvalidUtf8,
  InvalidPosition,
  DocumentCapacityExceeded,
  ClipboardCapacityExceeded,
  ClipboardEmpty,
  HistoryCapacityExceeded,
  ResourceExhausted,
  HistoryDiverged,
  HistoryUnavailable,
  TransactionCapacityExceeded,
};

enum class Change : std::uint8_t {
  None = 0,
  Text = 1U << 0U,
  Cursor = 1U << 1U,
  Selection = 1U << 2U,
};

constexpr Change operator|(const Change lhs, const Change rhs) noexcept {
  return static_cast<Change>(static_cast<std::uint8_t>(lhs) |
                             static_cast<std::uint8_t>(rhs));
}

constexpr Change& operator|=(Change& lhs, const Change rhs) noexcept {
  lhs = lhs | rhs;
  return lhs;
}

constexpr bool has(const Change value, const Change flag) noexcept {
  return (static_cast<std::uint8_t>(value) &
          static_cast<std::uint8_t>(flag)) != 0U;
}

struct Outcome {
  ResultCode code = ResultCode::NoAction;
  Change changes = Change::None;
  DocumentTextEdit text_edit{};

  bool textChanged() const noexcept { return has(changes, Change::Text); }
  bool cursorChanged() const noexcept { return has(changes, Change::Cursor); }
  bool selectionChanged() const noexcept {
    return has(changes, Change::Selection);
  }
};

struct SelectionView {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::size_t anchor = 0;
  std::size_t active = 0;
  std::uint32_t version = 0;
  hanime::renderer::CaretAffinity active_affinity =
      hanime::renderer::CaretAffinity::Downstream;
  bool has_selection = false;
};

struct MemoryBudget {
  std::size_t object_bytes = 0;
  std::size_t clipboard_bytes = 0;
  std::size_t history_pool_bytes = 0;
  std::size_t transaction_staging_bytes = 0;
  std::size_t history_record_bytes = 0;
  std::size_t persistent_total_bytes = 0;
};

// BodyEditor owns selection anchor, bounded device clipboard, and delta
// history. Document remains the only owner of text and the active byte cursor.
// All text-changing operations pass through one atomic range transaction.
class BodyEditor {
 public:
  explicit BodyEditor(Document& document, BodyEditorConfig config = {}) noexcept;
  ~BodyEditor();

  BodyEditor(const BodyEditor&) = delete;
  BodyEditor& operator=(const BodyEditor&) = delete;

  const void* clipboardAllocation() const noexcept { return clipboard_storage_.get(); }
  const void* historyAllocation() const noexcept { return history_pool_.get(); }
  const void* stagingAllocation() const noexcept { return transaction_staging_.get(); }
  const void* historyRecordsAllocation() const noexcept { return records_.get(); }
  static constexpr std::size_t historyRecordSize() noexcept { return sizeof(HistoryRecord); }
  bool ready() const noexcept { return ready_; }
  const BodyEditorConfig& config() const noexcept { return config_; }
  MemoryBudget memoryBudget() const noexcept;

  SelectionView selection() const noexcept;
  std::uint32_t selectionVersion() const noexcept { return selection_version_; }
  hanime::renderer::CaretAffinity caretAffinity() const noexcept {
    return affinity_;
  }

  std::string_view clipboard() const noexcept;
  std::size_t undoCount() const noexcept { return undo_count_; }
  std::size_t redoCount() const noexcept { return record_count_ - undo_count_; }
  std::size_t historyBytesUsed() const noexcept { return history_used_; }
  std::size_t evictedTransactionCount() const noexcept { return evicted_count_; }

  // Two-phase input is used by IME commit: preflight and stage the complete
  // transaction before the IME relinquishes its committed string.
  ResultCode prepareTextInput(std::string_view utf8) noexcept;
  Outcome commitPreparedTextInput(std::string_view committed_utf8) noexcept;
  void discardPreparedTextInput() noexcept { prepared_ = {}; }

  Outcome insertUtf8(std::string_view utf8) noexcept;
  Outcome insertLineBreak() noexcept;
  Outcome insertTab() noexcept;
  Outcome eraseBackward() noexcept;
  Outcome eraseForward() noexcept;

  Outcome copy() noexcept;
  Outcome cut() noexcept;
  Outcome paste() noexcept;
  Outcome undo() noexcept;
  Outcome redo() noexcept;

  Outcome moveToByte(std::size_t byte_offset, bool extend_selection = false,
      hanime::renderer::CaretAffinity affinity =
          hanime::renderer::CaretAffinity::Downstream) noexcept;
  Outcome selectAll() noexcept;
  Outcome cancelSelection() noexcept;
  Outcome moveHorizontal(
      visual_nav::VisualCursorNavigator& navigator,
      visual_nav::VisualHorizontalDirection direction,
      bool extend_selection) noexcept;
  Outcome moveVertical(
      visual_nav::VisualCursorNavigator& navigator,
      visual_nav::VisualDirection direction,
      bool extend_selection) noexcept;
  Outcome moveLogicalLine(
      visual_nav::VisualCursorNavigator& navigator,
      CursorMove direction,
      bool extend_selection) noexcept;

  // Install clears selection/history. Pass identity_changed only after a
  // successful activation of another document; Full keeps its clipboard.
  void resetForDocument(bool identity_changed = false) noexcept;

 private:
  struct CaretSnapshot {
    std::size_t cursor = 0;
    std::size_t anchor = 0;
    hanime::renderer::CaretAffinity affinity =
        hanime::renderer::CaretAffinity::Downstream;
    bool has_selection = false;
  };

  struct HistoryRecord {
    std::size_t pool_offset = 0;
    std::size_t begin = 0;
    std::size_t removed_bytes = 0;
    std::size_t inserted_bytes = 0;
    CaretSnapshot before{};
    CaretSnapshot after{};
  };

  struct PreparedTransaction {
    std::size_t begin = 0;
    std::size_t end = 0;
    std::size_t replacement_bytes = 0;
    std::size_t record_bytes = 0;
    std::size_t replacement_offset = 0;
    std::uint32_t expected_revision = 0;
    std::uint32_t expected_text_revision = 0;
    std::uint32_t expected_selection_version = 0;
    CaretSnapshot before{};
    CaretSnapshot after{};
    bool text_equal = false;
    bool valid = false;
  };

  static ResultCode translate(EditResult result) noexcept;
  CaretSnapshot snapshot() const noexcept;
  bool selectionDiffers(const CaretSnapshot& before,
                        const CaretSnapshot& after) const noexcept;
  void applySelectionSnapshot(const CaretSnapshot& state) noexcept;
  void clearSelectionAtCursor() noexcept;
  void noteSelectionTransition(const CaretSnapshot& before) noexcept;
  void synchronizeExternalDocument() noexcept;

  ResultCode prepareRange(std::size_t begin, std::size_t end,
                          std::string_view replacement,
                          std::size_t cursor_after) noexcept;
  Outcome commitPrepared() noexcept;
  Outcome performRange(std::size_t begin, std::size_t end,
                       std::string_view replacement,
                       std::size_t cursor_after) noexcept;
  Outcome movementOutcome(const CaretSnapshot& before,
                          bool affinity_changed) noexcept;

  void truncateRedo() noexcept;
  void evictOldest() noexcept;
  void appendPreparedHistory() noexcept;
  bool historyRecordMatches(const HistoryRecord& record,
                            bool applied_form) const noexcept;
  std::string_view recordRemoved(const HistoryRecord& record) const noexcept;
  std::string_view recordInserted(const HistoryRecord& record) const noexcept;

  Document& document_;
  BodyEditorConfig config_{};
  draftor::memory::ArrayPtr<char> clipboard_storage_;
  draftor::memory::ArrayPtr<char> history_pool_;
  draftor::memory::ArrayPtr<char> transaction_staging_;
  draftor::memory::ArrayPtr<HistoryRecord> records_;
  std::size_t clipboard_size_ = 0;
  std::size_t history_used_ = 0;
  std::size_t record_count_ = 0;
  std::size_t undo_count_ = 0;
  std::size_t evicted_count_ = 0;
  std::size_t selection_anchor_ = 0;
  std::uint32_t selection_version_ = 0;
  std::uint32_t observed_text_revision_ = 0;
  hanime::renderer::CaretAffinity affinity_ =
      hanime::renderer::CaretAffinity::Downstream;
  PreparedTransaction prepared_{};
  bool selection_active_ = false;
  bool ready_ = false;
};

const char* resultMessage(ResultCode result) noexcept;

}  // namespace editor::body_edit
