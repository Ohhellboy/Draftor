#pragma once
#include "ff.h"
int esp_vfs_fat_register(const char*,const char*,unsigned,FATFS**);
int esp_vfs_fat_unregister_path(const char*);
