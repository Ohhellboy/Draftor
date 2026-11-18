#include "ws_keyboard_bonds.hpp"
#include "ws_keyboard_privacy.hpp"

#include <atomic>
#include <cstring>
#include <esp_crc.h>
#include <freertos/FreeRTOS.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <nvs.h>

// Exported by the IDF 5.5.5 NimBLE host used in this target.
extern "C" bool ble_hs_pvcy_resolve_with_irk(const uint8_t*, const uint8_t*);
extern "C" void ble_hs_lock(void);
extern "C" void ble_hs_unlock(void);

namespace ws_bonds {
namespace {
constexpr unsigned capacity = 3;

// This build's SDK uses three fixed slots and may ask for a victim before
// authentication. Use the SDK's public store callbacks for the two security
// object classes. NVS atomically replaces one ordered set on successful pairing.
struct Member {
    ble_store_value_sec peer;
    ble_store_value_sec ours;
    uint8_t oursPresent;
    uint8_t boardConfirmed;
};
struct Members {
    uint32_t format;
    uint32_t count;
    Member entries[capacity];
};
static_assert(CONFIG_BT_NIMBLE_MAX_BONDS == capacity);
static_assert(sizeof(ble_store_value_sec) == 88, "Check the stored SDK ABI on upgrade");
constexpr uint32_t format = 0x57530101;
Members members{};
Member pending{};
enum class Attempt { Idle, Existing, Pairing };
Attempt attempt = Attempt::Idle;
ble_addr_t expected{};
uint32_t generation = 0;
uint16_t boundHandle = BLE_HS_CONN_HANDLE_NONE;
bool previousKeyUsed = false;
bool stagedPeer = false;
bool inconsistent = false;
nvs_handle_t handle;
std::atomic<bool> ready{false};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
ble_store_read_fn* previousRead = nullptr;
ble_store_write_fn* previousWrite = nullptr;
ble_store_delete_fn* previousDelete = nullptr;
ble_store_status_fn* previousStatus = nullptr;

bool same(const ble_addr_t& a, const ble_addr_t& b) {
    return a.type <= 3 && b.type <= 3 && (a.type & 1) == (b.type & 1) && std::memcmp(a.val,b.val,6) == 0;
}
ble_addr_t normalized(ble_addr_t a) { a.type &= 1; return a; }
int position(const Members& list, const ble_addr_t& id) {
    for (unsigned i=0; i<list.count; ++i)
        if (same(list.entries[i].peer.peer_addr,id)) return static_cast<int>(i);
    return -1;
}
bool usable(const ble_store_value_sec& k) {
    const bool identity = k.peer_addr.type == BLE_ADDR_PUBLIC ||
        (k.peer_addr.type == BLE_ADDR_RANDOM && (k.peer_addr.val[5] & 0xc0) == 0xc0);
    return identity && k.ltk_present && k.key_size == 16;
}
bool valid(const Members& list) {
    if (list.format != format || list.count > capacity) return false;
    for (unsigned i=0; i<list.count; ++i) {
        const auto& m = list.entries[i];
        if (!usable(m.peer) || (!m.peer.authenticated && !m.boardConfirmed) ||
            m.boardConfirmed > 1 || m.oursPresent > 1 ||
            (m.oursPresent && !same(m.ours.peer_addr,m.peer.peer_addr)) ||
            position(list,m.peer.peer_addr) != static_cast<int>(i)) return false;
    }
    return true;
}
bool save(const Members& desired) {
    if (!valid(desired) || nvs_set_blob(handle,"members",&desired,sizeof(desired)) != ESP_OK ||
        nvs_commit(handle) != ESP_OK) return false;
    Members verify{};
    size_t size = sizeof(verify);
    return nvs_get_blob(handle,"members",&verify,&size) == ESP_OK && size == sizeof(verify) &&
           std::memcmp(&verify,&desired,sizeof(verify)) == 0;
}

bool security(int type) {
    return type == BLE_STORE_OBJ_TYPE_PEER_SEC || type == BLE_STORE_OBJ_TYPE_OUR_SEC;
}
// Each snapshot is valid for exactly one connection/member generation. Perform
// the AES lookup outside the critical section, then recheck before using its index.
struct Match {
    int index;
    uint32_t generation;
};
Match matchAddress(const ble_addr_t& address) {
    struct Candidate { uint8_t irk[16]; bool hasIrk; };
    Candidate candidates[capacity]{};
    Match match{-1, 0};
    portENTER_CRITICAL(&mux);
    match.generation = generation;
    const unsigned count = members.count;
    for (unsigned i = 0; i < count; ++i) {
        const auto& peer = members.entries[i].peer;
        if (same(address, peer.peer_addr)) {
            match.index = i;
            break;
        }
        candidates[i].hasIrk = peer.irk_present;
        if (peer.irk_present) std::memcpy(candidates[i].irk, peer.irk, 16);
    }
    portEXIT_CRITICAL(&mux);
    if (match.index >= 0 || address.type != BLE_ADDR_RANDOM ||
        (address.val[5] & 0xc0) != 0x40) return match;
    const unsigned prand = (unsigned(address.val[5] & 0x3f) << 16) |
                           (unsigned(address.val[4]) << 8) | address.val[3];
    if (prand == 0 || prand == 0x3fffff) return match;
    for (unsigned i = 0; i < count; ++i) {
        if (!candidates[i].hasIrk ||
            !ble_hs_pvcy_resolve_with_irk(address.val, candidates[i].irk)) continue;
        if (match.index >= 0) {
            match.index = -1;
            break;
        }
        match.index = i;
    }
    return match;
}
// Caller holds mux; revalidates the result before returning credential material.
bool boundMatch(const Match& m, uint16_t connection) {
    return m.index >= 0 && m.generation == generation && attempt == Attempt::Existing &&
           !inconsistent && connection != BLE_HS_CONN_HANDLE_NONE && connection == boundHandle &&
           same(expected, members.entries[m.index].peer.peer_addr);
}
int readRecord(int type, const ble_store_key* key, ble_store_value* value) {
    if (type == BLE_STORE_OBJ_TYPE_PEER_ADDR) return BLE_HS_ENOENT;
    if (!security(type)) return previousRead(type,key,value);
    constexpr ble_addr_t wildcard{};
    if (ble_addr_cmp(&key->sec.peer_addr,&wildcard) == 0) {
        unsigned skip = key->sec.idx;
        int rc = BLE_HS_ENOENT;
        portENTER_CRITICAL(&mux);
        for (unsigned i = 0; i < members.count; ++i) {
            const Member& member = members.entries[i];
            if (type == BLE_STORE_OBJ_TYPE_OUR_SEC && !member.oursPresent) continue;
            if (skip) {
                --skip;
                continue;
            }
            value->sec = type == BLE_STORE_OBJ_TYPE_PEER_SEC ? member.peer : member.ours;
            rc = 0;
            break;
        }
        portEXIT_CRITICAL(&mux);
        return rc;
    }
    if (key->sec.idx) return BLE_HS_ENOENT;
    const Match m = matchAddress(key->sec.peer_addr);
    int rc = BLE_HS_ENOENT;
    portENTER_CRITICAL(&mux);
    if (boundMatch(m,boundHandle)) {
        const Member& member = members.entries[m.index];
        if (type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
            value->sec = member.peer;
            previousKeyUsed = true;
            rc = 0;
        } else if (member.oursPresent) {
            value->sec = member.ours;
            rc = 0;
        }
    }
    portEXIT_CRITICAL(&mux);
    return rc;
}
int writeRecord(int type, const ble_store_value* value) {
    if (type == BLE_STORE_OBJ_TYPE_PEER_ADDR) return 0; // RPA cache is unnecessary with IRK resolution
    if (!security(type)) return previousWrite(type,value);
    portENTER_CRITICAL(&mux);
    if (attempt == Attempt::Pairing && boundHandle != BLE_HS_CONN_HANDLE_NONE) {
        auto sec = value->sec;
        sec.peer_addr = normalized(sec.peer_addr);
        if (value->sec.peer_addr.type > 3 ||
            (pending.oursPresent && !same(pending.ours.peer_addr,sec.peer_addr)) ||
            (stagedPeer && !same(pending.peer.peer_addr,sec.peer_addr))) {
            inconsistent = true;
        } else if (type == BLE_STORE_OBJ_TYPE_OUR_SEC) {
            pending.ours = sec;
            pending.oursPresent = 1;
        } else {
            pending.peer = sec;
            stagedPeer = true;
        }
    } else if (attempt == Attempt::Existing) {
        // Restoring a bond does not run SMP key distribution. A fresh
        // distribution here means the peer re-paired during a known-only dial.
        // Never report such a link as a durable old-bond reconnection.
        inconsistent = true;
    }
    portEXIT_CRITICAL(&mux);
    // SMP delivers security completion separately. Do not let
    // ble_store_write_peer_sec install a speculative IRK into the controller.
    return BLE_HS_EBUSY;
}
int deleteRecord(int type, const ble_store_key* key) {
    if (security(type) || type == BLE_STORE_OBJ_TYPE_PEER_ADDR) return BLE_HS_ENOTSUP;
    return previousDelete(type,key);
}
int capacityNotice(ble_store_status_event* event, void*) {
    // FULL is an early capacity inquiry. OVERFLOW requests eviction and retry.
    if (!event || event->event_code != BLE_STORE_EVENT_FULL || !security(event->full.obj_type))
        return BLE_HS_ESTORE_CAP;
    portENTER_CRITICAL(&mux);
    const bool roomForCandidate = attempt == Attempt::Pairing;
    portEXIT_CRITICAL(&mux);
    return roomForCandidate ? 0 : BLE_HS_ESTORE_CAP;
}
// The only legacy-format dependency is this one-time, read-only import.
// D1 order starts at byte 4. Its local consent records bind identity AND LTK.
struct Legacy {
    bool hasOrder = false;
    uint8_t count = 0;
    ble_addr_t order[capacity]{};
    uint8_t trust[71]{};
};
ble_addr_t idAt(const uint8_t* bytes) {
    ble_addr_t id{};
    id.type = bytes[0];
    std::memcpy(id.val,bytes+1,6);
    return id;
}
bool legacyConsent(const Legacy& legacy, const ble_store_value_sec& key) {
    if (legacy.trust[0] != 1 || legacy.trust[1] > capacity) return false;
    for (unsigned i=0; i<legacy.trust[1]; ++i) {
        const auto* row = legacy.trust + 2+i*23;
        if (same(idAt(row),key.peer_addr) && std::memcmp(row+7,key.ltk,16) == 0) return true;
    }
    return false;
}
bool readLegacy(nvs_handle_t old, Legacy& legacy) {
    size_t size = 0;
    if (nvs_get_blob(old,"txn3",nullptr,&size) != ESP_ERR_NVS_NOT_FOUND) return false;
    uint8_t record[107]{};
    size = sizeof(record);
    const auto rc = nvs_get_blob(old,"bank3",record,&size);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        size = sizeof(legacy.trust);
        const auto local = nvs_get_blob(old,"local",legacy.trust,&size);
        return local == ESP_ERR_NVS_NOT_FOUND || (local == ESP_OK && size == sizeof(legacy.trust) &&
               legacy.trust[0] == 1 && legacy.trust[1] <= capacity);
    }
    if (rc != ESP_OK || size != sizeof(record) || record[0] != 3 || record[1] > capacity ||
        record[3] > 1 || record[32] != 1 || record[33] > capacity) return false;
    uint32_t checksum;
    std::memcpy(&checksum,record+103,4);
    if (checksum != esp_crc32_le(0,record,103)) return false;
    legacy.hasOrder = true;
    legacy.count = record[1];
    for (unsigned i=0; i<legacy.count; ++i) {
        legacy.order[i] = idAt(record+4+7*i);
        if (legacy.order[i].type > 1) return false;
        for (unsigned j=0; j<i; ++j) if (same(legacy.order[j],legacy.order[i])) return false;
    }
    std::memcpy(legacy.trust,record+32,sizeof(legacy.trust));
    return true;
}
bool importMember(Members& out,const Legacy& legacy,const ble_store_value_sec& peer) {
    const bool confirmed = legacyConsent(legacy,peer);
    if (!usable(peer) || (!peer.authenticated && !confirmed)) return !legacy.hasOrder;
    if (out.count == capacity || position(out,peer.peer_addr) >= 0) return false;
    auto& m = out.entries[out.count];
    m.peer = peer;
    m.boardConfirmed = confirmed;
    ble_store_key_sec key{};
    key.peer_addr = peer.peer_addr;
    const int rc = ble_store_read_our_sec(&key,&m.ours);
    if (rc != 0 && rc != BLE_HS_ENOENT) return false;
    m.oursPresent = rc == 0;
    ++out.count;
    return true;
}
bool import(Members& out) {
    Legacy legacy{};
    nvs_handle_t old;
    const auto opened = nvs_open("hanime-s3-kbd",NVS_READONLY,&old);
    if (opened == ESP_OK) {
        const bool ok = readLegacy(old,legacy);
        nvs_close(old);
        if (!ok) return false;
    } else if (opened != ESP_ERR_NVS_NOT_FOUND) return false;
    out.format = format;
    const unsigned n = legacy.hasOrder ? legacy.count : capacity;
    for (unsigned i=0; i<n; ++i) {
        ble_store_key_sec key{};
        if (legacy.hasOrder) key.peer_addr = legacy.order[i];
        else key.idx = i;
        ble_store_value_sec peer{};
        const int rc = ble_store_read_peer_sec(&key,&peer);
        if (rc == BLE_HS_ENOENT && !legacy.hasOrder) break;
        if (rc || !importMember(out,legacy,peer)) return false;
    }
    return valid(out);
}
} // namespace

