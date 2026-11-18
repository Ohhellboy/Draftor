#pragma once
#include "hid_keyboard.hpp"
#include <cstdint>
namespace keyboard {
enum class PickerMode : uint8_t { Closed, Waiting, Ambiguous, Busy, Confirm };
struct PairingView {
 PickerMode mode=PickerMode::Closed;
 uint32_t revision=0, confirmationToken=0;
 const char* message="";
 // Static stage names are copied by value with the matching code and anonymous slot.
 const char* lastStage=nullptr;
 int lastCode=0;
 unsigned lastSlot=0;
 uint32_t lastAttempt=0, lastPeer=0;
 uint32_t readyAttempt=0;
 unsigned readySlot=0;
};
enum class Phase:uint8_t{Starting,Scanning,Connecting,Pairing,AwaitRelease,Ready,Disconnected,Cleaning,Error,NeedsSelection};
struct Status {Phase phase;uint32_t passkey,epoch,received,overflow,bad_reports,queue_peak;bool pairing_not_saved=false,local_confirmation=false;};
inline constexpr const char* LocalConfirmationHint="Hold BOOT 2s to trust; tap cancels";
bool begin();
// Runtime-only emergency BOOT gesture; ordinary pairing is automatic.
void pairingButton(bool down,uint32_t now);
// UI acknowledges the actual, successful presentation of this exact prompt.
void pairingPresentation(uint32_t confirmation_token,bool eligible_surface,uint32_t now);
bool pairingKey(const s3hid::Event&); // Consume before text and editor shortcuts.
PairingView pairingView();
const char* pairingNotice(); // Static string snapshot; no device/UI ownership.
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
