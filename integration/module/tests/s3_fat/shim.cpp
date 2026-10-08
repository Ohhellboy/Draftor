#include "ff.h"
#include "diskio_impl.h"
#include "check.hpp"
#include <array>
static std::array<ff_diskio_impl_t,FF_VOLUMES> drivers{};
extern "C" {
PARTITION VolToPart[FF_VOLUMES]{};
void ff_diskio_register(BYTE p,const ff_diskio_impl_t* impl){CHECK(p<FF_VOLUMES);drivers[p]=impl?*impl:ff_diskio_impl_t{};}
int ff_diskio_get_drive(BYTE* p){for(BYTE i=0;i<FF_VOLUMES;++i)if(!drivers[i].read){*p=i;return ESP_OK;}return -1;}
DSTATUS disk_initialize(BYTE p){return p<FF_VOLUMES&&drivers[p].init?drivers[p].init(p):STA_NOINIT;}
DSTATUS disk_status(BYTE p){return p<FF_VOLUMES&&drivers[p].status?drivers[p].status(p):STA_NOINIT;}
DRESULT disk_read(BYTE p,BYTE* b,LBA_t at,UINT n){return p<FF_VOLUMES&&drivers[p].read?drivers[p].read(p,b,at,n):RES_NOTRDY;}
DRESULT disk_write(BYTE p,const BYTE* b,LBA_t at,UINT n){return p<FF_VOLUMES&&drivers[p].write?drivers[p].write(p,b,at,n):RES_NOTRDY;}
DRESULT disk_ioctl(BYTE p,BYTE c,void* b){return p<FF_VOLUMES&&drivers[p].ioctl?drivers[p].ioctl(p,c,b):RES_NOTRDY;}
DWORD get_fattime(){return (46u<<25)|(10u<<21)|(3u<<16);}
int ff_mutex_create(int){return 1;} void ff_mutex_delete(int){}
int ff_mutex_take(int){return 1;} void ff_mutex_give(int){}
}
