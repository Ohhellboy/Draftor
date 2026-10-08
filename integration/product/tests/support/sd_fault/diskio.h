#pragma once
#include "ff.h"
typedef uint8_t DSTATUS;typedef int DRESULT;
#define STA_NOINIT 1
#define STA_PROTECT 4
#define RES_OK 0
#define RES_ERROR 1
#define RES_WRPRT 2
#define RES_NOTRDY 3
#define RES_PARERR 4
#define CTRL_SYNC 0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE 2
#define GET_BLOCK_SIZE 3
