#pragma once
// Host callback harness only. The isolated S3 build uses the actual SDK headers.
#define ARDUINO_USB_MODE 0
#define ARDUINO_USB_MSC_ON_BOOT 0
#define ARDUINO_USB_DFU_ON_BOOT 0
struct HostUsb { void productName(const char*) {} bool begin() { return true; } };
inline HostUsb USB;
