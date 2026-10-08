#pragma once
#ifndef DRAFTOR_TELEMETRY_PROFILE
#define DRAFTOR_TELEMETRY_PROFILE 0
#endif
#ifndef DRAFTOR_TELEMETRY_CAPTURE
#define DRAFTOR_TELEMETRY_CAPTURE 0
#endif
#if DRAFTOR_TELEMETRY_PROFILE < 0 || DRAFTOR_TELEMETRY_PROFILE > 2
#error Invalid telemetry privacy profile
#endif
// OFF removes argument evaluation, record construction, copies and service calls.
#if DRAFTOR_TELEMETRY_PROFILE
#define BT_OBSERVE(...) ::ime_device::behavior::__VA_ARGS__
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <draftor/telemetry/TelemetryEmitter.h>
namespace s3hid { struct Event; }
namespace s3app { class App; }
namespace hanime_extension { class Composition; }
namespace ime_device { class DeviceIme; class CandidateWindow; struct VisibleItem;
namespace behavior {
std::size_t storageBytes() noexcept;
void bind(void* storage, std::uint64_t boot, s3app::App&, DeviceIme*) noexcept;
void service(draftor::telemetry::TelemetrySink&, bool input_pending) noexcept;
void stop() noexcept;
bool drained() noexcept;
void input(const s3hid::Event&, std::uint32_t action) noexcept;
void raw(DeviceIme&, std::uint32_t action, unsigned cause) noexcept;
void ended(std::uint32_t action, unsigned outcome) noexcept;
void query(const hanime_extension::Composition&, const CandidateWindow&, std::size_t prefix) noexcept;
void range(const hanime_extension::Composition&, const CandidateWindow&) noexcept;
void window(const hanime_extension::Composition&, const CandidateWindow&) noexcept;
void selection(const CandidateWindow&, const VisibleItem&, std::uint32_t generation, std::uint32_t action) noexcept;
void selectionResult(const hanime_extension::Composition&, unsigned outcome) noexcept;
void navigation(const CandidateWindow&, std::uint32_t action, std::size_t row, std::size_t col, bool edge) noexcept;
void confirm(std::uint32_t action) noexcept;
void revalidated(const hanime_extension::Composition&, std::uint32_t action) noexcept;
void boundary() noexcept;
void commit(const s3app::App&, std::uint32_t before, std::size_t cursor) noexcept;
void mode(const s3app::App&, const s3hid::Event&) noexcept;
void displayBegin(const s3app::App&, const DeviceIme*, bool candidates) noexcept;
void displayEnd(bool success) noexcept;
void state() noexcept;
// A small stack receipt only. The event/encoder scratch lives with the observer.
class EditorScope {
 const s3app::App* app_;
 std::uint32_t before_, cursor_, action_;
 std::uint64_t session_;
 std::uint8_t usage_, mods_;
 public:
 EditorScope(const s3app::App&, const s3hid::Event&) noexcept;
 ~EditorScope();
};
const draftor::telemetry::TelemetryHealth* health() noexcept;
} }
#else
#define BT_OBSERVE(...) ((void)0)
#endif

namespace draftor_behavior_output { template<class... Args> inline void muted(Args&&...) noexcept {} }
#if DRAFTOR_TELEMETRY_CAPTURE
#define BT_PRINTF(...) ::draftor_behavior_output::muted(__VA_ARGS__)
#define BT_PRINTLN(...) ::draftor_behavior_output::muted(__VA_ARGS__)
#else
#define BT_PRINTF(...) Serial.printf(__VA_ARGS__)
#define BT_PRINTLN(...) Serial.println(__VA_ARGS__)
#endif
