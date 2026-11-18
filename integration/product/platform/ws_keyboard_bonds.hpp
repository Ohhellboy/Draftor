#pragma once
#include <host/ble_gap.h>
namespace ws_privacy { struct PeerList; }

namespace ws_bonds {
bool begin();
bool healthy();
bool restoreStoreCallbacks();
bool copyPrivacyPeers(ws_privacy::PeerList&);
bool resolve(const ble_addr_t& observed, ble_addr_t& canonical);
unsigned slot(const ble_addr_t& canonical);
void startAttempt(bool known, const ble_addr_t& address);
bool bindConnection(const ble_gap_conn_desc& live);
int restoreEncryption(const ble_gap_conn_desc& live);
bool verifiedExisting(const ble_gap_conn_desc& live);
bool hasNewKeys(const ble_addr_t& canonical);
bool commit(const ble_gap_conn_desc& live, bool localConfirmation);
void cancelAttempt();
void endAttempt();
}
