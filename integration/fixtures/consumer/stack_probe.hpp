#pragma once
#include <cstddef>
namespace stack_probe {
void start() __attribute__((no_instrument_function));
void stop() __attribute__((no_instrument_function));
std::size_t span() __attribute__((no_instrument_function));
}
