#include "find_controller.hpp"

namespace hanime::find::input {

InputContext FindController::context(
    const bool composition_active, const bool direct_input) const noexcept {
  return {focus_, focus_generation_, session_.sessionId(), composition_active,
          direct_input};
}

KeyEvent FindController::stamp(const KeyKind kind, const char ascii,
                               const Modifier modifiers) const noexcept {
  return {kind, ascii, modifiers, focus_generation_, session_.sessionId()};
}

void FindController::changeFocus(const Focus focus) noexcept {
  if (focus_ == focus) return;
  focus_ = focus;
  ++focus_generation_;
  if (focus_generation_ == 0U) ++focus_generation_;
}

DispatchResult FindController::dispatch(
    const KeyEvent& event, const bool composition_active,
    const bool direct_input) noexcept {
  const RoutedCommand routed = ProductFindInputRouter::route(
      context(composition_active, direct_input), event);
  switch (routed.command) {
    case Command::PassToExistingInput:
      return DispatchResult::PassToExistingImeOrBody;
    case Command::CancelComposition:
      return DispatchResult::RequestImeCancel;
    case Command::IgnoreStale:
      return DispatchResult::IgnoredStale;
    case Command::ConsumeUnsupported:
      return DispatchResult::Handled;
    case Command::OpenFind:
      if (!session_.open(document_, composition_active)) {
        return DispatchResult::Rejected;
      }
      changeFocus(Focus::FindBar);
      return DispatchResult::Handled;
    case Command::CloseFind:
      if (!session_.close(document_, composition_active) && session_.view().open) {
        return DispatchResult::Rejected;
      }
      changeFocus(Focus::Body);
      return DispatchResult::Handled;
    case Command::SearchForward:
      session_.start(Direction::Forward);
      return DispatchResult::Handled;
    case Command::SearchBackward:
      session_.start(Direction::Backward);
      return DispatchResult::Handled;
    case Command::QueryBackspace:
      (void)session_.eraseQueryBackward();
      return DispatchResult::Handled;
    case Command::QueryDeleteForward:
      (void)session_.eraseQueryForward();
      return DispatchResult::Handled;
    case Command::QueryMoveLeft:
      (void)session_.moveQueryCursorLeft();
      return DispatchResult::Handled;
    case Command::QueryMoveRight:
      (void)session_.moveQueryCursorRight();
      return DispatchResult::Handled;
    case Command::QueryMoveHome:
      session_.moveQueryCursorHome();
      return DispatchResult::Handled;
    case Command::QueryMoveEnd:
      session_.moveQueryCursorEnd();
      return DispatchResult::Handled;
    case Command::QueryInsertAscii: {
      const char byte[1] = {routed.ascii};
      (void)session_.insertQueryUtf8({byte, 1U});
      return DispatchResult::Handled;
    }
    case Command::ToggleCaseSensitivity:
      session_.toggleCaseSensitive();
      return DispatchResult::Handled;
    case Command::MoveDocumentStart:
      return document_.moveDocumentStart() ? DispatchResult::Handled
                                            : DispatchResult::Rejected;
    case Command::MoveDocumentEnd:
      return document_.moveDocumentEnd() ? DispatchResult::Handled
                                          : DispatchResult::Rejected;
  }
  return DispatchResult::Rejected;
}

bool FindController::closeForExternalFocus(
    const bool composition_active) noexcept {
  if (focus_ != Focus::FindBar) return true;
  if (!session_.close(document_, composition_active) && session_.view().open) return false;
  changeFocus(Focus::Body);
  return true;
}

void FindController::invalidateQueuedInput() noexcept {
  session_.cancel();
  ++focus_generation_;
  if (focus_generation_ == 0U) ++focus_generation_;
}

void FindController::onDocumentSwitch() noexcept {
  session_.documentSwitched(document_);
  // Preserve the visible focus, but always invalidate queued events from the
  // previous document/session even when the enum value itself is unchanged.
  ++focus_generation_;
  if (focus_generation_ == 0U) ++focus_generation_;
}

}  // namespace hanime::find::input