bool begin() {
    if (nvs_open("ws-hid-v1",NVS_READWRITE,&handle) != ESP_OK) return false;
    size_t size = sizeof(members);
    const auto rc = nvs_get_blob(handle,"members",&members,&size);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        if (!import(members)) return false;
        // Bind this snapshot before replacing callbacks so discarded legacy
        // objects can never reappear after an ordinary reboot.
        if (!save(members)) return false;
    } else if (rc != ESP_OK || size != sizeof(members) || !valid(members)) return false;

    const bool installed = restoreStoreCallbacks();
    ready = installed;
    return installed;
}
bool restoreStoreCallbacks() {
    // Host resync on this configuration may rerun ble_store_config_init().
    // Capture native delegates only once, then reconcile each pointer. Never
    // install a wrapper as its own fallback.
    ble_hs_lock();
    if (!previousRead && !previousWrite && !previousDelete) {
        previousRead = ble_hs_cfg.store_read_cb;
        previousWrite = ble_hs_cfg.store_write_cb;
        previousDelete = ble_hs_cfg.store_delete_cb;
        previousStatus = ble_hs_cfg.store_status_cb;
    }
    const bool validCallbacks = previousRead && previousWrite && previousDelete &&
        previousRead != readRecord && previousWrite != writeRecord && previousDelete != deleteRecord &&
        (ble_hs_cfg.store_read_cb == previousRead || ble_hs_cfg.store_read_cb == readRecord) &&
        (ble_hs_cfg.store_write_cb == previousWrite || ble_hs_cfg.store_write_cb == writeRecord) &&
        (ble_hs_cfg.store_delete_cb == previousDelete || ble_hs_cfg.store_delete_cb == deleteRecord) &&
        (ble_hs_cfg.store_status_cb == previousStatus || ble_hs_cfg.store_status_cb == capacityNotice);
    if (validCallbacks) {
        ble_hs_cfg.store_read_cb = readRecord;
        ble_hs_cfg.store_write_cb = writeRecord;
        ble_hs_cfg.store_delete_cb = deleteRecord;
        ble_hs_cfg.store_status_cb = capacityNotice;
    }
    ble_hs_unlock();
    return validCallbacks;
}
bool healthy() { return ready.load(); }

