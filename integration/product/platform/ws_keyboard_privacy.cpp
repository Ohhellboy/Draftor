#include "ws_keyboard_privacy.hpp"
#include "ws_keyboard_bonds.hpp"

#include <atomic>
#include <cstring>
#include <host/ble_hs.h>
#include <host/ble_hs_id.h>
#include <host/ble_store.h>
#include <nimble/hci_common.h>
#include <nimble/nimble_port.h>

// The fixed S3 SDK exports these Host interfaces. Use the wire structs from
// that SDK for the two list commands unavailable through its public API.
extern "C" int ble_hs_pvcy_our_irk(const uint8_t**);
extern "C" int ble_hs_pvcy_ensure_started(void);
extern "C" int ble_hs_pvcy_set_resolve_enabled(int);
extern "C" int ble_hs_pvcy_set_mode(const ble_addr_t*, uint8_t);
extern "C" int ble_hs_hci_cmd_tx(uint16_t, const void*, uint8_t, void*, uint8_t);
extern "C" void ble_gap_conn_foreach_handle(ble_gap_conn_foreach_handle_fn*, void*);
extern "C" void ble_hs_lock(void);
extern "C" void ble_hs_unlock(void);

namespace ws_privacy {
namespace {
std::atomic<uint32_t> revision{1}, applied{0}, requested{0};
std::atomic<bool> synced{false};
std::atomic<const char*> lastStage{"host_sync"};
std::atomic<int> result{BLE_HS_ENOTSYNCED};
ble_npl_event event{};
ble_npl_sem done{};
bool initialized = false, inFlight = false; // worker-owned, one outstanding request
uint32_t failedRevision = 0;
int failedRc = 0;
const char* eventStage = "host_sync"; // delivered to the worker by the semaphore
ble_addr_t bootIdentity{};
uint8_t bootIrk[16]{};
bool pinned = false; // accessed only on the Host queue

int failure(const char* stageName, int rc) {
    eventStage = stageName;
    return rc;
}
int report(const char* stageName, int rc) {
    lastStage = stageName;
    return rc;
}
bool nonzero(const uint8_t* bytes, unsigned count) {
    for (unsigned i = 0; i < count; ++i) if (bytes[i] != 0) return true;
    return false;
}
int hasConnection(uint16_t, void* ctx) {
    *static_cast<bool*>(ctx) = true;
    return 1;
}
int add(const Peer& peer, const uint8_t* localIrk, const char* addStage, const char* modeStage) {
    ble_hci_le_add_resolv_list_cp cmd{};
    cmd.peer_addr_type = peer.identity.type;
    std::memcpy(cmd.peer_id_addr, peer.identity.val, sizeof(cmd.peer_id_addr));
    std::memcpy(cmd.peer_irk, peer.irk, sizeof(cmd.peer_irk));
    std::memcpy(cmd.local_irk, localIrk, sizeof(cmd.local_irk));
    int rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_ADD_RESOLV_LIST),
                               &cmd, sizeof(cmd), nullptr, 0);
    if (rc) return failure(addStage, rc);
    rc = ble_hs_pvcy_set_mode(&peer.identity, BLE_GAP_PRIVATE_MODE_DEVICE);
    return rc ? failure(modeStage, rc) : 0;
}
int reconcile(uint32_t wanted) {
    if (!synced || !ble_hs_synced() || revision != wanted)
        return failure("host_sync", BLE_HS_ENOTSYNCED);
    if (!ws_bonds::restoreStoreCallbacks()) return failure("store_callbacks", BLE_HS_ESTORE_FAIL);
    ble_store_key_local_irk key{};
    key.addr.type = BLE_ADDR_PUBLIC;
    int rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, key.addr.val, nullptr);
    if (rc || !nonzero(key.addr.val, sizeof(key.addr.val)))
        return failure("public_identity", rc ? rc : BLE_HS_EINVAL);
    ble_store_value_local_irk stored{};
    rc = ble_store_read_local_irk(&key, &stored);
    if (rc) return failure("local_irk_read", rc);
    const uint8_t* active = nullptr;
    rc = ble_hs_pvcy_our_irk(&active);
    if (rc) return failure("local_irk_active", rc);
    if (!active || !nonzero(stored.irk, sizeof(stored.irk)) ||
        ble_addr_cmp(&key.addr, &stored.addr) || std::memcmp(active, stored.irk, sizeof(stored.irk)))
        return failure("local_irk_match", BLE_HS_EAUTHEN);
    if (pinned && (ble_addr_cmp(&key.addr, &bootIdentity) || std::memcmp(bootIrk, active, sizeof(bootIrk))))
        return failure("local_irk_changed", BLE_HS_EAUTHEN);
    bootIdentity = key.addr;
    std::memcpy(bootIrk, active, sizeof(bootIrk));
    pinned = true;

    PeerList peers{};
    if (!ws_bonds::copyPrivacyPeers(peers) || peers.count > 3)
        return failure("privacy_members", BLE_HS_ESTORE_FAIL);
    for (unsigned i = 0; i < peers.count; ++i) {
        const auto& peer = peers.entries[i];
        if (peer.identity.type > BLE_ADDR_RANDOM || !nonzero(peer.identity.val, sizeof(peer.identity.val)) ||
            !nonzero(peer.irk, sizeof(peer.irk))) return failure("privacy_member", BLE_HS_EINVAL);
    }
    bool connected = false;
    // ble_hs_conn_foreach() requires its caller to hold the Host mutex.
    ble_hs_lock();
    ble_gap_conn_foreach_handle(hasConnection, &connected);
    ble_hs_unlock();
    if (connected || ble_gap_adv_active() || ble_gap_disc_active() || ble_gap_conn_active())
        return failure("gap_idle", BLE_HS_EBUSY);
    rc = ble_hs_pvcy_ensure_started();
    if (rc) return failure("privacy_start", rc);
    ble_hci_le_rd_resolv_list_size_rp capacity{};
    rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_RD_RESOLV_LIST_SIZE),
                           nullptr, 0, &capacity, sizeof(capacity));
    if (rc || capacity.size < peers.count + 1)
        return failure("resolving_capacity", rc ? rc : BLE_HS_ENOMEM);
    rc = ble_hs_pvcy_set_resolve_enabled(0);
    if (rc) return failure("resolve_disable", rc);
    rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(BLE_HCI_OGF_LE, BLE_HCI_OCF_LE_CLR_RESOLV_LIST),
                           nullptr, 0, nullptr, 0);
    if (rc) return failure("resolving_clear", rc);
    // Stock NimBLE creates this local-only entry during Host startup.
    const Peer local{};
    rc = add(local, stored.irk, "local_add", "local_mode");
    if (rc) return rc;
    static const char* const addStages[] = {"member_add_1", "member_add_2", "member_add_3"};
    static const char* const modeStages[] = {"member_mode_1", "member_mode_2", "member_mode_3"};
    for (unsigned i = 0; i < peers.count; ++i) {
        rc = add(peers.entries[i], stored.irk, addStages[i], modeStages[i]);
        if (rc) return rc;
    }
    rc = ble_hs_pvcy_set_resolve_enabled(1);
    if (rc) return failure("resolve_enable", rc);
    if (!synced || revision != wanted) return failure("changed", BLE_HS_ENOTSYNCED);
    eventStage = "privacy_ready";
    return 0;
}
void run(ble_npl_event*) {
    result = reconcile(requested.load());
    ble_npl_sem_release(&done);
}
} // namespace

