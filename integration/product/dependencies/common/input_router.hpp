#pragma once

#include <cstddef>
#include <cstdint>

namespace editor::input {

// Product-facing interaction state. Body means Chinese/HANIME mode with no
// active composition; DirectBody means English/direct-text mode. Composition
// states are reachable only while Chinese/HANIME mode is active.
enum class InteractionState : std::uint8_t {
  Body = 0,
  DirectBody,
  ComposingNoCandidates,
  ComposingWithCandidates,
};

enum class KeyKind : std::uint8_t {
  Character = 0,
  Enter,
  Escape,
  Backspace,
  DeleteForward,
  Space,
  Tab,
  Left,
  Right,
  Up,
  Down,
  Home,
  End,
  ToggleInputMode,
};

enum KeyModifier : std::uint8_t {
  ModifierNone = 0,
  ModifierShift = 1U << 0U,
  ModifierCtrl = 1U << 1U,
  ModifierAlt = 1U << 2U,
  ModifierGui = 1U << 3U,
};

struct NormalizedKey {
  KeyKind kind = KeyKind::Character;
  char ascii = 0;
  std::uint8_t modifiers = ModifierNone;

  static constexpr NormalizedKey character(
      char c, std::uint8_t modifiers = ModifierNone) noexcept {
    return {KeyKind::Character, c, modifiers};
  }
  static constexpr NormalizedKey special(
      KeyKind kind, std::uint8_t modifiers = ModifierNone) noexcept {
    return {kind, 0, modifiers};
  }

  constexpr bool shift() const noexcept {
    return (modifiers & ModifierShift) != 0U;
  }
  constexpr bool ctrl() const noexcept {
    return (modifiers & ModifierCtrl) != 0U;
  }
  constexpr bool alt() const noexcept {
    return (modifiers & ModifierAlt) != 0U;
  }
  constexpr bool gui() const noexcept {
    return (modifiers & ModifierGui) != 0U;
  }
};

enum class ActionKind : std::uint8_t {
  Ignore = 0,

  // Product input mode.
  InputModeToggle,

  // HANIME / composition actions.
  ImeAppendToken,
  CandidateChooseVisibleSlot,
  CandidateCommitSelected,
  CompositionCancel,
  ContextBackspace,

  // Body/editor actions.
  BodyInsertAscii,
  BodyInsertChinesePunctuation,
  BodyDeleteForward,
  BodyInsertLineBreak,
  BodyInsertTab,
  BodyCancelSelection,
  BodySelectAll,
  BodyCopy,
  BodyCut,
  BodyPaste,
  BodyUndo,
  BodyRedo,
  BodyCommandBlocked,
  BodyMoveLeft,
  BodyMoveRight,
  BodyMoveVisualUp,
  BodyMoveVisualDown,

  // Home/End remain the existing logical-paragraph semantics.
  BodyMoveLogicalLineStart,
  BodyMoveLogicalLineEnd,
  BodyExtendLeft,
  BodyExtendRight,
  BodyExtendVisualUp,
  BodyExtendVisualDown,
  BodyExtendLogicalLineStart,
  BodyExtendLogicalLineEnd,

  // Candidate movement semantics remain owned by CandidateNavigator.
  CandidateMoveLeft,
  CandidateMoveRight,
  CandidateMoveUp,
  CandidateMoveDown,
};

struct RoutedAction {
  ActionKind kind = ActionKind::Ignore;
  char ascii = 0;          // token/direct ASCII/source punctuation
  std::uint8_t slot = 0;   // zero-based CandidateChooseVisibleSlot

  constexpr bool operator==(const RoutedAction& other) const noexcept {
    return kind == other.kind && ascii == other.ascii && slot == other.slot;
  }
};

class InputRouter {
 public:
  constexpr RoutedAction route(InteractionState state,
                               NormalizedKey key) const noexcept;

