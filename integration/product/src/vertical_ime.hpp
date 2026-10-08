#pragma once
#include "ime_presentation.hpp"
#include "draftor/vertical_ab.hpp"

namespace ime_device {
const char* verticalStatus(const DeviceIme&) noexcept;
bool prepareImePresentation(DeviceIme*,s3app::App&,bool incremental=false) noexcept;

// Small receipt of one synchronous paint, never a copied candidate frame.
// Does not grant authority. The existing Window must publish successful pixels.
struct VerticalImeFrame {
  draftor::vertical::AbResult layout{};
  std::uint64_t input=0,revision=0;
  std::uint32_t generation=0;
  std::size_t row=0;
  // Selection authority only; a current empty surface can still be published.
  bool candidate_frame=false;
};
draftor::vertical::AbCandidateView verticalCandidateView(
    const hanime_extension::Snapshot&,const CandidateView&) noexcept;
VerticalImeFrame verticalIme(DeviceIme*,const s3app::App&,std::uint32_t now,
    std::uint32_t active_body_column,const draftor::vertical::AbPort&) noexcept;
// Explicit display completion call; failed body/AB/LCD must never call it.
std::uint32_t publishVerticalFrame(DeviceIme*,const s3app::App&,
    const VerticalImeFrame&) noexcept;
struct VerticalChoiceTarget {
  draftor::vertical::AbRole role=draftor::vertical::AbRole::Preview;
  unsigned number=0;
  VerticalImeFrame frame{};
};
// Test/consumer action adapter; uses the original Window selection path. Device
// HID digit/Space submission continues through the unchanged DeviceIme path.
ChoiceResult chooseVertical(CandidateWindow&,const VerticalChoiceTarget&,
    std::uint32_t action=0);
// R1 semantic column browsing. Caller supplies the current owner/focus action;
// these explicit consumer commands are not a product HID/runtime mode switch.
draftor::vertical::EdgeSide browseVerticalCandidates(CandidateWindow&,bool left,
    std::uint32_t action=0);
draftor::vertical::EdgeSide verticalCandidateKey(CandidateWindow&,char,
    std::uint32_t action=0);
enum class VerticalDispatch : std::uint8_t { Delivered, RejectedTarget, ExpiredFrame };
// Explicit numbered-selection consumer: retain P23's real HID -> Submission
// -> EditorSink path, but never let a retired Vertical receipt enter that path.
VerticalDispatch dispatchVerticalNumber(DeviceIme&,const s3app::App&,
    const VerticalChoiceTarget&,const s3hid::Event&);
} // namespace ime_device
