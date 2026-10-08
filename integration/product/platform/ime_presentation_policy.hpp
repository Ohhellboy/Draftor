#pragma once
#include "hanime_extension/navigation.hpp"
#include <algorithm>
#include <cstdint>

namespace s3app {
// Borrow the published path. Adjacent Unknown units are one unresolved row;
// this grouping never changes the navigator, its boundaries or selection.
inline std::size_t presentationSpanEnd(const hanime_extension::NavigationSpan* spans,
    std::size_t count,std::size_t& index) noexcept {
  auto end=spans[index].end;
  if(spans[index].kind==hanime_extension::NavigationKind::Unknown)
    while(index+1<count&&spans[index+1].begin==end&&
        (spans[index+1].kind==hanime_extension::NavigationKind::Unknown||
         spans[index+1].kind==hanime_extension::NavigationKind::IncompleteTail))end=spans[++index].end;
  return end;
}
// No platform, floating-point, allocation or PSRAM dependency.
struct IdealCaretAnchor {
  std::uint16_t numerator=0,denominator=1;
  bool valid=false;
  void observe(std::uint16_t slot,std::uint16_t capacity) noexcept {
    if(!capacity){valid=false;return;}
    if(capacity==1&&valid)return;
    numerator=std::min(slot,static_cast<std::uint16_t>(capacity-1));
    denominator=capacity>1?capacity-1:1;valid=true;
  }
  bool quantize(std::uint16_t capacity,std::uint16_t& slot) const noexcept {
    if(!valid||!denominator||!capacity)return false;
    slot=static_cast<std::uint16_t>((std::uint64_t(numerator)*(capacity-1)+denominator/2)/denominator);
    return slot<capacity;
  }
};
// A bookmark belongs only to its target presentation's logical row/column
// space. No source geometry, pixel coordinate or cross-mode fraction enters
// this policy. Re-entry keeps that target's first slot if the caret still fits.
// First/stale entry leaves one following row/column for reading context; a
// one-slot viewport necessarily uses its sole slot. Same-mode scroll is owned
// by the existing navigator and never applies this entry policy.
struct TargetViewportAnchor {
  std::uint32_t first=0;
  bool valid=false;
  void observe(std::uint32_t target_first) noexcept {first=target_first;valid=true;}
  std::uint16_t entrySlot(std::uint32_t caret,std::uint16_t capacity,bool current)const noexcept {
    if(current&&valid&&caret>=first&&caret-first<capacity)
      return static_cast<std::uint16_t>(caret-first);
    return capacity>1?capacity-2:0;
  }
};
} // namespace s3app
