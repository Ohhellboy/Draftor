#include "body_editor.hpp"

#include "utf8.hpp"
#include "visual_cursor_navigator.hpp"

#include <algorithm>
#include <cstring>
#include <new>

namespace editor::body_edit {
namespace {

bool equalBytes(const std::string_view lhs,
                const std::string_view rhs) noexcept {
  return lhs.size() == rhs.size() &&
         (lhs.empty() || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

}  // namespace

const char* resultMessage(const ResultCode result) noexcept {
  switch (result) {
    case ResultCode::Applied: return "APPLIED";
    case ResultCode::NoAction: return "NO_ACTION";
    case ResultCode::InputStateDisallowed: return "INPUT_STATE_DISALLOWED";
    case ResultCode::InvalidUtf8: return "INVALID_UTF8";
    case ResultCode::InvalidPosition: return "INVALID_POSITION";
    case ResultCode::DocumentCapacityExceeded: return "DOCUMENT_CAPACITY_EXCEEDED";
    case ResultCode::ClipboardCapacityExceeded: return "CLIPBOARD_CAPACITY_EXCEEDED";
    case ResultCode::ClipboardEmpty: return "CLIPBOARD_EMPTY";
    case ResultCode::HistoryCapacityExceeded: return "HISTORY_CAPACITY_EXCEEDED";
    case ResultCode::ResourceExhausted: return "RESOURCE_EXHAUSTED";
    case ResultCode::HistoryUnavailable: return "HISTORY_UNAVAILABLE";
    case ResultCode::TransactionCapacityExceeded: return "TRANSACTION_CAPACITY_EXCEEDED";
    case ResultCode::HistoryDiverged: return "HISTORY_DIVERGED";
  }
  return "UNKNOWN";
}

BodyEditor::BodyEditor(Document& document, BodyEditorConfig config) noexcept
    : document_(document), config_(config) {
  if (config_.tab_spaces == 0U) config_.tab_spaces = 1U;
  if (config_.clipboard_bytes != 0U) {
    clipboard_storage_.reset(draftor::memory::array<char>(config_.clipboard_bytes));
  }
  if (!config_.record_history) { config_.history_bytes=0; config_.history_records=0; }
  if (config_.transaction_bytes != 0U) {
    transaction_staging_.reset(draftor::memory::array<char>(config_.transaction_bytes));
  }
  if (config_.history_bytes != 0U) {
    history_pool_.reset(draftor::memory::array<char>(config_.history_bytes));
  }
  if (config_.history_records != 0U) {
    records_.reset(
        draftor::memory::array<HistoryRecord>(config_.history_records));
  }
  ready_ = (config_.clipboard_bytes == 0U || clipboard_storage_ != nullptr) &&
           config_.transaction_bytes != 0U && transaction_staging_ != nullptr &&
           (!config_.record_history || (config_.history_bytes != 0U &&
             history_pool_ != nullptr && config_.history_records != 0U && records_ != nullptr));
  observed_text_revision_ = document_.textRevision();
  selection_anchor_ = document_.cursorByte();
}

BodyEditor::~BodyEditor() = default;

MemoryBudget BodyEditor::memoryBudget() const noexcept {
  MemoryBudget result;
  result.object_bytes = sizeof(*this);
  result.clipboard_bytes = clipboard_storage_ ? config_.clipboard_bytes : 0;
  result.history_pool_bytes = history_pool_ ? config_.history_bytes : 0;
  result.transaction_staging_bytes = transaction_staging_ ? config_.transaction_bytes : 0;
  result.history_record_bytes = records_ ? config_.history_records * sizeof(HistoryRecord) : 0;
  result.persistent_total_bytes = result.object_bytes +
      result.clipboard_bytes + result.history_pool_bytes +
      result.transaction_staging_bytes + result.history_record_bytes;
  return result;
}

SelectionView BodyEditor::selection() const noexcept {
  const std::size_t active = document_.cursorByte();
  return {selection_active_ ? std::min(selection_anchor_, active) : active,
          selection_active_ ? std::max(selection_anchor_, active) : active,
          selection_active_ ? selection_anchor_ : active,
          active,
          selection_version_,
          affinity_,
          selection_active_};
}

std::string_view BodyEditor::clipboard() const noexcept {
  return clipboard_size_ == 0U
             ? std::string_view{}
             : std::string_view(clipboard_storage_.get(), clipboard_size_);
}

ResultCode BodyEditor::translate(const EditResult result) noexcept {
  switch (result) {
    case EditResult::Changed: return ResultCode::Applied;
    case EditResult::NoChange: return ResultCode::NoAction;
    case EditResult::InvalidUtf8: return ResultCode::InvalidUtf8;
    case EditResult::CapacityExceeded:
      return ResultCode::DocumentCapacityExceeded;
    case EditResult::InvalidPosition: return ResultCode::InvalidPosition;
    case EditResult::ResourceExhausted: return ResultCode::ResourceExhausted;
  }
  return ResultCode::ResourceExhausted;
}

BodyEditor::CaretSnapshot BodyEditor::snapshot() const noexcept {
  return {document_.cursorByte(),
          selection_active_ ? selection_anchor_ : document_.cursorByte(),
          affinity_, selection_active_};
}

bool BodyEditor::selectionDiffers(const CaretSnapshot& before,
                                  const CaretSnapshot& after) const noexcept {
  if (before.has_selection != after.has_selection) return true;
  if (!before.has_selection) return false;
  return before.anchor != after.anchor || before.cursor != after.cursor;
}

void BodyEditor::applySelectionSnapshot(const CaretSnapshot& state) noexcept {
  selection_active_ = state.has_selection && state.anchor != state.cursor;
  selection_anchor_ = selection_active_ ? state.anchor : state.cursor;
  affinity_ = state.affinity;
}

void BodyEditor::clearSelectionAtCursor() noexcept {
  selection_active_ = false;
  selection_anchor_ = document_.cursorByte();
}

void BodyEditor::noteSelectionTransition(
    const CaretSnapshot& before) noexcept {
  if (selectionDiffers(before, snapshot())) ++selection_version_;
}

void BodyEditor::synchronizeExternalDocument() noexcept {
  if (document_.textRevision() == observed_text_revision_) return;
  prepared_ = {};
  history_used_ = 0;
  record_count_ = 0;
  undo_count_ = 0;
  const bool had_selection = selection_active_;
  selection_active_ = false;
  selection_anchor_ = document_.cursorByte();
  affinity_ = hanime::renderer::CaretAffinity::Downstream;
  observed_text_revision_ = document_.textRevision();
  if (had_selection) ++selection_version_;
}

ResultCode BodyEditor::prepareRange(const std::size_t begin,
                                    const std::size_t end,
                                    const std::string_view replacement,
                                    const std::size_t cursor_after) noexcept {
  synchronizeExternalDocument();
  prepared_ = {};
  if (!ready_) return ResultCode::ResourceExhausted;

  const EditResult preflight = document_.prepareReplaceRange(
      begin, end, replacement, cursor_after);
  if (preflight != EditResult::Changed && preflight != EditResult::NoChange) {
    return translate(preflight);
  }

  const std::string_view removed(document_.text().data() + begin, end - begin);
  const bool text_equal = equalBytes(removed, replacement);
  const std::size_t record_bytes = text_equal || !config_.record_history
      ? 0U : removed.size() + replacement.size();
  if (config_.record_history && record_bytes > config_.history_bytes) {
    return ResultCode::HistoryCapacityExceeded;
  }
  // Full stages old+new deltas for undo. Without history, only the inserted
  // bytes are needed for atomic/late commit; a deletion stages zero bytes.
  const std::size_t offset = record_bytes != 0U ? removed.size() : 0U;
  if (offset + replacement.size() > config_.transaction_bytes) {
    return ResultCode::TransactionCapacityExceeded;
  }
  if (offset != 0U) std::memcpy(transaction_staging_.get(), removed.data(), offset);
  if (!replacement.empty()) {
    std::memcpy(transaction_staging_.get() + offset, replacement.data(), replacement.size());
  }

  const CaretSnapshot before = snapshot();
  prepared_.begin = begin;
  prepared_.end = end;
  prepared_.replacement_bytes = replacement.size();
  prepared_.record_bytes = record_bytes;
  prepared_.replacement_offset = offset;
  prepared_.expected_revision = document_.revision();
  prepared_.expected_text_revision = document_.textRevision();
  prepared_.expected_selection_version = selection_version_;
  prepared_.before = before;
  prepared_.after = {cursor_after, cursor_after,
                     hanime::renderer::CaretAffinity::Downstream, false};
  prepared_.text_equal = text_equal;
  prepared_.valid = true;
  return ResultCode::Applied;
}

ResultCode BodyEditor::prepareTextInput(const std::string_view utf8) noexcept {
  synchronizeExternalDocument();
  prepared_ = {};
  if (utf8.empty()) return ResultCode::NoAction;
  const auto selected = selection();
  const std::size_t begin = selected.has_selection
      ? selected.begin : document_.cursorByte();
  const std::size_t end = selected.has_selection
      ? selected.end : document_.cursorByte();
  return prepareRange(begin, end, utf8, begin + utf8.size());
}

void BodyEditor::truncateRedo() noexcept {
  if (undo_count_ >= record_count_) return;
  history_used_ = records_[undo_count_].pool_offset;
  record_count_ = undo_count_;
}

void BodyEditor::evictOldest() noexcept {
  if (record_count_ == 0U) return;
  const std::size_t bytes =
      records_[0].removed_bytes + records_[0].inserted_bytes;
  if (bytes < history_used_) {
    std::memmove(history_pool_.get(), history_pool_.get() + bytes,
                 history_used_ - bytes);
  }
  history_used_ -= bytes;
  for (std::size_t i = 1; i < record_count_; ++i) {
    records_[i - 1U] = records_[i];
    records_[i - 1U].pool_offset -= bytes;
  }
  --record_count_;
  if (undo_count_ != 0U) --undo_count_;
  ++evicted_count_;
}

void BodyEditor::appendPreparedHistory() noexcept {
  truncateRedo();
  while (record_count_ >= config_.history_records ||
         history_used_ + prepared_.record_bytes > config_.history_bytes) {
    evictOldest();
  }

  HistoryRecord& record = records_[record_count_];
  record.pool_offset = history_used_;
  record.begin = prepared_.begin;
  record.removed_bytes = prepared_.end - prepared_.begin;
  record.inserted_bytes = prepared_.replacement_bytes;
  record.before = prepared_.before;
  record.after = prepared_.after;
  if (prepared_.record_bytes != 0U) {
    std::memcpy(history_pool_.get() + history_used_,
                transaction_staging_.get(), prepared_.record_bytes);
  }
  history_used_ += prepared_.record_bytes;
  ++record_count_;
  undo_count_ = record_count_;
}

Outcome BodyEditor::commitPrepared() noexcept {
  if (!prepared_.valid) return {};
  if (document_.revision() != prepared_.expected_revision ||
      document_.textRevision() != prepared_.expected_text_revision ||
      selection_version_ != prepared_.expected_selection_version) {
    prepared_ = {};
    return {ResultCode::HistoryDiverged};
  }

  const CaretSnapshot before = prepared_.before;
  const PreparedTransaction transaction = prepared_;
  if (transaction.text_equal) {
    const EditResult moved = document_.setCursorByte(transaction.after.cursor);
    applySelectionSnapshot(transaction.after);
    noteSelectionTransition(before);
    Change changes = Change::None;
    if (before.cursor != transaction.after.cursor ||
        before.affinity != transaction.after.affinity) {
      changes |= Change::Cursor;
    }
    if (selectionDiffers(before, transaction.after)) {
      changes |= Change::Selection;
    }
    prepared_ = {};
    if (moved == EditResult::InvalidPosition) {
      return {ResultCode::InvalidPosition};
    }
    return {changes == Change::None ? ResultCode::NoAction
                                    : ResultCode::Applied,
            changes};
  }

  const std::string_view replacement(
      transaction_staging_.get() + transaction.replacement_offset,
      transaction.replacement_bytes);
  const EditResult changed = document_.replaceRange(
      transaction.begin, transaction.end, replacement,
      transaction.after.cursor);
  if (changed != EditResult::Changed) {
    prepared_ = {};
    return {translate(changed)};
  }

  if (config_.record_history) appendPreparedHistory();
  applySelectionSnapshot(transaction.after);
  noteSelectionTransition(before);
  observed_text_revision_ = document_.textRevision();
  Change changes = Change::Text;
  if (before.cursor != transaction.after.cursor ||
      before.affinity != transaction.after.affinity) {
    changes |= Change::Cursor;
  }
  if (selectionDiffers(before, transaction.after)) {
    changes |= Change::Selection;
  }
  const DocumentTextEdit edit = document_.lastTextEdit();
  prepared_ = {};
  return {ResultCode::Applied, changes, edit};
}

Outcome BodyEditor::commitPreparedTextInput(
    const std::string_view committed_utf8) noexcept {
  if (!prepared_.valid) return {ResultCode::HistoryDiverged};
  const std::string_view staged(
      transaction_staging_.get() + prepared_.replacement_offset,
      prepared_.replacement_bytes);
  if (!equalBytes(staged, committed_utf8)) {
    prepared_ = {};
    return {ResultCode::HistoryDiverged};
  }
  return commitPrepared();
}

Outcome BodyEditor::performRange(const std::size_t begin,
                                 const std::size_t end,
                                 const std::string_view replacement,
                                 const std::size_t cursor_after) noexcept {
  const ResultCode prepared = prepareRange(begin, end, replacement, cursor_after);
  if (prepared != ResultCode::Applied) return {prepared};
  return commitPrepared();
}

Outcome BodyEditor::insertUtf8(const std::string_view utf8) noexcept {
  const ResultCode prepared = prepareTextInput(utf8);
  if (prepared != ResultCode::Applied) return {prepared};
  return commitPrepared();
}

Outcome BodyEditor::insertLineBreak() noexcept { return insertUtf8("\n"); }

Outcome BodyEditor::insertTab() noexcept {
  char spaces[255];
  const std::size_t count = config_.tab_spaces;
  std::memset(spaces, ' ', count);
  return insertUtf8(std::string_view(spaces, count));
}

Outcome BodyEditor::eraseBackward() noexcept {
  synchronizeExternalDocument();
  const auto selected = selection();
  if (selected.has_selection) {
    return performRange(selected.begin, selected.end, {}, selected.begin);
  }
  const std::size_t cursor = document_.cursorByte();
  if (cursor == 0U) return {};
  const std::size_t begin = previousUtf8Boundary(document_.text(), cursor);
  return performRange(begin, cursor, {}, begin);
}

Outcome BodyEditor::eraseForward() noexcept {
  synchronizeExternalDocument();
  const auto selected = selection();
  if (selected.has_selection) {
    return performRange(selected.begin, selected.end, {}, selected.begin);
  }
  const std::size_t cursor = document_.cursorByte();
  if (cursor == document_.text().size()) return {};
  const std::size_t end = nextUtf8Boundary(document_.text(), cursor);
  return performRange(cursor, end, {}, cursor);
}

Outcome BodyEditor::copy() noexcept {
  synchronizeExternalDocument();
  const auto selected = selection();
  if (!selected.has_selection) return {};
  const std::size_t bytes = selected.end - selected.begin;
  if (bytes > config_.clipboard_bytes || clipboard_storage_ == nullptr) {
    return {ResultCode::ClipboardCapacityExceeded};
  }
  if (bytes != 0U) {
    std::memcpy(clipboard_storage_.get(),
                document_.text().data() + selected.begin, bytes);
  }
  clipboard_size_ = bytes;
  return {ResultCode::Applied};
}

Outcome BodyEditor::cut() noexcept {
  synchronizeExternalDocument();
  const auto selected = selection();
  if (!selected.has_selection) return {};
  const std::size_t bytes = selected.end - selected.begin;
  if (bytes > config_.clipboard_bytes || clipboard_storage_ == nullptr) {
    return {ResultCode::ClipboardCapacityExceeded};
  }
  const ResultCode prepared = prepareRange(
      selected.begin, selected.end, {}, selected.begin);
  if (prepared != ResultCode::Applied) return {prepared};
  if (!config_.record_history) {
    if (bytes > config_.transaction_bytes) {
      prepared_ = {};
      return {ResultCode::TransactionCapacityExceeded};
    }
    std::memcpy(transaction_staging_.get(), document_.text().data()+selected.begin, bytes);
  }
  const Outcome result = commitPrepared();
  if (!result.textChanged()) return result;
  if (bytes != 0U) {
    std::memcpy(clipboard_storage_.get(), transaction_staging_.get(), bytes);
  }
  clipboard_size_ = bytes;
  return result;
}

Outcome BodyEditor::paste() noexcept {
  synchronizeExternalDocument();
  if (clipboard_size_ == 0U) return {ResultCode::ClipboardEmpty};
  return insertUtf8(clipboard());
}

std::string_view BodyEditor::recordRemoved(
    const HistoryRecord& record) const noexcept {
  return {history_pool_.get() + record.pool_offset, record.removed_bytes};
}

std::string_view BodyEditor::recordInserted(
    const HistoryRecord& record) const noexcept {
  return {history_pool_.get() + record.pool_offset + record.removed_bytes,
          record.inserted_bytes};
}

bool BodyEditor::historyRecordMatches(const HistoryRecord& record,
                                      const bool applied_form) const noexcept {
  const std::string_view expected = applied_form
      ? recordInserted(record) : recordRemoved(record);
  if (record.begin > document_.text().size() ||
      expected.size() > document_.text().size() - record.begin) {
    return false;
  }
  return equalBytes(
      std::string_view(document_.text()).substr(record.begin, expected.size()),
      expected);
}

Outcome BodyEditor::undo() noexcept {
  if (!config_.record_history) return {ResultCode::HistoryUnavailable};
  synchronizeExternalDocument();
  if (undo_count_ == 0U) return {};
  const HistoryRecord& record = records_[undo_count_ - 1U];
  if (!historyRecordMatches(record, true)) {
    return {ResultCode::HistoryDiverged};
  }
  const CaretSnapshot before = snapshot();
  const EditResult result = document_.replaceRange(
      record.begin, record.begin + record.inserted_bytes,
      recordRemoved(record), record.before.cursor);
  if (result != EditResult::Changed) return {translate(result)};
  --undo_count_;
  applySelectionSnapshot(record.before);
  noteSelectionTransition(before);
  observed_text_revision_ = document_.textRevision();
  Change changes = Change::Text;
  if (before.cursor != record.before.cursor ||
      before.affinity != record.before.affinity) {
    changes |= Change::Cursor;
  }
  if (selectionDiffers(before, record.before)) changes |= Change::Selection;
  return {ResultCode::Applied, changes, document_.lastTextEdit()};
}

Outcome BodyEditor::redo() noexcept {
  if (!config_.record_history) return {ResultCode::HistoryUnavailable};
  synchronizeExternalDocument();
  if (undo_count_ >= record_count_) return {};
  const HistoryRecord& record = records_[undo_count_];
  if (!historyRecordMatches(record, false)) {
    return {ResultCode::HistoryDiverged};
  }
  const CaretSnapshot before = snapshot();
  const EditResult result = document_.replaceRange(
      record.begin, record.begin + record.removed_bytes,
      recordInserted(record), record.after.cursor);
  if (result != EditResult::Changed) return {translate(result)};
  ++undo_count_;
  applySelectionSnapshot(record.after);
  noteSelectionTransition(before);
  observed_text_revision_ = document_.textRevision();
  Change changes = Change::Text;
  if (before.cursor != record.after.cursor ||
      before.affinity != record.after.affinity) {
    changes |= Change::Cursor;
  }
  if (selectionDiffers(before, record.after)) changes |= Change::Selection;
  return {ResultCode::Applied, changes, document_.lastTextEdit()};
}

Outcome BodyEditor::movementOutcome(const CaretSnapshot& before,
                                    const bool affinity_changed) noexcept {
  const CaretSnapshot after = snapshot();
  noteSelectionTransition(before);
  Change changes = Change::None;
  if (before.cursor != after.cursor || affinity_changed) {
    changes |= Change::Cursor;
  }
  if (selectionDiffers(before, after)) changes |= Change::Selection;
  return {changes == Change::None ? ResultCode::NoAction : ResultCode::Applied,
          changes};
}

Outcome BodyEditor::moveToByte(const std::size_t byte_offset,
    const bool extend_selection,
    const hanime::renderer::CaretAffinity affinity) noexcept {
  if (!ready_) return {ResultCode::ResourceExhausted};
  if (!isUtf8Boundary(document_.text(), byte_offset))
    return {ResultCode::InvalidPosition};
  synchronizeExternalDocument();
  const auto before = snapshot();
  if (extend_selection && !selection_active_) selection_anchor_ = before.cursor;
  const auto moved = document_.setCursorByte(byte_offset);
  if (moved == EditResult::InvalidPosition) return {ResultCode::InvalidPosition};
  affinity_ = affinity;
  if (extend_selection) {
    selection_active_ = document_.cursorByte() != selection_anchor_;
    if (!selection_active_) selection_anchor_ = document_.cursorByte();
  } else clearSelectionAtCursor();
  prepared_ = {};
  return movementOutcome(before, before.affinity != affinity_);
}

Outcome BodyEditor::selectAll() noexcept {
  synchronizeExternalDocument();
  if (document_.text().empty()) return {};
  const CaretSnapshot before = snapshot();
  const EditResult moved = document_.setCursorByte(document_.text().size());
  if (moved == EditResult::InvalidPosition) {
    return {ResultCode::InvalidPosition};
  }
  selection_anchor_ = 0U;
  selection_active_ = document_.cursorByte() != 0U;
  affinity_ = hanime::renderer::CaretAffinity::Downstream;
  return movementOutcome(before, before.affinity != affinity_);
}

Outcome BodyEditor::cancelSelection() noexcept {
  synchronizeExternalDocument();
  if (!selection_active_) return {};
  const CaretSnapshot before = snapshot();
  clearSelectionAtCursor();
  return movementOutcome(before, false);
}

Outcome BodyEditor::moveHorizontal(
    visual_nav::VisualCursorNavigator& navigator,
    const visual_nav::VisualHorizontalDirection direction,
    const bool extend_selection) noexcept {
  synchronizeExternalDocument();
  affinity_ = navigator.caretAffinity();
  if (!extend_selection && selection_active_) {
    const auto selected = selection();
    const auto target = direction == visual_nav::VisualHorizontalDirection::Left
        ? selected.begin : selected.end;
    const auto outcome = moveToByte(target);
    navigator.restoreCaretAffinity(affinity_);
    (void)navigator.sync();
    return outcome;
  }
  return navigator.moveHorizontal(direction, *this, extend_selection);
}

Outcome BodyEditor::moveVertical(
    visual_nav::VisualCursorNavigator& navigator,
    const visual_nav::VisualDirection direction,
    const bool extend_selection) noexcept {
  synchronizeExternalDocument();
  affinity_ = navigator.caretAffinity();
  return navigator.move(direction, *this, extend_selection);
}

Outcome BodyEditor::moveLogicalLine(
    visual_nav::VisualCursorNavigator& navigator,
    const CursorMove direction,
    const bool extend_selection) noexcept {
  synchronizeExternalDocument();
  affinity_ = navigator.caretAffinity();
  const auto outcome = moveToByte(document_.cursorTarget(direction), extend_selection);
  navigator.restoreCaretAffinity(affinity_);
  (void)navigator.sync();
  return outcome;
}

void BodyEditor::resetForDocument(const bool identity_changed) noexcept {
  if (identity_changed && !config_.cross_document_clipboard) clipboard_size_ = 0;
  const bool had_selection = selection_active_;
  prepared_ = {};
  history_used_ = 0;
  record_count_ = 0;
  undo_count_ = 0;
  selection_active_ = false;
  selection_anchor_ = document_.cursorByte();
  affinity_ = hanime::renderer::CaretAffinity::Downstream;
  observed_text_revision_ = document_.textRevision();
  if (had_selection) ++selection_version_;
}

}  // namespace editor::body_edit
