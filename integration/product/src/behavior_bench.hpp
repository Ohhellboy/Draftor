#pragma once
#include "behavior_observer.hpp"
#include "release_config.hpp"
#if DRAFTOR_TELEMETRY_CAPTURE && defined(ARDUINO) && !DRAFTOR_RELEASE_BUILD
#define BT_MEASURE(...) ::ime_device::bench::__VA_ARGS__
#include <cstdint>
namespace s3hid {struct Event;}
namespace ime_device::bench {
enum Metric {InputAge, CandidateReady, SelectionCommit, HorizontalDraw, VerticalDraw, F4Frame, Loop, ServiceTurn, SerialDrain, Count};
void setup() noexcept;
void sample(Metric,std::uint64_t) noexcept;
void input(const s3hid::Event&,std::uint32_t) noexcept;
void ready(std::uint32_t) noexcept;
void choose(std::uint32_t) noexcept;
void confirm(std::uint32_t) noexcept;
void committed() noexcept;
void f4() noexcept;
void drawn(bool vertical,std::uint64_t started) noexcept;
// B starts the bounded measurement window; E ends capture permanently and
// dumps structural samples only after the behavior queue/FIFO have drained.
void service(bool telemetry_drained,std::uint32_t sent,std::uint32_t reused,std::uint32_t failed) noexcept;
}
#else
#define BT_MEASURE(...) ((void)0)
#endif
