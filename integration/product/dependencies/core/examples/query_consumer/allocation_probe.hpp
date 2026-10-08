#pragma once
#include <cstddef>
namespace query_probe {
struct Result {std::size_t current,peak,allocations;bool enabled;};
void start();
Result stop();
}
