#pragma once

#include "find_input.hpp"
#include "hanime_document_adapter.hpp"

#include <cstdint>

namespace hanime::find::input {

enum class DispatchResult : std::uint8_t {
  Handled,
  PassToExistingImeOrBody,
  RequestImeCancel,
  IgnoredStale,
  Rejected,
};

// Focus-aware semantic controller. It executes real query/document operations;
// the integration shell remains responsible only for existing IME actions,
// display damage notification, user-activity notification, and cooperative
// calls to FindSession::step().
class FindController {
 public:
  FindController(FindSession& session,
                 integration::HanimeDocumentAdapter& document) noexcept
      : session_(session), document_(document) {}

  InputContext context(bool composition_active,
                       bool direct_input) const noexcept;
  KeyEvent stamp(KeyKind kind, char ascii, Modifier modifiers) const noexcept;
  DispatchResult dispatch(const KeyEvent& event,
                          bool composition_active,
                          bool direct_input) noexcept;
  void onDocumentSwitch() noexcept;
  bool closeForExternalFocus(bool composition_active) noexcept;
  void invalidateQueuedInput() noexcept;

  Focus focus() const noexcept { return focus_; }
  std::uint32_t focusGeneration() const noexcept { return focus_generation_; }

 private:
  void changeFocus(Focus focus) noexcept;

  FindSession& session_;
  integration::HanimeDocumentAdapter& document_;
  Focus focus_ = Focus::Body;
  std::uint32_t focus_generation_ = 1U;
};

}  // namespace hanime::find::input
