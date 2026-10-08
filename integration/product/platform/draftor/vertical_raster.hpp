#pragma once

#include "draftor/vertical_layout.hpp"
#include "draftor/vertical_punctuation.hpp"

namespace draftor::vertical {

struct Bitmap {
  const std::uint8_t* bits = nullptr;  // MSB-first, borrowed until next lookup.
  std::uint16_t width = 0, height = 0, stride = 0;
  std::int16_t bearing_px = 0;
  std::size_t bytes = 0;  // Available bitmap storage; checked before reading.
  bool missing = false; // Propagate FontPort::GlyphView::missing for native fallback.
};

struct RasterPort {
  Bitmap (*glyph)(void*, std::uint32_t) = nullptr;
  void* font_context = nullptr;
  void (*ink)(void*, std::uint16_t, std::uint16_t) = nullptr;
  void* pixel_context = nullptr;
  Rect clip{};
  Bitmap (*small_glyph)(void*, std::uint32_t) = nullptr; // Optional fixed 14px font for A/numbers.
  PunctuationPolicy punctuation_policy = PunctuationPolicy::Legacy;
};

struct PaintResult {
  Status status = Status::Ready;
  std::size_t glyphs = 0, pixels_tested = 0, pixels_inked = 0;
};

// No frame buffer, TF access or retained glyph pointers. Platform owns glyph
// fallback and pixel colors. Fill highlight rectangles before this call and
// choose normal/inverse ink according to the same real byte-range mapping.
PaintResult paint(const Viewport& viewport, const Stamp& current,
                  const RasterPort& port) noexcept;

}  // namespace draftor::vertical
