#pragma once
#include "esp_vfs_fat.h"
constexpr BYTE FF_DRV_NOT_USED=255;
int ff_diskio_get_drive(BYTE*);
void ff_diskio_unregister(BYTE);
