#pragma once
#include "hid_keyboard.hpp"
namespace usb_causal {
enum Action { Pass,PhysicalF9,AlternateUsb };
inline Action classify(const s3hid::Event& e)noexcept{
 if(e.usage==66)return PhysicalF9;
 // Ctrl+Shift+U, either side; exclude Alt/GUI. Only a down opens the menu.
 if(e.down&&e.usage==24&&(e.mods&0x11)&&(e.mods&0x22)&&!(e.mods&0xcc))return AlternateUsb;
 return Pass;
}
inline s3hid::Event originalUsbCommand(const s3hid::Event& e)noexcept{
 auto command=e;command.usage=66;command.mods=0;return command;
}
}
