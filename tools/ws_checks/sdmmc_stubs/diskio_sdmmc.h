#pragma once
#include "esp_vfs_fat.h"
void ff_diskio_register_sdmmc(BYTE,sdmmc_card_t*);
void ff_sdmmc_set_disk_status_check(BYTE,bool);