 private:
  static constexpr bool isComposition(InteractionState state) noexcept {
    return state == InteractionState::ComposingNoCandidates ||
           state == InteractionState::ComposingWithCandidates;
  }
  static constexpr bool isDirect(InteractionState state) noexcept {
    return state == InteractionState::DirectBody;
  }
  static constexpr bool hasCandidates(InteractionState state) noexcept {
    return state == InteractionState::ComposingWithCandidates;
  }
  static constexpr char normalizeAsciiLetter(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  static constexpr bool isPrintableAscii(char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    return byte >= 0x20U && byte <= 0x7EU;
  }
};

constexpr RoutedAction InputRouter::route(const InteractionState state,
                                          const NormalizedKey key) const noexcept {
  const bool composing = isComposition(state);
  const bool candidates = hasCandidates(state);
  const bool direct = isDirect(state);

  // Classify modifier chords before candidate-demand deferral. Supported body
  // commands are explicit, composing rejects them explicitly, and every other
  // Ctrl/Alt/GUI chord is consumed without leaking a character or IME token.
  // Ctrl+GUI and Alt-mixed chords are undefined and therefore ignored.
  if (key.alt() || (key.ctrl() && key.gui())) return {};
  if (key.ctrl() || key.gui()) {
    const char c = normalizeAsciiLetter(key.ascii);
    ActionKind command = ActionKind::Ignore;
    if (c == 'a' && !key.shift()) command = ActionKind::BodySelectAll;
    else if (c == 'c' && !key.shift()) command = ActionKind::BodyCopy;
    else if (c == 'x' && !key.shift()) command = ActionKind::BodyCut;
    else if (c == 'v' && !key.shift()) command = ActionKind::BodyPaste;
    else if (c == 'z') command = key.shift()
        ? ActionKind::BodyRedo : ActionKind::BodyUndo;
    else if (c == 'y' && key.ctrl() && !key.shift()) {
      command = ActionKind::BodyRedo;
    }
    if (command == ActionKind::Ignore) return {};
    return composing ? RoutedAction{ActionKind::BodyCommandBlocked, 0, 0}
                     : RoutedAction{command, 0, 0};
  }

  const bool selection_direction = key.shift() &&
      (key.kind == KeyKind::Left || key.kind == KeyKind::Right ||
       key.kind == KeyKind::Up || key.kind == KeyKind::Down ||
       key.kind == KeyKind::Home || key.kind == KeyKind::End);
  if (composing && (selection_direction || key.kind == KeyKind::Tab)) {
    return {ActionKind::BodyCommandBlocked, 0, 0};
  }

  if (key.kind == KeyKind::ToggleInputMode) {
    return {ActionKind::InputModeToggle, 0, 0};
  }

  switch (key.kind) {
    case KeyKind::Enter:
    case KeyKind::Space:
      return composing
          ? RoutedAction{ActionKind::CandidateCommitSelected, 0, 0}
          : (key.kind == KeyKind::Enter
                 ? RoutedAction{ActionKind::BodyInsertLineBreak, 0, 0}
                 : RoutedAction{ActionKind::BodyInsertAscii, ' ', 0});

    case KeyKind::Escape:
      return composing
          ? RoutedAction{ActionKind::CompositionCancel, 0, 0}
          : RoutedAction{ActionKind::BodyCancelSelection, 0, 0};

    case KeyKind::Tab:
      return key.shift()
          ? RoutedAction{}
          : RoutedAction{ActionKind::BodyInsertTab, 0, 0};

    case KeyKind::Backspace:
      return {ActionKind::ContextBackspace, 0, 0};

    case KeyKind::DeleteForward:
      return composing
          ? RoutedAction{}
          : RoutedAction{ActionKind::BodyDeleteForward, 0, 0};

    case KeyKind::Left:
      return composing
          ? RoutedAction{ActionKind::CandidateMoveLeft, 0, 0}
          : RoutedAction{key.shift() ? ActionKind::BodyExtendLeft
                                     : ActionKind::BodyMoveLeft, 0, 0};

    case KeyKind::Right:
      return composing
          ? RoutedAction{ActionKind::CandidateMoveRight, 0, 0}
          : RoutedAction{key.shift() ? ActionKind::BodyExtendRight
                                     : ActionKind::BodyMoveRight, 0, 0};

    case KeyKind::Up:
      return composing
          ? RoutedAction{ActionKind::CandidateMoveUp, 0, 0}
          : RoutedAction{key.shift() ? ActionKind::BodyExtendVisualUp
                                     : ActionKind::BodyMoveVisualUp, 0, 0};

    case KeyKind::Down:
      return composing
          ? RoutedAction{ActionKind::CandidateMoveDown, 0, 0}
          : RoutedAction{key.shift() ? ActionKind::BodyExtendVisualDown
                                     : ActionKind::BodyMoveVisualDown, 0, 0};

    case KeyKind::Home:
      return composing
          ? RoutedAction{}
          : RoutedAction{key.shift()
                             ? ActionKind::BodyExtendLogicalLineStart
                             : ActionKind::BodyMoveLogicalLineStart, 0, 0};

    case KeyKind::End:
      return composing
          ? RoutedAction{}
          : RoutedAction{key.shift()
                             ? ActionKind::BodyExtendLogicalLineEnd
                             : ActionKind::BodyMoveLogicalLineEnd, 0, 0};

    case KeyKind::ToggleInputMode:
      return {ActionKind::InputModeToggle, 0, 0};

    case KeyKind::Character:
      break;
  }

  // Direct/English mode bypasses HANIME completely. Preserve the translated
  // printable ASCII byte, including case, symbols and punctuation.
  if (direct) {
    return isPrintableAscii(key.ascii)
               ? RoutedAction{ActionKind::BodyInsertAscii, key.ascii, 0}
               : RoutedAction{};
  }

  // Normalized ASCII only: shifted '_'/'+' keep their previous meaning.
  if (composing && key.ascii == '-') return {ActionKind::CandidateMoveUp, 0, 0};
  if (composing && key.ascii == '=') return {ActionKind::CandidateMoveDown, 0, 0};

  // Chinese/HANIME body/composition mode.
  const char c = normalizeAsciiLetter(key.ascii);
  if (c >= 'a' && c <= 'z') {
    return {ActionKind::ImeAppendToken, c, 0};
  }

  if (c >= '0' && c <= '9') {
    if (candidates && c >= '1' && c <= '6') {
      return {ActionKind::CandidateChooseVisibleSlot, 0,
              static_cast<std::uint8_t>(c - '1')};
    }
    // Digits are never double-pinyin tokens. While composing, a numbered key
    // only chooses a slot that the current candidate row actually exposes;
    // the executor rejects an out-of-range slot without mutating composition.
    // Other digits are consumed here so they cannot leak into the body.
    if (composing) return {};
    return {ActionKind::BodyInsertAscii, c, 0};
  }

  // Punctuation does not mutate an unfinished no-candidate composition. A
  // valid candidate composition still uses the existing explicit commit key;
  // after commit, punctuation enters through this body path. This keeps one
  // semantic action per physical key in v0.1.
  if (!composing && isPrintableAscii(key.ascii)) {
    return {ActionKind::BodyInsertChinesePunctuation, key.ascii, 0};
  }

  return {};
}

}  // namespace editor::input
