// Waveshare keyboard receiver. The worker owns every dial and GATT operation.
// GAP callbacks publish observations; the editor receives only admitted input.
#include "ble_keyboard_host.hpp"
#include "ws_keyboard_bonds.hpp"
#include "ws_keyboard_smp.hpp"
#include "ws_keyboard_privacy.hpp"
#include "ws_keyboard_input.hpp"
#include <Arduino.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <host/ble_hs_adv.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs_pvcy.h>
#include <nimble/nimble_port.h>
#include <atomic>
#include <cstring>
#include <new>

namespace keyboard {
namespace {
constexpr uint32_t NoPasskey = 1000000;
constexpr uint32_t ScanMs = 1500;
constexpr uint32_t AuthMs = 60000;
std::atomic<Phase> phase{Phase::Starting};
std::atomic<uint32_t> pin{NoPasskey};
std::atomic<bool> interrupted{false}, pinAllowed{false}, newPair{false}, surfaceReady{false};
TaskHandle_t worker = nullptr;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
PairingView view{};
bool confirmationVisible = false, buttonReleased = false, consent = false;
bool buttonDown = false;
uint32_t buttonSince = 0;

bool same(const ble_addr_t& a, const ble_addr_t& b) {
  return (a.type & 1) == (b.type & 1) && std::memcmp(a.val, b.val, sizeof(a.val)) == 0;
}

void display(PickerMode mode, const char* message, uint32_t token = 0) {
  portENTER_CRITICAL(&lock);
  if (view.mode != mode || view.message != message || view.confirmationToken != token) {
    view.mode = mode;
    view.message = message;
    view.confirmationToken = token;
    ++view.revision;
    confirmationVisible = buttonReleased = consent = false;
  }
  portEXIT_CRITICAL(&lock);
}

// Host/worker events enter a small RAM queue. Only the worker writes UART0.
uint32_t attemptSerial = 0;  // worker-owned
struct AttemptInfo {
  uint32_t serial, started, air, peer;
  unsigned slot;
  bool known;
};
AttemptInfo active{};
bool admittedInput = false;
uint16_t traceHandle = BLE_HS_CONN_HANDLE_NONE;
struct TraceEvent {
  AttemptInfo attempt;
  const char* stage;  // only static strings
  uint32_t elapsed;
  uint16_t handle, detail;
  int rc;
};
TraceEvent events[24]{};
unsigned traceHead = 0, traceCount = 0, traceLost = 0;

// Boot-local opaque aliases; a rotated or aged-out address receives a new
// label. Only known candidates use the resolved member address as peer.
struct AliasEntry { ble_addr_t address; uint32_t label; };
uint32_t peerAlias(const ble_addr_t& address) {
  static AliasEntry recent[16]{};
  static uint32_t next = 0;
  for (const auto& entry : recent)
    if (entry.label && same(address, entry.address)) return entry.label;
  const unsigned index = next % 16;
  recent[index] = {address, ++next};
  return next;
}

void noticeFailure(const char* stage, int rc) {
  // Scan and host failures have no associated connection attempt.
  portENTER_CRITICAL(&lock);
  if (view.lastStage != stage || view.lastCode != rc || view.lastAttempt) {
    view.lastStage = stage;
    view.lastCode = rc;
    view.lastSlot = view.lastAttempt = view.lastPeer = 0;
    ++view.revision;
  }
  portEXIT_CRITICAL(&lock);
}

void traceEvent(uint32_t serial, const char* stage, int rc, uint16_t handle,
                uint16_t detail, bool failure) {
  const uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  if (serial && serial == active.serial) {
    if (traceCount < sizeof(events)/sizeof(events[0])) {
      events[(traceHead + traceCount) % (sizeof(events)/sizeof(events[0]))] =
          {active, stage, now - active.started, handle, detail, rc};
      ++traceCount;
    } else ++traceLost;
    // Preserve the first failure of each attempt, including through teardown.
    if (failure && view.lastAttempt != serial) {
      view.lastAttempt = serial;
      view.lastSlot = active.slot;
      view.lastPeer = active.peer;
      view.lastStage = stage;
      view.lastCode = rc;
      ++view.revision;
    }
  }
  portEXIT_CRITICAL(&lock);
}
void drainTrace() {
  for (;;) {
    portENTER_CRITICAL(&lock);
    const bool have = traceCount != 0;
    TraceEvent event{};
    if (have) {
      event = events[traceHead];
      traceHead = (traceHead + 1) % (sizeof(events)/sizeof(events[0]));
      --traceCount;
    }
    const unsigned lost = traceLost;
    traceLost = 0;
    portEXIT_CRITICAL(&lock);
    if (lost) Serial.printf("[WS_BLE] trace_lost=%u\n", lost);
    if (!have) return;
    const auto& a = event.attempt;
    Serial.printf("[WS_BLE] attempt=%lu ms=%lu air=%lu peer=%lu known=%u slot=%u handle=%u stage=%s rc=%d detail=%u\n",
                  (unsigned long)a.serial, (unsigned long)event.elapsed, (unsigned long)a.air,
                  (unsigned long)a.peer, a.known, a.slot, event.handle, event.stage, event.rc, event.detail);
  }
}

struct Target {
  ble_addr_t air{};
  ble_addr_t identity{};
  bool known = false;
};

// Keep recent connection failures out of the next few scans so one stale
// peripheral cannot monopolize attempts while a different keyboard wakes.
struct Retry {
  ble_addr_t id{};
  uint32_t expires = 0;
  bool valid = false;
};
Retry retries[4];
unsigned nextRetry = 0;

void postpone(const Target& target, uint32_t ms) {
  portENTER_CRITICAL(&lock);
  retries[nextRetry] = {target.known ? target.identity : target.air, millis() + ms, true};
  nextRetry = (nextRetry + 1) % 4;
  portEXIT_CRITICAL(&lock);
}

// An unknown peer can split flags and the HID service across an advertisement
// and its scan response. Never use the device name as an identity.
struct NewAdvertiser {
  ble_addr_t air{};
  bool discoverable = false, hid = false;
};
struct ScanWindow {
  NewAdvertiser devices[24]{};
  unsigned count = 0;
  bool open = false, overflow = false, matched = false;
  Target match{};
  uint32_t generation = 0;
} scan;

bool keyboardHint(const ble_hs_adv_fields& fields) {
  if (fields.appearance_is_present) {
    if (fields.appearance == 0x03c1) return true;
    if ((fields.appearance & 0xffc0) == 0x03c0 && fields.appearance != 0x03c0) return false;
  }
  for (unsigned i = 0; i < fields.num_uuids16; ++i)
    if (fields.uuids16[i].value == 0x1812) return true;
  return false;
}

int advertisement(ble_gap_event* event, void* context) {
  if (event->type != BLE_GAP_EVENT_DISC || !ws_privacy::current()) return 0;
  const auto& adv = event->disc;
  const auto kind = adv.event_type;
  if (kind != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && kind != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND &&
      kind != BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) return 0;
  Target peer{};
  peer.air = adv.addr;
  peer.known = ws_bonds::resolve(peer.air, peer.identity);
  ble_hs_adv_fields fields{};
  const bool parsed = !peer.known && ble_hs_adv_parse_fields(&fields, adv.data, adv.length_data) == 0;
  const bool hid = parsed && keyboardHint(fields);
  const bool discoverable = kind == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && parsed &&
                            (fields.flags & (BLE_HS_ADV_F_DISC_LTD | BLE_HS_ADV_F_DISC_GEN));
  if (!peer.known && !hid && !discoverable) return 0;
  portENTER_CRITICAL(&lock);
  if (scan.open && scan.generation == reinterpret_cast<uintptr_t>(context)) {
    bool cooling = false;
    const auto now = millis();
    for (auto& retry : retries) {
      if (retry.valid && int32_t(now - retry.expires) >= 0) retry.valid = false;
      if (retry.valid && same(retry.id, peer.known ? peer.identity : peer.air)) cooling = true;
    }
    if (!cooling && peer.known && !scan.matched) {
      scan.match = peer;
      scan.matched = true;
    } else if (!cooling && !peer.known) {
      unsigned i = 0;
      while (i < scan.count && !same(scan.devices[i].air, peer.air)) ++i;
      if (i == sizeof(scan.devices)/sizeof(scan.devices[0])) {
        scan.overflow = true;
      } else {
        if (i == scan.count) {
          scan.devices[scan.count++] = {peer.air, false, false};
        }
        scan.devices[i].discoverable |= discoverable;
        scan.devices[i].hid |= hid;
      }
    }
  }
  portEXIT_CRITICAL(&lock);
  return 0;
}

bool discover(Target& target) {
  if (!ws_privacy::current()) return false;
  const uint8_t ownType = BLEDevice::ownAddressType();
  portENTER_CRITICAL(&lock);
  const uint32_t generation = ++scan.generation;
  scan.open = true;
  scan.matched = scan.overflow = false;
  scan.count = 0;
  portEXIT_CRITICAL(&lock);
  ble_gap_disc_params params{};
  params.itvl = 64;     // 40 ms interval/window while unconnected.
  params.window = 64;
  params.passive = 0;
  params.filter_duplicates = 1;
  const int started = ble_gap_disc(ownType, ScanMs, &params, advertisement,
                                   reinterpret_cast<void*>(uintptr_t(generation)));
  const uint32_t start = millis();
  if (started == 0) {
    while (ble_gap_disc_active() && uint32_t(millis() - start) < ScanMs + 100) {
      portENTER_CRITICAL(&lock);
      const bool matched = scan.matched;
      portEXIT_CRITICAL(&lock);
      if (matched) break;
      delay(10);
    }
  }
  unsigned strangers = 0;
  portENTER_CRITICAL(&lock);
  scan.open = false;
  const bool known = scan.matched;
  if (known) {
    target = scan.match;
  } else if (!scan.overflow) {
    for (unsigned i = 0; i < scan.count; ++i) {
      const auto& peer = scan.devices[i];
      if (peer.hid && peer.discoverable) {
        ++strangers;
        target = {peer.air, peer.air, false};
      }
    }
  }
  portEXIT_CRITICAL(&lock);
  if (started != 0) {
    noticeFailure("scan_start", started);
    Serial.printf("[WS_BLE] scan=%lu own=%u rc=%d\n", (unsigned long)generation, ownType, started);
    return false;
  }
  if (ble_gap_disc_active()) {
    const int stopped = ble_gap_disc_cancel();
    if (stopped != 0 && ble_gap_disc_active()) {
      noticeFailure("scan_stop", stopped);
      Serial.printf("[WS_BLE] scan=%lu cancel_rc=%d\n", (unsigned long)generation, stopped);
      return false;
    }
  }
  if (!ws_privacy::current()) return false;
  if (known || strangers || generation % 10 == 1) {
    Serial.printf("[WS_BLE] scan=%lu own=%u known=%u slot=%u strangers=%u rc=0\n",
                  (unsigned long)generation, ownType, known, known ? ws_bonds::slot(target.identity) : 0u, strangers);
  }
  if (known) return true;
  if (strangers == 1 && surfaceReady.load()) return true;
  if (strangers > 1) display(PickerMode::Ambiguous, "Multiple keyboards; waiting");
  else display(PickerMode::Waiting, "Scan OK / no match");
  return false;
}

constexpr int NoEncryptionEvent = -1;
std::atomic<int> encryptionResult{NoEncryptionEvent}, disconnectReason{0};
std::atomic<bool> linkBound{false};
void record(const char* stage, int rc) {
  const bool securityInProgress = rc == BLE_HS_EALREADY &&
      (!std::strcmp(stage, "restore_encryption") || !std::strcmp(stage, "pair"));
  portENTER_CRITICAL(&lock);
  const uint16_t handle = traceHandle;
  portEXIT_CRITICAL(&lock);
  traceEvent(attemptSerial, stage, rc, handle, 0, rc && !securityInProgress);
  drainTrace();
}

void blockAttempt() {
  ws_smp::close();
  linkBound = false;
  interrupted = true;
  pinAllowed = false;
  input::invalidate();
  ws_bonds::cancelAttempt();
}

class LinkCallbacks final : public BLEClientCallbacks {
  const uint32_t serial_;
public:
  explicit LinkCallbacks(uint32_t serial) : serial_(serial) {}
  void onConnectionStage(uint16_t handle, const char* stage, int rc, uint16_t detail) override {
    // A disconnect after admission is a normal boundary, still captured on UART.
    portENTER_CRITICAL(&lock);
    const bool admitted = view.readyAttempt == serial_;
    portEXIT_CRITICAL(&lock);
    const bool retryingStart = rc == BLE_HS_EBUSY && !std::strcmp(stage, "gap_start");
    const bool normalDisconnect = admitted && !std::strcmp(stage, "disconnect");
    const bool unrelatedIO = !std::strncmp(stage, "io_skip_", 8) || !std::strcmp(stage, "repeat_skip_handle");
    traceEvent(serial_, stage, rc, handle, detail, rc && !retryingStart && !normalDisconnect && !unrelatedIO);
  }
  bool onLinkEstablished(const ble_gap_conn_desc& link) override {
    if (link.role != BLE_GAP_ROLE_MASTER || interrupted.load() || !ws_privacy::current() ||
        !ws_bonds::bindConnection(link) || !ws_smp::bind(link.conn_handle, newPair.load())) {
      traceEvent(serial_, "bind_gate", BLE_HS_EAUTHEN, link.conn_handle, 0, true);
      blockAttempt();
      return false;
    }
    portENTER_CRITICAL(&lock);
    traceHandle = link.conn_handle;
    portEXIT_CRITICAL(&lock);
    linkBound = true;
    return true;
  }
  bool acceptPairingIO(uint16_t handle, uint16_t action) override {
    if (pinAllowed.load() && action == BLE_SM_IOACT_DISP && ws_smp::permitsFresh(handle)) return true;
    const char* reason = !newPair.load() ? "io_known" : !pinAllowed.load() ? "io_closed" :
                         action != BLE_SM_IOACT_DISP ? "io_action" : "io_handle";
    traceEvent(serial_, reason, BLE_HS_EAUTHEN, handle, action, true);
    blockAttempt();
    return false;
  }
  void onEncryptionResult(uint16_t handle, int rc) override {
    traceEvent(serial_, "enc_change", rc, handle, 0, rc != 0);
    if (rc) {
      encryptionResult = rc;
      blockAttempt();
    }
  }
  void onRepeatPairing(uint16_t handle) override {
    traceEvent(serial_, "repeat_ignored", BLE_HS_EAUTHEN, handle, 0, true);
    blockAttempt();
  }
  void onConnect(BLEClient* client) override {
    input::connected(client->getConnId());
  }
  void onDisconnect(BLEClient* client) override {
    disconnectReason = client->lastConnectionError();
    blockAttempt();
    pin = NoPasskey;
    phase = Phase::Disconnected;
  }
};

class SecurityCallbacks final : public BLESecurityCallbacks {
  void rejectIO(const char* reason, uint16_t action) {
    // The library's security callbacks have no handle parameter. For these
    // events UART records the already bound handle, or NONE if not bound.
    portENTER_CRITICAL(&lock);
    const uint32_t serial = active.serial;
    const uint16_t handle = traceHandle;
    portEXIT_CRITICAL(&lock);
    traceEvent(serial, reason, BLE_HS_EAUTHEN, handle, action, true);
    blockAttempt();
  }
  bool onSecurityRequest() override { return !interrupted.load(); }
  void onPassKeyNotify(uint32_t value) override {
    if (!pinAllowed.load() || !surfaceReady.load()) {
      rejectIO(pinAllowed.load() ? "io_surface" : "io_closed", BLE_SM_IOACT_DISP);
      return;
    }
    if (!interrupted.load()) { pin = value; phase = Phase::Pairing; }
  }
  uint32_t onPassKeyRequest() override { rejectIO("io_input", BLE_SM_IOACT_INPUT); return 0; }
  bool onConfirmPIN(uint32_t) override { rejectIO("io_confirm", BLE_SM_IOACT_NUMCMP); return false; }
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    if (desc && (!desc->sec_state.encrypted || desc->sec_state.key_size != 16)) blockAttempt();
  }
} securityCallbacks;

bool live(BLEClient& client, ble_gap_conn_desc& desc) {
  return !interrupted.load() && client.isConnected() &&
         ble_gap_conn_find(client.getConnId(), &desc) == 0;
}

bool secure(BLEClient& client, const Target& target, ble_gap_conn_desc& desc) {
  if (!linkBound.load() || !live(client, desc)) {
    const int observed = encryptionResult.load();
    if (observed > 0) record("enc_event", observed);
    else if (ws_smp::refused()) record("smp_guard", BLE_HS_EAUTHEN);
    else if (disconnectReason.load()) record("disconnect", disconnectReason.load());
    else record("link_identity", BLE_HS_ENOTCONN);
    return false;
  }
  int rc = 0;
  const char* stage = target.known ? "restore_encryption" : "pair";
  if (target.known) {
    // The peer can request security before MTU negotiation returns. Accept
    // completion only for this handle after the matching old credential was used.
    if (!ws_bonds::verifiedExisting(desc)) rc = ws_bonds::restoreEncryption(desc);
  } else if (!desc.sec_state.encrypted || !ws_bonds::hasNewKeys(desc.peer_id_addr)) {
    rc = ble_gap_pair_initiate(client.getConnId());
  }
  record(stage, rc);
  if (rc != 0 && rc != BLE_HS_EALREADY) return false;
  const auto start = millis();
  while (uint32_t(millis() - start) < (target.known ? 8000u : AuthMs) && live(client, desc)) {
    if (desc.sec_state.encrypted && desc.sec_state.key_size == 16 && desc.sec_state.bonded) {
      const bool accepted = target.known ? ws_bonds::verifiedExisting(desc)
                                         : ws_bonds::hasNewKeys(desc.peer_id_addr);
      if (accepted) {
        record("encrypted", 0);
        return true;
      }
      if (target.known) {
        record("key_identity", BLE_HS_EAUTHEN);
        return false;
      }
    }
    delay(15);
  }
  const int observed = encryptionResult.load();
  if (observed > 0) record("encryption_result", observed);
  else if (ws_smp::refused()) record("smp_guard", BLE_HS_EAUTHEN);
  else if (disconnectReason.load()) record("disconnect", disconnectReason.load());
  else record("encryption_result", interrupted.load() ? BLE_HS_EAUTHEN : BLE_HS_ETIMEOUT);
  return false;
}

bool approveOnDevice(BLEClient& client) {
  const auto token = input::stats().epoch;
  display(PickerMode::Confirm, LocalConfirmationHint, token);
  const uint32_t start = millis();
  while (client.isConnected() && !interrupted.load() && uint32_t(millis()-start) < AuthMs) {
    portENTER_CRITICAL(&lock);
    const bool approved = consent && confirmationVisible && view.confirmationToken == token;
    portEXIT_CRITICAL(&lock);
    if (approved) return true;
    delay(20);
  }
  return false;
}

bool establish(BLEClient& client, const Target& target, LinkCallbacks& callbacks) {
  ++attemptSerial;
  const AttemptInfo chosen{attemptSerial, millis(), peerAlias(target.air), peerAlias(target.identity),
                           target.known ? ws_bonds::slot(target.identity) : 0, target.known};
  portENTER_CRITICAL(&lock);
  active = chosen;
  traceHandle = BLE_HS_CONN_HANDLE_NONE;
  admittedInput = false;
  portEXIT_CRITICAL(&lock);
  encryptionResult = NoEncryptionEvent;
  disconnectReason = 0;
  linkBound = false;
  ws_smp::start(!target.known);
  newPair = !target.known;
  pinAllowed = !target.known;
  pin = NoPasskey;
  interrupted = false;
  ws_bonds::startAttempt(target.known, target.identity);
  BLESecurity::setAuthenticationMode(!target.known, true, true);
  BLESecurity::setPassKey(false);
  phase = Phase::Connecting;
  display(PickerMode::Busy, "Connecting keyboard");
  client.setClientCallbacks(&callbacks);
  client.useExternalSecurity();
  if (!ws_privacy::current() || interrupted.load()) return false;
  record("dial", 0);
  if (!client.connect(BLEAddress(target.air), target.air.type, 4000)) {
    const int observed = encryptionResult.load();
    if (observed > 0) record("enc_event", observed);
    else if (ws_smp::refused()) record("smp_guard", BLE_HS_EAUTHEN);
    else record("connect", client.lastConnectionError());
    return false;
  }
  record("connect", 0);
  phase = Phase::Pairing;
  ble_gap_conn_desc desc{};
  if (!secure(client, target, desc)) return false;
  pinAllowed = false;
  const bool displayedPin = pin.exchange(NoPasskey) < NoPasskey;
  if (displayedPin && !desc.sec_state.authenticated) {
    record("pin_auth", BLE_HS_EAUTHEN);
    return false;
  }
  ws_smp::close(); // Never negotiate fresh credentials from a subsequent ATT retry.
  if (!input::prepare(client) || !live(client, desc)) {
    record("hid_prepare", BLE_HS_EREJECT);
    return false;
  }
  bool local = false;
  if (!target.known && !desc.sec_state.authenticated) {
    if (!approveOnDevice(client)) {
      record("consent", BLE_HS_EREJECT);
      return false;
    }
    local = true;
  }
  if (!live(client, desc) || !desc.sec_state.encrypted || desc.sec_state.key_size != 16) {
    record("link_lost", BLE_HS_ENOTCONN);
    return false;
  }
  // The existing identity needs no rotation or renewed consent on reconnect.
  if (!ws_bonds::commit(desc, local) || !live(client, desc)) {
    record("commit", BLE_HS_EREJECT);
    return false;
  }
  if (!input::activate(client.getConnId())) {
    record("input_gate", BLE_HS_ENOTCONN);
    return false;
  }
  const unsigned committedSlot = ws_bonds::slot(desc.peer_id_addr);
  // On a known link the descriptor may still carry an unresolved RPA. The
  // selected canonical member was verified on this exact handle.
  const unsigned slot = chosen.known ? chosen.slot : committedSlot;
  traceEvent(attemptSerial, "admitted", 0, client.getConnId(), slot, false);
  portENTER_CRITICAL(&lock);
  view.readySlot = slot;
  view.readyAttempt = attemptSerial;
  ++view.revision;
  portEXIT_CRITICAL(&lock);
  drainTrace();
  phase = Phase::Ready;
  display(PickerMode::Closed, "");
  return true;
}

// The fixed Arduino client owns late callbacks even when connect() times out.
// Have NimBLE retire it on its event queue before destroying its GATT objects.
ble_npl_event retireEvent{};
BLEClient* retiring = nullptr;
std::atomic<bool> retireCompleted{false}, safeToDestroy{false};
void drain(ble_npl_event*) {
  safeToDestroy = retiring->retireStep(millis());
  retireCompleted.store(true, std::memory_order_release);
}
void retire(BLEClient* client) {
  ws_smp::close();
  linkBound = false;
  input::invalidate();
  interrupted = true;
  pinAllowed = false;
  newPair = false;
  pin = NoPasskey;
  phase = Phase::Cleaning;
  retiring = client;
  do {
    retireCompleted = false;
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &retireEvent);
    while (!retireCompleted.load(std::memory_order_acquire)) delay(5);
    if (!safeToDestroy.load()) delay(30);
  } while (!safeToDestroy.load());
  delete client;
  retiring = nullptr;
  ws_bonds::endAttempt();
}