bool copyPrivacyPeers(ws_privacy::PeerList& out) {
    out = {};
    if (!healthy()) return false;
    portENTER_CRITICAL(&mux);
    for (unsigned i=0; i<members.count; ++i) {
        const auto& peer = members.entries[i].peer;
        if (!peer.irk_present) continue;
        auto& entry = out.entries[out.count++];
        entry.identity = peer.peer_addr;
        std::memcpy(entry.irk, peer.irk, sizeof(entry.irk));
    }
    portEXIT_CRITICAL(&mux);
    return true;
}
bool resolve(const ble_addr_t& observed, ble_addr_t& canonical) {
    const Match m = matchAddress(observed);
    portENTER_CRITICAL(&mux);
    const bool matched = m.generation == generation && m.index >= 0;
    canonical = matched ? members.entries[m.index].peer.peer_addr : normalized(observed);
    portEXIT_CRITICAL(&mux);
    return matched;
}
unsigned slot(const ble_addr_t& canonical) {
    portENTER_CRITICAL(&mux);
    const unsigned result = static_cast<unsigned>(1 + position(members,canonical));
    portEXIT_CRITICAL(&mux);
    return result;
}
void startAttempt(bool known, const ble_addr_t& id) {
    portENTER_CRITICAL(&mux);
    ++generation;
    boundHandle = BLE_HS_CONN_HANDLE_NONE;
    previousKeyUsed = false;
    attempt = known ? Attempt::Existing : Attempt::Pairing;
    expected = normalized(id);
    pending = {};
    stagedPeer = false;
    inconsistent = false;
    portEXIT_CRITICAL(&mux);
}
bool bindConnection(const ble_gap_conn_desc& live) {
    if (live.conn_handle == BLE_HS_CONN_HANDLE_NONE) return false;
    const Match m = matchAddress(live.peer_id_addr);
    portENTER_CRITICAL(&mux);
    const bool verified = m.generation == generation &&
        (boundHandle == BLE_HS_CONN_HANDLE_NONE || boundHandle == live.conn_handle) &&
        ((attempt == Attempt::Existing && m.index >= 0 &&
          same(members.entries[m.index].peer.peer_addr,expected)) ||
         (attempt == Attempt::Pairing && same(live.peer_ota_addr,expected)));
    if (verified) boundHandle = live.conn_handle;
    portEXIT_CRITICAL(&mux);
    return verified;
}

