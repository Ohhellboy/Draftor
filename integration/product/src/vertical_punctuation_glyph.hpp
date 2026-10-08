#pragma once
#include <array>
#include <cstdint>
namespace ime_device {
// One necessary 28x28 glyph in the existing MSB-first bitmap format. Two
// adjacent U+2014 cells join at the baseline's centre without changing advance.
// U+FE31 is requested only by display; keyboard/body storage stays U+2014.
inline constexpr auto verticalDash=[](){
 std::array<std::uint8_t,112> bits{};
 for(unsigned y=0;y<28;++y)bits[y*4+1]=0x06; // x=13,14
 return bits;
}();
}
