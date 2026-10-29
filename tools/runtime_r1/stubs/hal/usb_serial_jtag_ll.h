#pragma once
#include <cstdint>
uint32_t usb_serial_jtag_ll_write_txfifo(const uint8_t*,uint32_t);
void usb_serial_jtag_ll_txfifo_flush();
