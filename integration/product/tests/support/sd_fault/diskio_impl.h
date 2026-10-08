#pragma once
#include "diskio.h"
typedef struct {DSTATUS(*init)(uint8_t);DSTATUS(*status)(uint8_t);DRESULT(*read)(uint8_t,uint8_t*,DWORD,UINT);DRESULT(*write)(uint8_t,const uint8_t*,DWORD,UINT);DRESULT(*ioctl)(uint8_t,uint8_t,void*);} ff_diskio_impl_t;
int ff_diskio_get_drive(uint8_t*);
void ff_diskio_register(uint8_t,const ff_diskio_impl_t*);
