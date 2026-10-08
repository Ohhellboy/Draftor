#pragma once
#include <cstdint>
namespace shim {inline void (*beforeDisable)()=nullptr;}
inline uint32_t save_and_disable_interrupts(){if(shim::beforeDisable){auto f=shim::beforeDisable;shim::beforeDisable=nullptr;f();}return 0;}
inline void restore_interrupts(uint32_t){}
