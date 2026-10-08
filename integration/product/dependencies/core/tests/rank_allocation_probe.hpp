#pragma once
#include <cstddef>
namespace rank_probe {
struct Statistics { std::size_t current,peak,allocations; bool supported; };
void start();
Statistics stop();
bool fail_next_allocation(); // Ordinary Host only; one controlled bad_alloc.
// Only rank calls / caller-buffer construction are captured; not the full
// oracle, fixture construction, log formatting or test assertion strings.
struct Capture {
  Capture();
  ~Capture();
  bool previous;
};
}
