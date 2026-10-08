#pragma once
#include "hid_keyboard.hpp"
#include <cstdint>
namespace keyboard {
enum class Phase:uint8_t{Starting,Scanning,Connecting,Pairing,AwaitRelease,Ready,Disconnected,Cleaning,Error};
struct Status {Phase phase;uint32_t passkey,epoch,received,overflow,bad_reports,queue_peak;bool pairing_not_saved=false;};
bool begin();
uint32_t workerStackMinimumBytes(); // Owned BLE coordinator only; not all NimBLE tasks.
bool pop(s3hid::Event&);
// Linearizes raw-event admission against invalidate under the same stateMux.
// An admitted event may finish after invalidation; App command ownership is separate.
bool admit(const s3hid::Event&,uint32_t& observedEpoch);
bool pending();
void publishFrame(uint32_t route_epoch,uint32_t generation);
void setImeAttempt(uint32_t serial);
void publishImeContext(uint32_t serial,uint32_t resource_owner);
Status status();
const char* phaseName(Phase);
}
