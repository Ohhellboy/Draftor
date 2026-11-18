#pragma once

#include <cstdint>

class BLEClient;

namespace keyboard::input {
struct Stats {
    uint32_t epoch;
    uint32_t received;
    uint32_t overflow;
    uint32_t bad;
    uint32_t peak;
    bool awaitRelease;
};

bool begin();
void connected(uint16_t handle);
void invalidate();
// Only the connection worker performs synchronous GATT operations.
bool prepare(BLEClient& client);
// Enable delivery only after the worker has accepted this connection and its bond.
bool activate(uint16_t handle);
Stats stats();
}