std::atomic<int> hostResetCode{0};
void hostLifecycle(bool synced, int reason) {
  if (synced) {
    ws_privacy::hostSynced();
    hostResetCode = 0;
  } else {
    ws_privacy::hostReset();
    hostResetCode = reason;
    blockAttempt();
  }
}

void receive(void*) {
  BLEDevice::observePublicHost(hostLifecycle, 10000);
  if (!BLEDevice::init("Draftor BLE keyboard host")) {
    phase = Phase::Error;
    const int rc = BLEDevice::hostSyncError();
    display(PickerMode::Closed, "BLE host initialization failed");
    noticeFailure("host_init", rc ? rc : BLE_HS_EOS);
    vTaskSuspend(nullptr);
    return;
  }
  if (!ws_bonds::begin()) {
    display(PickerMode::Closed, "Bond storage unavailable");
    noticeFailure("bond_init", BLE_HS_ESTORE_FAIL);
    phase = Phase::Error;
    vTaskSuspend(nullptr);
    return;
  }
  BLEDevice::setSecurityCallbacks(&securityCallbacks);
  BLESecurity::setCapability(ESP_IO_CAP_OUT);
  BLESecurity::setInitEncryptionKey();
  BLESecurity::setRespEncryptionKey();
  BLESecurity::setForceAuthentication(false);
  ble_npl_event_init(&retireEvent, drain, nullptr);
  for (;;) {
    if (!ws_bonds::healthy()) {
      phase = Phase::Error;
      display(PickerMode::Closed, "Bond storage unavailable");
      vTaskSuspend(nullptr);
      continue;
    }
    // This worker alone starts GAP discovery/initiation. Rebuild the resolving
    // list after durable membership changes or a controller reset, off-link.
    const int rc = ws_privacy::sync();
    if (rc != 0 || !ws_privacy::current()) {
      phase = Phase::Error;
      display(PickerMode::Closed, "Keyboard privacy unavailable");
      const int reset = hostResetCode.load();
      noticeFailure(reset ? "host_reset" : ws_privacy::stage(),
                    reset ? reset : (rc ? rc : BLE_HS_ENOTSYNCED));
      delay(200);
      continue;
    }
    phase = Phase::Scanning;
    Target target{};
    if (!discover(target) || !ws_privacy::current()) { delay(60); continue; }
    LinkCallbacks callbacks(attemptSerial + 1);  // lives through host-queue retirement
    BLEClient* client = nullptr;
    bool admitted = false;
    try {
      client = BLEDevice::createClient();
      if (client) admitted = establish(*client, target, callbacks);
      while (admitted && !interrupted.load() && client->isConnected()) { drainTrace(); delay(40); }
    } catch (const std::bad_alloc&) {
      phase = Phase::Error;
    }
    if (client) retire(client);
    drainTrace();
    if (!admitted) postpone(target, target.known ? 3000 : 15000);
    display(PickerMode::Waiting, "Waiting for keyboard");
  }
}
} // namespace

