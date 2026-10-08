#pragma once
#include <stdint.h>
#define FF_VOLUMES 2
#define FF_MAX_SS 512
#define FM_ANY 0
#define FR_OK 0
typedef uint8_t BYTE;typedef uint16_t WORD;typedef uint32_t DWORD;typedef unsigned UINT;
typedef int FRESULT;typedef struct {int unused;} FATFS;
typedef struct {BYTE fmt;unsigned n_fat,align,n_root,au_size;} MKFS_PARM;
FRESULT f_mount(FATFS*,const char*,BYTE);
FRESULT f_mkfs(const char*,const MKFS_PARM*,void*,UINT);