void dirty() { ++revision; }
void hostReset() { synced = false; dirty(); }
void hostSynced() { dirty(); synced = true; }
bool current() { return synced && applied.load() == revision.load(); }
const char* stage() { return lastStage.load(); }

int sync() {
    if (!inFlight) {
        if (current()) return 0;
        if (!synced) return report("host_sync", BLE_HS_ENOTSYNCED);
        const uint32_t wanted = revision.load();
        if (failedRevision == wanted) return failedRc;
        if (!initialized) {
            if (ble_npl_sem_init(&done, 0) != BLE_NPL_OK) return report("privacy_queue", BLE_HS_ENOMEM);
            ble_npl_event_init(&event, run, nullptr);
            initialized = true;
        }
        requested = wanted;
        inFlight = true;
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &event);
    }
    // A timed-out event keeps its storage and token until the original Host
    // callback returns. A new revision never consumes that completion as its own.
    if (ble_npl_sem_pend(&done, ble_npl_time_ms_to_ticks32(1500)) != BLE_NPL_OK)
        return report("privacy_wait", BLE_HS_ETIMEOUT);
    inFlight = false;
    const uint32_t wanted = requested.load();
    if (!synced || revision != wanted) return report("changed", BLE_HS_ENOTSYNCED);
    const int rc = result.load();
    if (!rc) {
        applied = wanted;
    } else if (std::strcmp(eventStage, "gap_idle") != 0 && rc != BLE_HS_ENOTSYNCED) {
        failedRevision = wanted;
        failedRc = rc;
    }
    return report(eventStage, rc);
}

extern "C" int __real_ble_store_write_local_irk(const ble_store_value_local_irk*);

// The pinned Host generates a default IRK after *any* read error. Its startup
// runs on the Host queue and is this target's only local-IRK writer. Re-read
// before a cold-start write, while the native store callbacks are installed.
extern "C" int __wrap_ble_store_write_local_irk(const ble_store_value_local_irk* proposed) {
    if (!proposed || proposed->addr.type != BLE_ADDR_PUBLIC ||
        !nonzero(proposed->addr.val, sizeof(proposed->addr.val)) ||
        !nonzero(proposed->irk, sizeof(proposed->irk))) return BLE_HS_EINVAL;
    ble_store_key_local_irk key{};
    key.addr = proposed->addr;
    ble_store_value_local_irk saved{};
    const int rc = ble_store_read_local_irk(&key, &saved);
    if (rc == BLE_HS_ENOENT) return __real_ble_store_write_local_irk(proposed);
    if (rc != 0) return rc;
    if (ble_addr_cmp(&saved.addr, &key.addr) || std::memcmp(saved.irk, proposed->irk, sizeof(saved.irk)))
        return BLE_HS_EAUTHEN;
    return 0;
}
} // namespace ws_privacy
