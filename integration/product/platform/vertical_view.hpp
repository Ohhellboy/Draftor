#pragma once
#include "vertical_presentation.hpp"
#include "vertical_ime.hpp"

namespace s3app {
enum class VerticalColor : std::uint8_t { White, Black, Red };
struct VerticalPaintPort {
  void (*rect)(void*,draftor::vertical::Rect,VerticalColor)=nullptr;
  void (*ink)(void*,std::uint16_t,std::uint16_t,VerticalColor)=nullptr;
  void* context=nullptr;
  // Optional synchronous geometry observer, no retained borrowed text/identity.
  void (*ab_run)(void*,const draftor::vertical::AbRun&)=nullptr;
};
struct VerticalPaintResult {
  draftor::vertical::Status status=draftor::vertical::Status::Ready;
  ime_device::VerticalImeFrame ime{};
  std::size_t glyphs=0;
};
// Explicit consumer entry for Host/target compile tests. Caller serializes all
// shared/IME mutation against this call, then owns successful display completion.
VerticalPaintResult paintVerticalView(const App&,VerticalPresentation&,
    ime_device::DeviceIme*,const s3host::FontPort&,std::uint32_t now,
    const VerticalPaintPort&) noexcept;
} // namespace s3app
