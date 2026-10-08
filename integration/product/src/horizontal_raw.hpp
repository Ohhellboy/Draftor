#pragma once
#include "device_ime.hpp"
#include "ime_composition_font.hpp"

namespace s3app {
struct HorizontalRawPort {
  void (*ink)(void*,int x,int y)=nullptr;
  void (*accent)(void*,int x,int y,unsigned width,unsigned height)=nullptr;
  void* context=nullptr;
};
// Synchronous presentation only. The existing raw viewport still chooses bytes,
// syllable gaps and edit-target visibility using the shared, larger advances.
void paintHorizontalRaw(const ime_device::DeviceIme&,int composition_y,
                        const HorizontalRawPort&) noexcept;
}
