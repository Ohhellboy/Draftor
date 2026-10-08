#pragma once
#include <cstddef>
namespace allocation_probe {
struct Stats { std::size_t live, peak, requested, allocations; };
void reset_peak();
Stats stats();
bool fail_after(std::size_t successful_allocations);
void clear_failure();
}
