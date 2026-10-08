#pragma once
#include "USB.h"
#define TUD_MSC_DESCRIPTOR(itf,str,out,in,size) 9,(itf),(str),(out),(in),(size),0,0
#define TUD_MSC_DESC_LEN 8
#define CFG_TUD_ENDPOINT_SIZE 64
#define USB_INTERFACE_MSC 1
#define ESP_OK 0
inline std::uint8_t tinyusb_add_string_descriptor(const char*){return 1;}
inline std::uint8_t tinyusb_get_free_duplex_endpoint(){return 1;}
inline int tinyusb_enable_interface(int,int,std::uint16_t(*descriptor)(std::uint8_t*,std::uint8_t*)){++usb_test.interfaces;std::uint8_t out[64]{},i=0;return descriptor(out,&i)?0:1;}
