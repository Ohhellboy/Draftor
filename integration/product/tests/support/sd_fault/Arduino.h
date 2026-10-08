#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstddef>
constexpr int HIGH=1,LOW=0,OUTPUT=1;
namespace sd_fault {extern uint32_t ms,transfers;extern uint8_t response;extern int cs;}
inline uint32_t millis(){return sd_fault::ms;}
inline void delay(uint32_t n){sd_fault::ms+=n;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int value){sd_fault::cs=value;}
inline uint8_t digitalPinToGPIONumber(uint8_t pin){return pin;}
template<class... Args> inline void faultLog(const char*,Args...){}
#define log_w(...) faultLog(__VA_ARGS__)
#define log_e(...) faultLog(__VA_ARGS__)
#define log_d(...) faultLog(__VA_ARGS__)
#define ARDUHAL_LOG_LEVEL 1
#define ARDUHAL_LOG_LEVEL_ERROR 1

// Bare strict-newlib headers hide POSIX strdup; Arduino VFS supplies it.
extern "C" char* strdup(const char*);
