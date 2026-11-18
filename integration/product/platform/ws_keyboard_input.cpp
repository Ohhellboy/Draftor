#include "ws_keyboard_input.hpp"
#include "hid_keyboard.hpp"

#include <Arduino.h>
#include <BLEClient.h>
#include <esp_timer.h>
#include <host/ble_gap.h>
#include <cstring>
#include <type_traits>
#include <array>

namespace keyboard {
namespace {
constexpr uint16_t NoConnection = BLE_HS_CONN_HANDLE_NONE;
constexpr size_t QueueSize = 512;
constexpr size_t MaxKeyboardReports = 8;  // s3hid::Tracker slots
constexpr size_t MaxInputs = 16;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Guard {
    Guard() { portENTER_CRITICAL(&mux); }
    ~Guard() { portEXIT_CRITICAL(&mux); }
};

// The queue is overwritten before any slot is read. Byte storage keeps its
// capacity in BSS; Event's nonzero field defaults would otherwise occupy Flash.
static_assert(std::is_trivially_copyable_v<s3hid::Event>);
struct EventSlot {
    alignas(s3hid::Event) unsigned char bytes[sizeof(s3hid::Event)];
};
std::array<EventSlot, QueueSize> queue;
size_t front = 0;
size_t queued = 0;
uint32_t eventId = 0;
s3hid::IngressContext ingress;
s3hid::Tracker tracker;
s3hid::Map descriptor;
input::Stats counters{1, 0, 0, 0, 0, false};

struct Source {
    BLERemoteCharacteristic* characteristic;
    const s3hid::Report* report;
    uint32_t updates;
    bool waitForBaseline;
};
std::array<Source, MaxKeyboardReports> keys{};
size_t keyCount = 0;
uint16_t connection = NoConnection;
bool prepared = false;
bool admitted = false;

void discardEvents() {
    front = queued = 0;
    ++counters.epoch;
}
void resetLink(uint16_t handle) {
    discardEvents();
    keyCount = 0;
    tracker.clear();
    prepared = admitted = counters.awaitRelease = false;
    connection = handle;
}

bool decode(const s3hid::Report& report, const uint8_t* bytes, size_t length, s3hid::State& state) {
    const size_t payload = (report.bits + 7u) / 8u;
    // Report Reference supplies the ID for normal HOGP. Some bridges also
    // include that same ID in the value. Never identify a layout by length alone.
    if (bytes && report.id && length == payload + 1 && bytes[0] == report.id) {
        ++bytes;
        --length;
    }
    return s3hid::Map::decode(report, bytes, length, state);
}

void enqueue(s3hid::Event event) {
    event.event_id = ++eventId;
    event.received_us = static_cast<uint64_t>(esp_timer_get_time());
    std::memcpy(queue[(front + queued) % QueueSize].bytes, &event, sizeof(event));
    ++queued;
    ++counters.received;
    if (queued > counters.peak) counters.peak = queued;
}

void notify(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool) {
    Guard lock;
    size_t slot = 0;
    while (slot < keyCount && keys[slot].characteristic != characteristic) ++slot;
    if (slot == keyCount) return;

    auto& source = keys[slot];
    ++source.updates;
    s3hid::State next;
    const uint32_t now = millis();
    if (!decode(*source.report, data, length, next)) {
        ++counters.bad;
        discardEvents();
        // Unknown/rollover state is not a release. Fence this report until a
        // subsequent valid report establishes that no keys are held.
        next.set(4);
        tracker.update(slot, next, counters.epoch, now, [](s3hid::Event) {});
        counters.awaitRelease = true;
        return;
    }
    if (source.waitForBaseline) {
        // An optional keyboard report may never be used. An unavailable
        // initial read fences only that report until it delivers a release.
        bool held = next.mods != 0;
        for (auto bits : next.keys) held |= bits != 0;
        if (held) return;
        source.waitForBaseline = false;
    }
    if (!admitted || counters.awaitRelease) {
        tracker.update(slot, next, counters.epoch, now, [](s3hid::Event) {});
        counters.awaitRelease = !tracker.allReleased();
        return;
    }
    bool overflow = false;
    s3hid::dispatchReport(tracker, slot, next, counters.epoch, now, ingress, [&](s3hid::Event event) {
        if (queued == QueueSize) overflow = true;
        else enqueue(event);
    });
    if (overflow) {
        ++counters.overflow;
        discardEvents();
        counters.awaitRelease = !tracker.allReleased();
    }
}

struct Endpoint {
    BLERemoteCharacteristic* characteristic;
    BLERemoteDescriptor* cccd;
    const s3hid::Report* keyboard;
    bool notifications;
};
struct Plan {
    std::array<Endpoint, MaxInputs> inputs{};
    size_t count = 0;
    size_t keyboards = 0;

