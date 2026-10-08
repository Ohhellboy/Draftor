#pragma once
#include <cstdint>
constexpr int ESP_OK = 0, USB_INTERFACE_MSC = 1, TUD_MSC_DESC_LEN = 23, CFG_TUD_ENDPOINT_SIZE = 64;
constexpr std::uint8_t SCSI_SENSE_MEDIUM_ERROR = 3, SCSI_SENSE_DATA_PROTECT = 7,
  SCSI_SENSE_NOT_READY = 2, SCSI_SENSE_ILLEGAL_REQUEST = 5, SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL = 0x1e;
#define TUD_MSC_DESCRIPTOR(i,s,e,o,n) std::uint8_t(i), std::uint8_t(s), std::uint8_t(e), std::uint8_t(o), std::uint8_t(n)
inline std::uint8_t tinyusb_add_string_descriptor(const char*) { return 1; }
inline std::uint8_t tinyusb_get_free_duplex_endpoint() { return 1; }
inline int tinyusb_enable_interface(int, int, std::uint16_t (*cb)(std::uint8_t*, std::uint8_t*)) {
  std::uint8_t bytes[64]{}, itf = 0; return cb(bytes, &itf) ? ESP_OK : -1;
}
extern "C" bool tud_msc_set_sense(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t);
