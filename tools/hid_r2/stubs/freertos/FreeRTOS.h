#pragma once
#include <cassert>
struct portMUX_TYPE{unsigned held;};
#define portMUX_INITIALIZER_UNLOCKED {0}
extern unsigned probe_lock_held;
#define portENTER_CRITICAL(p) do{assert(!(p)->held);(p)->held=1;++probe_lock_held;}while(0)
#define portEXIT_CRITICAL(p) do{assert((p)->held);(p)->held=0;--probe_lock_held;}while(0)