int restoreEncryption(const ble_gap_conn_desc& live) {
    const Match m = matchAddress(live.peer_id_addr);
    ble_store_value_sec key{};
    portENTER_CRITICAL(&mux);
    const bool allowed = boundMatch(m,live.conn_handle) && usable(members.entries[m.index].peer);
    if (allowed) key = members.entries[m.index].peer;
    portEXIT_CRITICAL(&mux);
    if (!allowed) return BLE_HS_ENOENT;
    const int rc = ble_gap_encryption_initiate(live.conn_handle, key.key_size, key.ltk,
                                                key.ediv, key.rand_num, key.authenticated);
    portENTER_CRITICAL(&mux);
    const bool stillBound = boundMatch(m,live.conn_handle);
    if (rc == 0 && stillBound) previousKeyUsed = true;
    portEXIT_CRITICAL(&mux);
    return stillBound ? rc : BLE_HS_ENOTCONN;
}
bool verifiedExisting(const ble_gap_conn_desc& live) {
    if (!live.sec_state.encrypted || !live.sec_state.bonded || live.sec_state.key_size != 16)
        return false;
    const Match m = matchAddress(live.peer_id_addr);
    portENTER_CRITICAL(&mux);
    const bool verified = previousKeyUsed && boundMatch(m,live.conn_handle) &&
        (live.sec_state.authenticated || members.entries[m.index].boardConfirmed);
    portEXIT_CRITICAL(&mux);
    return verified;
}
bool hasNewKeys(const ble_addr_t& id) {
    portENTER_CRITICAL(&mux);
    const bool ok = attempt == Attempt::Pairing && !inconsistent && pending.oursPresent && stagedPeer &&
                    usable(pending.peer) && same(pending.peer.peer_addr,id);
    portEXIT_CRITICAL(&mux);
    return ok;
}
void cancelAttempt() {
    portENTER_CRITICAL(&mux);
    ++generation;
    boundHandle = BLE_HS_CONN_HANDLE_NONE;
    previousKeyUsed = false;
    attempt = Attempt::Idle;
    pending = {};
    stagedPeer = false;
    portEXIT_CRITICAL(&mux);
}
bool commit(const ble_gap_conn_desc& info, bool localConfirmation) {
    ble_gap_conn_desc live{};
    if (!healthy() || ble_gap_conn_find(info.conn_handle,&live) != 0 ||
        !live.sec_state.encrypted || !live.sec_state.bonded || live.sec_state.key_size != 16 ||
        live.conn_handle != info.conn_handle) return false;
    portENTER_CRITICAL(&mux);
    const bool known = attempt == Attempt::Existing;
    portEXIT_CRITICAL(&mux);
    if (known) return verifiedExisting(live);
    if (!same(live.peer_id_addr,info.peer_id_addr)) return false;
    const auto id = normalized(live.peer_id_addr);
    Members next{};
    portENTER_CRITICAL(&mux);
    const int existing = position(members,id);
    const bool approved = attempt == Attempt::Pairing && boundHandle == live.conn_handle && !inconsistent && pending.oursPresent && stagedPeer &&
            usable(pending.peer) && same(pending.peer.peer_addr,id) &&
            ((live.sec_state.authenticated && pending.peer.authenticated) || localConfirmation);
    if (!approved) {
        portEXIT_CRITICAL(&mux);
        return false;
    }
    // Admission decision linearizes here. Cancellation before this decision
    // drops the prospective member; a later disconnect cannot undo it.
    const Member candidate = pending;
    next = members;
    attempt = Attempt::Idle;
    portEXIT_CRITICAL(&mux);

    unsigned slot;
    if (existing >= 0) {
        slot = static_cast<unsigned>(existing);
    } else {
        if (next.count == capacity) {
            for (unsigned i=1; i<capacity; ++i) next.entries[i-1] = next.entries[i];
            --next.count;
        }
        slot = next.count++;
    }
    next.entries[slot] = candidate;
    next.entries[slot].boardConfirmed = localConfirmation && !candidate.peer.authenticated;
    if (!save(next)) { ready = false; return false; }
    portENTER_CRITICAL(&mux);
    members = next;
    ++generation;
    portEXIT_CRITICAL(&mux);
    ws_privacy::dirty();
    return true;
}

void endAttempt() {
    cancelAttempt();
}
} // namespace ws_bonds
