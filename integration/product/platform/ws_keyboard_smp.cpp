#include "ws_keyboard_smp.hpp"
#include <atomic>
#include <host/ble_hs.h>
#include <os/os_mbuf.h>

namespace ws_smp {
namespace {
constexpr uint32_t Closed = 0x10000u, NewDial = 0x10001u;
std::atomic<uint32_t> permitted{Closed};
std::atomic<unsigned> failures{0};
}
void start(bool fresh) {
  failures = 0;
  // An observed link must pass bond/OTA/role checks before any fresh exchange.
  permitted = fresh ? NewDial : Closed;
}
bool permitsFresh(uint16_t handle) {
  return handle != BLE_HS_CONN_HANDLE_NONE && permitted.load() == handle;
}
bool bind(uint16_t handle, bool fresh) {
  if (handle == BLE_HS_CONN_HANDLE_NONE) return false;
  if (!fresh) return permitted.load() == Closed;
  uint32_t expected = NewDial;
  return permitted.compare_exchange_strong(expected, handle) || expected == handle;
}
void close() { permitted = Closed; }
unsigned refused() { return failures.load(); }
void denied() { ++failures; close(); onRefused(); }
}

extern "C" int __real_ble_sm_tx(uint16_t, os_mbuf*);
extern "C" int __wrap_ble_sm_tx(uint16_t handle, os_mbuf* packet) {
  uint8_t opcode = 0;
  // The fixed stack calls this with its host mutex held. Its caller transfers
  // ownership of the entire mbuf chain, including on errors.
  if (packet && os_mbuf_copydata(packet, 0, 1, &opcode) == 0 &&
      (opcode == 0x05 /* SMP Pairing Failed */ || ws_smp::permitsFresh(handle))) {
    return __real_ble_sm_tx(handle, packet);
  }
  if (packet) os_mbuf_free_chain(packet);
  ws_smp::denied();
  return BLE_HS_EAUTHEN;
}
