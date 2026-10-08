#pragma once
#include "diskio.h"
#define FF_DRV_NOT_USED 0xff
constexpr int ESP_OK=0;
extern "C" {
struct ff_diskio_impl_t {
 DSTATUS (*init)(BYTE); DSTATUS (*status)(BYTE);
 DRESULT (*read)(BYTE,BYTE*,DWORD,UINT); DRESULT (*write)(BYTE,const BYTE*,DWORD,UINT);
 DRESULT (*ioctl)(BYTE,BYTE,void*);
};
void ff_diskio_register(BYTE,const ff_diskio_impl_t*);
int ff_diskio_get_drive(BYTE*);
}
#define ff_diskio_unregister(p) ff_diskio_register(p,nullptr)
