#pragma once
#include "board_config.hpp"
#if DRAFTOR_BOARD == WAVESHARE_RLCD42
#include "ST7305_Waveshare.h"
using BoardDisplay=ST7305Waveshare;
#else
#include "ST7306_OsptekLpm8.h"
using BoardDisplay=ST7306OsptekLpm8;
#endif
