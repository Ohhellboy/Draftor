#pragma once

#include "find_session.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hanime::find::input {

enum class Modifier : std::uint8_t {
  None = 0,
  Shift = 1U << 0U,
  Ctrl = 1U << 1U,
  Alt = 1U << 2U,
  Gui = 1U << 3U,
};

constexpr Modifier operator|(const Modifier lhs, const Modifier rhs) noexcept {
  return static_cast<Modifier>(static_cast<std::uint8_t>(lhs) |
                               static_cast<std::uint8_t>(rhs));
}

enum class KeyKind : std::uint8_t {
  Character,
  Enter,
  Escape,
  Backspace,
  DeleteForward,
  Left,
  Right,
  Up,
  Down,
  Home,
  End,
  Space,
  ToggleInputMode,
};

struct KeyEvent {
  KeyKind kind = KeyKind::Character;
  char ascii = 0;
  Modifier modifiers = Modifier::None;
  std::uint32_t focus_generation = 0;
  std::uint32_t find_session_id = 0;
};

enum class Focus : std::uint8_t { Body, FindBar };

struct InputContext {
  Focus focus = Focus::Body;
  std::uint32_t focus_generation = 0;
  std::uint32_t find_session_id = 0;
  bool composition_active = false;
  bool direct_input = false;
};

enum class Command : std::uint8_t {
  PassToExistingInput,
  ConsumeUnsupported,
  IgnoreStale,
  OpenFind,
  CloseFind,
  CancelComposition,
  SearchForward,
  SearchBackward,
  QueryBackspace,
  QueryDeleteForward,
  QueryMoveLeft,
  QueryMoveRight,
  QueryMoveHome,
  QueryMoveEnd,
  QueryInsertAscii,
  ToggleCaseSensitivity,
  MoveDocumentStart,
  MoveDocumentEnd,
};

struct RoutedCommand {
  Command command = Command::PassToExistingInput;
  char ascii = 0;
};

class ProductFindInputRouter {
 public:
  static RoutedCommand route(const InputContext& context,
                             const KeyEvent& event) noexcept;
};

enum class CommitResult : std::uint8_t {
  Prepared,
  Committed,
  Rejected,
  Stale,
  NoPreparedCommit,
};

// Concrete IME submission target for the find bar. The host asks it to stage
// the exact candidate before releasing the IME candidate, then commits the
// exact returned text. This path writes only FindSession's bounded query.
class FindQueryCommitTarget final {
 public:
  explicit FindQueryCommitTarget(FindSession& session) noexcept
      : session_(session) {}

  CommitResult prepare(std::string_view utf8) noexcept;
  CommitResult commit(std::string_view committed_utf8) noexcept;
  void discard() noexcept { prepared_ = false; }
  bool prepareCommit(std::string_view utf8) noexcept {
    return prepare(utf8) == CommitResult::Prepared;
  }
  bool finishCommit(std::string_view utf8) noexcept {
    return commit(utf8) == CommitResult::Committed;
  }
  void discardCommit() noexcept { discard(); }
  std::size_t stagingBytes() const noexcept { return staging_.size(); }

 private:
  FindSession& session_;
  std::array<char, kHardMaximumQueryBytes> staging_{};
  std::size_t staged_size_ = 0;
  std::uint32_t expected_session_id_ = 0;
  std::uint32_t expected_query_version_ = 0;
  std::size_t expected_query_cursor_ = 0;
  bool prepared_ = false;
};

}  // namespace hanime::find::input
