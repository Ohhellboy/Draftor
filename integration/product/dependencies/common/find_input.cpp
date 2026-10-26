#include "find_input.hpp"

#include <cstring>

namespace hanime::find::input {

namespace {

bool exact(const Modifier actual, const Modifier expected) noexcept {
  return actual == expected;
}

bool printable(const char c) noexcept {
  const auto byte = static_cast<unsigned char>(c);
  return byte >= 0x20U && byte <= 0x7EU;
}

char lowerAscii(const char c) noexcept {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

RoutedCommand ProductFindInputRouter::route(
    const InputContext& context, const KeyEvent& event) noexcept {
  if (event.focus_generation != context.focus_generation) {
    return {Command::IgnoreStale, 0};
  }
  if (context.focus == Focus::FindBar &&
      event.find_session_id != context.find_session_id) {
    return {Command::IgnoreStale, 0};
  }

  if (context.focus == Focus::Body) {
    if (event.kind == KeyKind::Character &&
        lowerAscii(event.ascii) == 'f' &&
        (exact(event.modifiers, Modifier::Ctrl) ||
         exact(event.modifiers, Modifier::Gui))) {
      return {context.composition_active ? Command::ConsumeUnsupported
                                         : Command::OpenFind, 0};
    }
    if (!context.composition_active && event.kind == KeyKind::Home &&
        exact(event.modifiers, Modifier::Ctrl)) {
      return {Command::MoveDocumentStart, 0};
    }
    if (!context.composition_active && event.kind == KeyKind::End &&
        exact(event.modifiers, Modifier::Ctrl)) {
      return {Command::MoveDocumentEnd, 0};
    }
    if (!context.composition_active && event.kind == KeyKind::Up &&
        exact(event.modifiers, Modifier::Ctrl | Modifier::Alt)) {
      return {Command::MoveDocumentStart, 0};
    }
    if (!context.composition_active && event.kind == KeyKind::Down &&
        exact(event.modifiers, Modifier::Ctrl | Modifier::Alt)) {
      return {Command::MoveDocumentEnd, 0};
    }
    if (event.modifiers != Modifier::None) {
      // Existing BodyEditor shortcuts and its unsupported-chord sink remain
      // authoritative outside Find. Only Find-owned chords are intercepted.
      return {Command::PassToExistingInput, event.ascii};
    }
    return {Command::PassToExistingInput, event.ascii};
  }

  if (context.composition_active) {
    // Direction, digits, Space/Enter, Backspace and Escape all remain owned by
    // the existing IME while composing. Escape is named so the shell knows it
    // must cancel composition without closing the bar.
    if (event.kind == KeyKind::Escape) {
      return {Command::CancelComposition, 0};
    }
    if (event.modifiers != Modifier::None) {
      return {Command::ConsumeUnsupported, 0};
    }
    return {Command::PassToExistingInput, event.ascii};
  }

  if (event.kind == KeyKind::ToggleInputMode &&
      exact(event.modifiers, Modifier::None)) {
    return {Command::PassToExistingInput, 0};
  }

  if (event.kind == KeyKind::Enter && exact(event.modifiers, Modifier::None)) {
    return {Command::SearchForward, 0};
  }
  if (event.kind == KeyKind::Enter && exact(event.modifiers, Modifier::Shift)) {
    return {Command::SearchBackward, 0};
  }
  if (event.kind == KeyKind::Escape && exact(event.modifiers, Modifier::None)) {
    return {Command::CloseFind, 0};
  }
  if (event.kind == KeyKind::Backspace &&
      exact(event.modifiers, Modifier::None)) {
    return {Command::QueryBackspace, 0};
  }
  if (event.kind == KeyKind::DeleteForward &&
      exact(event.modifiers, Modifier::None)) {
    return {Command::QueryDeleteForward, 0};
  }
  if (event.kind == KeyKind::Left &&
      (exact(event.modifiers, Modifier::None)||exact(event.modifiers, Modifier::Shift))) {
    return {Command::QueryMoveLeft, 0,exact(event.modifiers, Modifier::Shift)};
  }
  if (event.kind == KeyKind::Right &&
      (exact(event.modifiers, Modifier::None)||exact(event.modifiers, Modifier::Shift))) {
    return {Command::QueryMoveRight, 0,exact(event.modifiers, Modifier::Shift)};
  }
  if (event.kind == KeyKind::Home &&
      (exact(event.modifiers, Modifier::None)||exact(event.modifiers, Modifier::Shift))) {
    return {Command::QueryMoveHome, 0,exact(event.modifiers, Modifier::Shift)};
  }
  if (event.kind == KeyKind::End &&
      (exact(event.modifiers, Modifier::None)||exact(event.modifiers, Modifier::Shift))) {
    return {Command::QueryMoveEnd, 0,exact(event.modifiers, Modifier::Shift)};
  }
  if(event.kind==KeyKind::Character&&lowerAscii(event.ascii)=='a'&&
     (exact(event.modifiers,Modifier::Ctrl)||exact(event.modifiers,Modifier::Gui)))return {Command::QuerySelectAll,0};
  // Ctrl+I is the default demo/product semantic entry for the v0.1 option.
  if (event.kind == KeyKind::Character && lowerAscii(event.ascii) == 'i' &&
      exact(event.modifiers, Modifier::Ctrl)) {
    return {Command::ToggleCaseSensitivity, 0};
  }
  if (event.kind == KeyKind::Space &&
      exact(event.modifiers, Modifier::None)) {
    return {Command::QueryInsertAscii, ' '};
  }
  if (event.kind == KeyKind::Character &&
      exact(event.modifiers, Modifier::None) && printable(event.ascii) &&
      (context.direct_input || lowerAscii(event.ascii) < 'a' ||
       lowerAscii(event.ascii) > 'z')) {
    return {Command::QueryInsertAscii, event.ascii};
  }
  if (event.kind == KeyKind::Character &&
      exact(event.modifiers, Modifier::None) && printable(event.ascii)) {
    return {Command::PassToExistingInput, event.ascii};
  }
  return {event.modifiers == Modifier::None
              ? Command::ConsumeUnsupported
              : Command::ConsumeUnsupported, 0};
}

CommitResult FindQueryCommitTarget::prepare(
    const std::string_view utf8) noexcept {
  prepared_ = false;
  if (utf8.empty() || utf8.size() > staging_.size() ||
      !session_.canInsertQuery(utf8)) return CommitResult::Rejected;
  std::memcpy(staging_.data(), utf8.data(), utf8.size());
  staged_size_ = utf8.size();
  const ViewModel view = session_.view();
  expected_session_id_ = session_.sessionId();
  expected_query_version_ = view.query_version;
  expected_query_cursor_ = view.query_cursor;
  expected_selected_ = view.query_selected; expected_selection_=view.selection_version;
  prepared_ = true;
  return CommitResult::Prepared;
}

CommitResult FindQueryCommitTarget::commit(
    const std::string_view committed_utf8) noexcept {
  if (!prepared_) return CommitResult::NoPreparedCommit;
  const ViewModel view = session_.view();
  if (!view.open || session_.sessionId() != expected_session_id_ ||
      view.query_version != expected_query_version_ ||
      view.query_cursor != expected_query_cursor_ || view.query_selected != expected_selected_ || view.selection_version != expected_selection_ ||
      committed_utf8.size() != staged_size_ ||
      (staged_size_ != 0U &&
       std::memcmp(staging_.data(), committed_utf8.data(), staged_size_) != 0)) {
    prepared_ = false;
    return CommitResult::Stale;
  }
  prepared_ = false;
  return session_.insertQueryUtf8(committed_utf8) == QueryEditResult::Applied
      ? CommitResult::Committed : CommitResult::Rejected;
}

}  // namespace hanime::find::input
