#include "draftor/vertical_raster.hpp"
#include "draftor/vertical_punctuation.hpp"

namespace draftor::vertical {

PaintResult paint(const Viewport& viewport, const Stamp& current,
                  const RasterPort& port) noexcept {
  PaintResult result;
  result.status = viewport.status(current);
  if (result.status != Status::Ready) return result;
  if (!port.glyph || !port.ink || port.clip.width == 0 || port.clip.height == 0) {
    result.status = Status::InvalidGeometry;
    return result;
  }
  for (std::size_t i = 0; i < viewport.eventCount(); ++i) {
    const auto event = viewport.eventAt(i);
    if (!event.glyph() || !viewport.visible(event.column)) continue;
    auto form = event.kind == EventKind::Upright ? punctuationForm(event.codepoint, port.punctuation_policy) : PunctuationForm{};
    auto glyph = form.native ? port.glyph(port.font_context, form.native) : Bitmap{};
    if (glyph.bits && !glyph.missing) {
      form = {};
    } else {
      glyph = port.glyph(port.font_context, form.fallback ? form.fallback : event.codepoint);
    }
    const bool rotate = event.kind == EventKind::RotateCW90 || form.rotate;
    if (!glyph.bits || glyph.width == 0 || glyph.height == 0 ||
        glyph.stride < (static_cast<unsigned>(glyph.width) + 7U) / 8U ||
        glyph.bytes / glyph.stride < glyph.height) {
      result.status = Status::InvalidGeometry;
      return result;
    }
    const auto box = viewport.glyphRect(event);
    ++result.glyphs;
    // Rotating each bitmap in its original advance slot is exactly the rigid
    // CW90 transform of the whole horizontal run (shared baseline, no kerning).
    // It does not give each Latin letter a separate Chinese character cell.
    for (std::uint16_t row = 0; row < glyph.height; ++row) {
      for (std::uint16_t col = 0; col < glyph.width; ++col) {
        ++result.pixels_tested;
        if ((glyph.bits[static_cast<std::size_t>(row) * glyph.stride + col / 8U] &
             (0x80U >> (col % 8U))) == 0) continue;
        const int x = form.x + (rotate
            ? box.x + glyph.height - 1 - row : box.x + glyph.bearing_px + col);
        const int y = form.y + (rotate
            ? box.y + glyph.bearing_px + col : box.y + row);
        if (x < port.clip.x || y < port.clip.y ||
            x >= static_cast<int>(port.clip.x) + port.clip.width ||
            y >= static_cast<int>(port.clip.y) + port.clip.height) continue;
        port.ink(port.pixel_context, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
        ++result.pixels_inked;
      }
    }
  }
  return result;
}

}  // namespace draftor::vertical