    bool append(BLERemoteCharacteristic* characteristic, const s3hid::Report* report) {
        if (count == inputs.size() || (report && keyboards == MaxKeyboardReports)) return false;
        const bool notifySupported = characteristic->canNotify();
        if (!notifySupported && !characteristic->canIndicate()) return false;
        auto cccd = characteristic->getDescriptor(BLEUUID(uint16_t(0x2902)));
        if (!cccd) return false;
        inputs[count++] = {characteristic, cccd, report, notifySupported};
        if (report) ++keyboards;
        return true;
    }
};

bool discover(BLERemoteService& service, Plan& plan) {
    auto mode = service.getCharacteristic(BLEUUID(uint16_t(0x2a4e)));
    auto map = service.getCharacteristic(BLEUUID(uint16_t(0x2a4b)));
    if (map) {
        const String contents = map->readValue();
        if (descriptor.parse(reinterpret_cast<const uint8_t*>(contents.c_str()), contents.length())) {
            auto characteristics = service.getCharacteristicsByHandle();
            if (!characteristics) return false;
            for (const auto& entry : *characteristics) {
                auto c = entry.second;
                if (!c->getUUID().equals(BLEUUID(uint16_t(0x2a4d)))) continue;
                auto ref = c->getDescriptor(BLEUUID(uint16_t(0x2908)));
                if (!ref) return false;
                const String value = ref->readValue();
                if (value.length() != 2) return false;
                const uint8_t type = uint8_t(value[1]);
                if (type == 2 || type == 3) continue;  // Output / feature
                if (type != 1 || !plan.append(c, descriptor.report(uint8_t(value[0])))) return false;
            }
            if (plan.keyboards) return !mode || mode->writeValue(uint8_t(1), false);
        }
    }

    // A boot input has a fixed eight-byte format. Switch the peripheral into
    // Boot Protocol before using this characteristic.
    auto boot = service.getCharacteristic(BLEUUID(uint16_t(0x2a22)));
    if (!boot || !mode || !mode->writeValue(uint8_t(0), false)) return false;
    static constexpr uint8_t bootDescriptor[] = {
        0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x75,0x01,0x95,0x08,0x81,0x02,
        0x75,0x08,0x95,0x01,0x81,0x01,0x19,0x00,0x29,0xff,0x75,0x08,0x95,0x06,0x81,0x00,0xc0
    };
    if (!descriptor.parse(bootDescriptor, sizeof(bootDescriptor))) return false;
    plan = {};
    return plan.append(boot, descriptor.report(0));
}

bool subscribe(const Endpoint& endpoint) {
    auto c = endpoint.characteristic;
    // Subscribe all mapped Input characteristics, including consumer inputs.
    // notify() routes only characteristics with keyboard fields to the editor.
    if (!c->subscribe(endpoint.notifications, notify, true)) return false;
    const String bits = endpoint.cccd->readValue();
    const uint8_t expected = endpoint.notifications ? 1 : 2;
    return bits.length() == 2 && uint8_t(bits[0]) == expected && uint8_t(bits[1]) == 0;
}

bool sampleInitial(uint16_t handle, size_t slot, const Endpoint& endpoint) {
    uint32_t version;
    {
        Guard lock;
        if (handle != connection) return false;
        version = keys[slot].updates;
    }
    s3hid::State state;
    bool valid = false;
    if (endpoint.characteristic->canRead()) {
        const String value = endpoint.characteristic->readValue();
        valid = decode(*endpoint.keyboard, reinterpret_cast<const uint8_t*>(value.c_str()), value.length(), state);
    }
    Guard lock;
    if (connection != handle) return false;
    // A notification received since subscription is more reliable than a
    // possibly stale read. A missing sample must not release held PIN keys.
    if (version == 0 && keys[slot].updates == version) {
        keys[slot].waitForBaseline = !valid;
        if (valid) tracker.update(slot, state, counters.epoch, millis(), [](s3hid::Event) {});
    }
    return true;
}
}  // namespace

namespace input {
bool begin() { return true; }  // queue is statically allocated

void connected(uint16_t handle) {
    Guard lock;
    resetLink(handle);
}
void invalidate() {
    Guard lock;
    resetLink(NoConnection);
}

bool prepare(BLEClient& client) {
    const uint16_t handle = client.getConnId();
    {
        Guard lock;
        if (handle == NoConnection || connection != handle || prepared || keyCount) return false;
    }
    ble_gap_conn_desc link{};
    if (ble_gap_conn_find(handle, &link) || !link.sec_state.encrypted) return false;
    auto service = client.getService(BLEUUID(uint16_t(0x1812)));
    Plan plan;
    if (!service || !discover(*service, plan)) return false;
    {
        Guard lock;
        if (connection != handle) return false;
        for (size_t i = 0; i < plan.count; ++i) {
            auto& input = plan.inputs[i];
            if (input.keyboard) keys[keyCount++] = {input.characteristic, input.keyboard, 0, false};
        }
    }
    size_t slot = 0;
    for (size_t i = 0; i < plan.count; ++i) {
        const auto& input = plan.inputs[i];
        if (!client.isConnected() || !subscribe(input)) return false;
        if (input.keyboard && !sampleInitial(handle, slot++, input)) return false;
    }
    Guard lock;
    prepared = connection == handle;
    return prepared;
}

bool activate(uint16_t handle) {
    Guard lock;
    if (!prepared || connection != handle || handle == NoConnection) return false;
    counters.awaitRelease = !tracker.allReleased();
    admitted = true;
    return true;
}
Stats stats() {
    Guard lock;
    return counters;
}
}  // namespace input

bool pop(s3hid::Event& event) {
    Guard lock;
    if (!queued) return false;
    std::memcpy(&event, queue[front].bytes, sizeof(event));
    front = (front + 1) % QueueSize;
    --queued;
    return true;
}
bool admit(const s3hid::Event& event, uint32_t& observedEpoch) {
    Guard lock;
    observedEpoch = counters.epoch;
    return admitted && event.epoch == observedEpoch;
}
bool pending() {
    Guard lock;
    return queued != 0;
}
void publishFrame(uint32_t route, uint32_t generation) {
    Guard lock;
    ingress.route = route;
    ingress.frame = generation;
}
void setImeAttempt(uint32_t serial) {
    Guard lock;
    ingress.attempt = serial;
}
void publishImeContext(uint32_t serial, uint32_t resourceOwner) {
    Guard lock;
    ingress.ime_context = serial;
    ingress.resource_owner = resourceOwner;
}
}  // namespace keyboard
