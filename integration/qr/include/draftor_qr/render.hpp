// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "qr.hpp"
#include <climits>
namespace draftor::qr {
struct Rect { int x, y, width, height; };
struct Geometry { int x = 0, y = 0, scale = 0, pixels = 0, quiet = 4; };
inline Geometry fit(Matrix m, Rect r, int quiet = 4) noexcept {
 if (!m.valid() || quiet < 4 || quiet > 32 || r.x < 0 || r.y < 0 ||
     r.width <= 0 || r.height <= 0 || r.x > INT_MAX - r.width || r.y > INT_MAX - r.height) return {};
 const int edge = m.side + 2 * quiet;
 const int scale = (r.width < r.height ? r.width : r.height) / edge;
 if (!scale) return {};
 const int pixels = edge * scale;
 return {r.x + (r.width - pixels) / 2, r.y + (r.height - pixels) / 2, scale, pixels, quiet};
}
// Callback writes into the existing product framebuffer. ink=false is white.
// Coordinates are screen geometry, never text-layout or punctuation geometry.
template<class Fill> bool paint(Matrix m, Rect r, Fill fill, int quiet = 4) {
 const auto g = fit(m, r, quiet);
 if (!g.scale) return false;
 fill(g.x, g.y, g.pixels, g.pixels, false);
 for (int y = 0; y < m.side; ++y) for (int x = 0; x < m.side; ++x)
  if (m.module(x, y)) fill(g.x + (x + quiet) * g.scale, g.y + (y + quiet) * g.scale, g.scale, g.scale, true);
 return true;
}
}
