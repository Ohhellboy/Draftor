#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
namespace ime_device {
uint64_t clockUs() noexcept;
enum class Phase : unsigned { Open, SharedValidation, Admission, SchemeValidation, Query, VisibleText, Submission, FontLoad, Navigation, FontIndex, FontContent, TfProbe, OrderValidation, RowPrefetch, Count };
// Logical ResourceReader requests are distinct from POSIX reads. Cache fill
// bytes include bounded read-ahead and are never used as a progress percentage.
struct IoCounters {
 uint64_t requests=0,requested_bytes=0,hits=0,hit_bytes=0,misses=0,reads=0,bytes=0,seeks=0,seek_us=0,read_us=0;
 IoCounters operator-(const IoCounters& b)const {
  return {requests-b.requests,requested_bytes-b.requested_bytes,hits-b.hits,hit_bytes-b.hit_bytes,misses-b.misses,reads-b.reads,bytes-b.bytes,seeks-b.seeks,seek_us-b.seek_us,read_us-b.read_us};
 }
 void add(const IoCounters& b){requests+=b.requests;requested_bytes+=b.requested_bytes;hits+=b.hits;hit_bytes+=b.hit_bytes;misses+=b.misses;reads+=b.reads;bytes+=b.bytes;seeks+=b.seeks;seek_us+=b.seek_us;read_us+=b.read_us;}
};
struct Measure { uint64_t steps=0,us=0,max_us=0,read_calls=0,read_bytes=0,work=0,first_us=0,last_us=0;IoCounters io{}; };
struct Metrics:IoCounters {
 Measure phases[unsigned(Phase::Count)]{};
 uint64_t max_read_us=0,max_seek_us=0,max_open_us=0;
 uint32_t resource_handles=0,document_handles=0,handle_peak=0;
 uint64_t stale_choices=0,reauthorizations=0,blocked=0,duplicates=0;
 size_t largest_read=0,cache_live_bytes=0,cache_peak_bytes=0,cache_alloc_failures=0;
 IoCounters io()const{return *this;}
 void openHandle(bool resource) { ++(resource?resource_handles:document_handles); handle_peak=std::max(handle_peak,resource_handles+document_handles); }
 void closeHandle(bool resource) { auto& n=resource?resource_handles:document_handles; if(n)--n; }
 void step(Phase phase,uint64_t start,size_t calls,size_t bytes_,size_t work_,const IoCounters& before) {
  auto& m=phases[unsigned(phase)];const auto end=clockUs();auto elapsed=end-start;
  if(!m.steps)m.first_us=start;
  m.last_us=end;++m.steps;m.us+=elapsed;m.max_us=std::max(m.max_us,elapsed);m.read_calls+=calls;m.read_bytes+=bytes_;m.work+=work_;m.io.add(io()-before);
 }
};
inline Metrics metrics;
}
