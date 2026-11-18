#pragma once
#include <host/ble_gap.h>
namespace ws_privacy {
struct Peer { ble_addr_t identity; uint8_t irk[16]; };
struct PeerList { unsigned count; Peer entries[3]; };
void dirty();
void hostReset();
void hostSynced();
bool current();
// Sole keyboard worker, while GAP is idle. Returns a NimBLE status.
int sync();
const char* stage();
}
