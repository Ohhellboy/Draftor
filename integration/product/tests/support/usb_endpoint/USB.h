#pragma once
#include <cstdint>
#define ARDUINO_USB_MODE 0
#define ARDUINO_USB_MSC_ON_BOOT 0
#define ARDUINO_USB_DFU_ON_BOOT 0
struct UsbTestState {unsigned begins{},interfaces{};bool begin_ok=true;};
extern UsbTestState usb_test;
struct UsbFake {void productName(const char*){} bool begin(){++usb_test.begins;return usb_test.begin_ok;}};
extern UsbFake USB;
