#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace st7306_osptek_lpm8 {
// Keep the native column range 05..36. Two native rows (300 bytes) are the
// smallest unit here so every source offset and writeBytes length is a multiple
// of four, as required by the pinned Arduino spiWriteNL uint32_t reader.
constexpr std::size_t kDeltaTileBytes = 300;
constexpr unsigned kDeltaTiles = 100;
constexpr unsigned kDeltaMaxBands = 8;
constexpr unsigned kWindowWireBytes = 7; // 2A+2 params, 2B+2 params, 2C.
struct NativeBand { std::uint8_t first_tile=0, last_tile=0; };
struct DeltaPlan {
  NativeBand bands[kDeltaMaxBands]{};
  std::uint32_t compare_span_bytes=0, payload_bytes=0;
  std::uint8_t count=0;
  bool full=false;
};
inline DeltaPlan fullDeltaPlan() {
  DeltaPlan p;
  p.bands[0]={0,kDeltaTiles-1};p.count=1;p.full=true;
  p.payload_bytes=kDeltaTiles*kDeltaTileBytes;
  return p;
}
inline DeltaPlan planDelta(const std::uint8_t* current,
                          const std::uint8_t* completed) {
  if(!completed)return fullDeltaPlan();
  DeltaPlan p;
  for(unsigned tile=0;tile<kDeltaTiles;++tile){
    p.compare_span_bytes+=kDeltaTileBytes;
    if(std::memcmp(current+tile*kDeltaTileBytes,
                   completed+tile*kDeltaTileBytes,kDeltaTileBytes)==0)continue;
    if(p.count&&p.bands[p.count-1].last_tile+1U==tile){
      p.bands[p.count-1].last_tile=static_cast<std::uint8_t>(tile);continue;
    }
    if(p.count==kDeltaMaxBands){
      // Keep at most eight ascending bands. Discard the smallest unchanged
      // gap (including the new last gap), retaining the seven largest gaps.
      unsigned merge=p.count-1;
      unsigned gap=tile-p.bands[merge].last_tile-1;
      for(unsigned i=0;i+1<p.count;++i){
        const unsigned next=p.bands[i+1].first_tile-p.bands[i].last_tile-1;
        if(next<gap){gap=next;merge=i;}
      }
      if(merge==p.count-1){p.bands[merge].last_tile=static_cast<std::uint8_t>(tile);continue;}
      p.bands[merge].last_tile=p.bands[merge+1].last_tile;
      for(unsigned i=merge+1;i+1<p.count;++i)p.bands[i]=p.bands[i+1];
      --p.count;
    }
    p.bands[p.count++]={static_cast<std::uint8_t>(tile),static_cast<std::uint8_t>(tile)};
  }
  for(unsigned i=0;i<p.count;++i)
    p.payload_bytes+=(p.bands[i].last_tile-p.bands[i].first_tile+1U)*kDeltaTileBytes;
  if(p.payload_bytes+p.count*kWindowWireBytes>=kDeltaTileBytes*kDeltaTiles+kWindowWireBytes){
    auto full=fullDeltaPlan();full.compare_span_bytes=p.compare_span_bytes;return full;
  }
  return p;
}
} // namespace st7306_osptek_lpm8
