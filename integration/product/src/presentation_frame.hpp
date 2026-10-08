#pragma once
#include "vertical_ime.hpp"
namespace ime_device {
struct PresentationPublication { bool accepted=false;std::uint32_t candidates=0; };
// Caller has completed drawing and a successful transfer/reuse of this exact
// frame. A retired layout receipt cannot publish geometry or candidate pixels.
// The candidate semantic generation is never manufactured from a layout epoch.
inline PresentationPublication publishActivePresentation(s3app::App& app,DeviceIme* ime,
    s3app::PresentationFrame frame,const VerticalImeFrame& vertical={}) noexcept {
  if(!app.presentationFramePresented(frame))return {};
  return {true,app.horizontalImeSurface()?
    publishVisibleCandidate(ime,&app):publishVerticalFrame(ime,app,vertical)};
}
} // namespace ime_device
