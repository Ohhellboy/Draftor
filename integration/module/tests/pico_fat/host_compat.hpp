#pragma once
#include <cstdint>
class __FlashStringHelper;
inline uint32_t millis() { static uint32_t tick=0; return ++tick; }
