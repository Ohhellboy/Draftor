#pragma once
#include <cstdint>
#include <esp_timer.h>
// Diagnostic counters only. All TF access is on the main loop; no allocation/logging here.
namespace hanime_sd_trace {
enum Part { DiskRead, DiskWrite, Status, Sync, SpiAcquire, BusyWait, TokenWait, RetryDelay, PartCount };
struct Timing { uint64_t us=0, max_us=0; uint32_t count=0; };
struct Stats {
 Timing part[PartCount]{};
 uint32_t read_sectors=0, write_sectors=0, commands[64]{};
 uint32_t busy_fail=0, token_fail=0, data_crc_fail=0, command_retry=0;
 uint32_t configured_hz=0, register_hz=0;
};
extern bool enabled;
extern Stats stats;
class Scope {
 Part part_; bool enabled_; uint64_t start_;
public:
 explicit Scope(Part part):part_(part),enabled_(enabled),start_(enabled_?esp_timer_get_time():0){}
 ~Scope(){if(enabled_){auto& s=stats.part[part_];auto us=uint64_t(esp_timer_get_time())-start_;
  ++s.count;s.us+=us;if(us>s.max_us)s.max_us=us;}}
 Scope(const Scope&)=delete;
 Scope& operator=(const Scope&)=delete;
};
}