bool begin() {
  return input::begin() &&
         xTaskCreatePinnedToCore(receive, "ws-keyboard", 8192, nullptr, 2, &worker, 0) == pdPASS;
}
uint32_t workerStackMinimumBytes() { return worker ? uxTaskGetStackHighWaterMark(worker) : 0; }
Status status() {
  const auto stats = input::stats();
  const auto state = phase.load();
  return {state == Phase::Ready && stats.awaitRelease ? Phase::AwaitRelease : state, pin.load(),
          stats.epoch, stats.received, stats.overflow, stats.bad, stats.peak,
          false, pairingView().mode == PickerMode::Confirm};
}
const char* phaseName(Phase state) {
  switch (state) {
    case Phase::Ready: return "Keyboard connected";
    case Phase::AwaitRelease: return "Release held keys";
    case Phase::Pairing: return "Pairing keyboard";
    case Phase::Connecting: return "Connecting keyboard";
    case Phase::Error: return "Keyboard connection error";
    default: return "Waiting for keyboard";
  }
}
PairingView pairingView() {
  portENTER_CRITICAL(&lock);
  const PairingView snapshot = view;
  portEXIT_CRITICAL(&lock);
  return snapshot;
}
const char* pairingNotice() { return pairingView().message; }
void pairingPresentation(uint32_t token, bool eligible, uint32_t) {
  surfaceReady = eligible;
  portENTER_CRITICAL(&lock);
  const bool visible = eligible && token && view.mode == PickerMode::Confirm &&
                       token == view.confirmationToken;
  if (view.mode == PickerMode::Confirm && (!visible || !confirmationVisible)) {
    buttonReleased = false;
    buttonDown = false;
  }
  confirmationVisible = visible;
  portEXIT_CRITICAL(&lock);
}
void pairingButton(bool down, uint32_t now) {
  bool cancel = false;
  portENTER_CRITICAL(&lock);
  if (view.mode == PickerMode::Confirm && confirmationVisible) {
    if (!down && !buttonReleased) buttonReleased = true;
    if (buttonReleased) {
      if (down && !buttonDown) { buttonDown = true; buttonSince = now; }
      if (!down && buttonDown) {
        buttonDown = false;
        const auto held = now - buttonSince;
        if (held >= 2000) consent = true;
        else if (held >= 40) cancel = true;
      }
    }
  } else if (phase.load() == Phase::Pairing && newPair.load()) {
    if (down && !buttonDown) { buttonDown = true; buttonSince = now; }
    if (!down && buttonDown) {
      buttonDown = false;
      if (now - buttonSince >= 40) cancel = true;
    }
  } else buttonDown = false;
  if (cancel) interrupted = true;
  portEXIT_CRITICAL(&lock);
  if (cancel) blockAttempt();
}
bool pairingKey(const s3hid::Event& event) {
  // Invoked by the editor after both admission and route checks. Preserve
  // the last failure through reconnects that never produce input.
  if (event.down && phase.load() == Phase::Ready) {
    portENTER_CRITICAL(&lock);
    const uint32_t serial = view.readyAttempt;
    const unsigned slot = view.readySlot;
    const uint16_t handle = traceHandle;
    const bool first = !admittedInput && serial == active.serial && view.mode == PickerMode::Closed &&
                       phase.load() == Phase::Ready;
    if (first) {
      admittedInput = true;
      view.lastStage = nullptr;
      view.lastCode = 0;
      view.lastSlot = view.lastAttempt = view.lastPeer = 0;
      ++view.revision;
    }
    portEXIT_CRITICAL(&lock);
    if (first) traceEvent(serial, "input", 0, handle, slot, false);
  }
  return false;
}
} // namespace keyboard

void ws_smp::onRefused() { keyboard::blockAttempt(); }

// The fixed Host installs the real link here, before its later public CONNECT
// notification. The event remains opaque; use only the public GAP descriptor.
struct ble_gap_conn_complete;
extern "C" int __real_ble_gap_rx_conn_complete(ble_gap_conn_complete*, uint8_t);
extern "C" int __wrap_ble_gap_rx_conn_complete(ble_gap_conn_complete* event, uint8_t instance) {
  const int rc = __real_ble_gap_rx_conn_complete(event, instance);
  if (!rc) BLEClient::controllerConnectionAvailable();
  return rc;
}
