#include "stack_probe.hpp"
#include <cstdint>
namespace {
bool measuring=false;
std::uintptr_t low=0,high=0;
}
extern "C" void __cyg_profile_func_enter(void*,void*) __attribute__((no_instrument_function));
extern "C" void __cyg_profile_func_exit(void*,void*) __attribute__((no_instrument_function));
extern "C" void __cyg_profile_func_enter(void*,void*) {
  if(measuring) {
    char marker;
    const auto address=reinterpret_cast<std::uintptr_t>(&marker);
    if(address<low)low=address;
    if(address>high)high=address;
  }
}
extern "C" void __cyg_profile_func_exit(void*,void*) {}
namespace stack_probe {
void start() {char marker;low=high=reinterpret_cast<std::uintptr_t>(&marker);measuring=true;}
void stop() {measuring=false;}
std::size_t span() {return static_cast<std::size_t>(high-low);}
}
