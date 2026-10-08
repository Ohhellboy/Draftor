// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#include "../integration/product/platform/st7306_osptek_lpm8_adapter.hpp"
#include <array>
#include <cassert>
int main() {
 using namespace st7306_osptek_lpm8;
 std::array<unsigned char,kFramebufferSize> pixels{};
 fillFramebuffer(pixels.data(),Color::White);
 for (auto c : {Color::Off,Color::Red,Color::White,Color::Black})
  assert(wireColorToLogical(logicalColorToWire(c))==c);
 for (int y=0;y<kPanelHeight;++y) for(int x=0;x<kPanelWidth;++x)
  assert(packedPixelForLogical(x,y).byte_index<pixels.size());
}
