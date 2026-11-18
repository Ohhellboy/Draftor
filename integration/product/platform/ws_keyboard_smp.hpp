#pragma once
#include <cstdint>

// One worker, one pending Central dial. Existing-key encryption uses the
// controller, without any outbound SMP pairing or key-distribution messages.
namespace ws_smp {
void start(bool fresh);
bool bind(uint16_t handle, bool fresh);
bool permitsFresh(uint16_t handle);
void close();
unsigned refused();
void onRefused(); // Host-lock context: revoke locally, do not call GAP or log.
}
