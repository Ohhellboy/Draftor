#pragma once
#include "vertical_ime.hpp"
#include "ui16_font.hpp"
namespace s3app::find_ui {
// Shares the physical framebuffer with the editor. No display-sized allocation.
struct PaintPort { void(*pixel)(void*,int,int,bool)=nullptr;void* context=nullptr; };
struct Receipt {
 App::QueryStamp query{};
 uint64_t input=0,revision=0;
 uint32_t generation=0;
 size_t row=0;
 bool complete=false,candidates=false;
};
// Drawing primitive only: it never grants an IME selection identity.
bool wholeCandidate(std::string_view,bool vertical,const PaintPort&) noexcept;
Receipt paint(App&,ime_device::DeviceIme*,const PaintPort&) noexcept;
uint32_t publish(App&,ime_device::DeviceIme*,const Receipt&) noexcept;
}
