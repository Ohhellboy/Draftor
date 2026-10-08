#pragma once
#include <cstdint>
#include <cstddef>
#include <cerrno>
namespace s3profile {
// Main-loop only. One fixed record per user operation; no allocation or inner-loop logging.
enum Stage { Access, Read, Write, Verify, Install, Layout, Draw, StageCount };
enum Call { Open, Close, Stat, Fstat, Opendir, Readdir, Closedir, Mkdir, Unlink, Rename, Fsync, CallCount };
struct CallStats { uint64_t us=0, max_us=0; uint32_t count=0; };
struct SdStats {
 CallStats part[8]{};
 uint32_t read_sectors=0,write_sectors=0,commands[64]{};
 uint32_t busy_fail=0,token_fail=0,data_crc_fail=0,command_retry=0;
 uint32_t configured_hz=0,register_hz=0;
};
struct Record {
 const char* operation = "none";
 uint64_t max_main_step_us=0;
 uint64_t started = 0, total_us = 0, covered_us = 0, us[StageCount]{};
 CallStats calls[CallCount]{};
 SdStats sd;
 uint32_t opens = 0, entries = 0, reads = 0, read_bytes = 0, max_read = 0;
 uint32_t read_sizes[4]{}, writes = 0, write_bytes = 0;
 uint32_t glyph_calls = 0, glyph_probes = 0, crc_bytes = 0;
};
extern Record record;
extern bool active;
uint64_t clockUs() noexcept;
void begin(const char* operation) noexcept;
Record finish() noexcept;
void print(const Record&, const char* status, size_t bytes, size_t cursor);
// One measurement around each actual syscall, including failed calls and EOF readdir.
// Nested within the original stages; do not add these times again to total_us.
template<class F> auto call(Call kind, F&& fn) {
 if(!active) return fn();
 const auto start=clockUs();
 auto result=fn();
 const int saved_errno=errno;
 const auto elapsed=clockUs()-start;
 auto& stat=record.calls[kind];
 ++stat.count;stat.us+=elapsed;if(elapsed>stat.max_us)stat.max_us=elapsed;
 errno=saved_errno;
 return result;
}
class Scope {
 Stage stage_; bool enabled_; uint64_t start_, covered_;
 public:
 explicit Scope(Stage stage) noexcept;
 ~Scope();
 Scope(const Scope&) = delete;
 Scope& operator=(const Scope&) = delete;
};
inline void glyph(unsigned probes) noexcept {
 if(active){++record.glyph_calls;record.glyph_probes+=probes;}
}
}
