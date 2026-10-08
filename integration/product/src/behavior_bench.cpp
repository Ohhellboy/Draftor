#include "behavior_bench.hpp"
#if DRAFTOR_TELEMETRY_CAPTURE && defined(ARDUINO) && !DRAFTOR_RELEASE_BUILD
#include "hid_keyboard.hpp"
#include "device_metrics.hpp"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <driver/uart.h>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <new>
namespace ime_device::bench {
namespace {
constexpr unsigned Capacity=256;
struct Samples {std::uint64_t count{},max{};std::uint32_t values[Capacity]{};};
struct Ticket {std::uint32_t action{};std::uint64_t input{},choice{};bool ready{};};
struct Bench {
 Samples samples[Count]{};Ticket tickets[128]{};
 std::uint64_t f4{},last_memory{},commit_start{};
 std::uint32_t internal_min{UINT32_MAX},psram_min{UINT32_MAX},stack_min{UINT32_MAX};
 std::uint32_t sent{},reused{},failed{},retired_tickets{};
 bool active{},ending{},done{},awaiting_ack{},format_ok{true};unsigned row{},size{};
 char line[4096]{};
};
// Includes worst-case uint32 samples, uint64 counters, header, LF and NUL.
static_assert(Capacity * 11 + 256 + 2 < sizeof(Bench::line));
Bench* b=nullptr;
void append(const char* format, ...) noexcept {
 if(!b->format_ok)return;
 va_list args;va_start(args,format);
 const int n=std::vsnprintf(b->line+b->size,sizeof(b->line)-b->size,format,args);
 va_end(args);
 if(n<0||unsigned(n)>=sizeof(b->line)-b->size){b->format_ok=false;return;}
 b->size+=unsigned(n);
}
constexpr const char* names[]={"input_dispatch_age_us","input_to_candidate_ready_us","selection_to_editor_commit_us","horizontal_draw_us","vertical_draw_us","f4_to_frame_us","loop_us","service_turn_us","serial_drain_us"};
}
void setup() noexcept {if(void* p=heap_caps_malloc(sizeof(Bench),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))b=new(p)Bench;Serial.printf("[BT_BENCH_BOOT] allocation=%u bytes=%u sample_capacity=%u\n",b!=nullptr,unsigned(sizeof(Bench)),Capacity);}
void sample(Metric m,std::uint64_t us) noexcept {
 if(!b||!b->active)return;auto& s=b->samples[m];if(s.count<Capacity)s.values[s.count]=std::uint32_t(std::min(us,std::uint64_t(UINT32_MAX)));++s.count;s.max=std::max(s.max,us);
}
void input(const s3hid::Event& e,std::uint32_t a) noexcept {
 if(!b||!b->active)return;const auto now=clockUs();const auto start=e.received_us&&e.received_us<=now?e.received_us:now;
 b->tickets[a%128]={a,start,0};sample(InputAge,now-start);
}
void ready(std::uint32_t a) noexcept {if(b&&b->active){auto& t=b->tickets[a%128];if(a&&t.action==a&&t.input){if(!t.ready){sample(CandidateReady,clockUs()-t.input);t.ready=true;}}else ++b->retired_tickets;}}
void choose(std::uint32_t a) noexcept {if(b&&b->active){auto& t=b->tickets[a%128];if(a&&t.action==a)t.choice=clockUs();else ++b->retired_tickets;}}
void confirm(std::uint32_t a) noexcept {if(b&&b->active){const auto& t=b->tickets[a%128];b->commit_start=a&&t.action==a?t.choice:0;}}
void committed() noexcept {if(b&&b->active&&b->commit_start){sample(SelectionCommit,clockUs()-b->commit_start);b->commit_start=0;}}
void f4() noexcept {if(b&&b->active)b->f4=clockUs();}
void drawn(bool vertical,std::uint64_t started) noexcept {if(b&&b->active){const auto now=clockUs();sample(vertical?VerticalDraw:HorizontalDraw,now-started);if(b->f4){sample(F4Frame,now-b->f4);b->f4=0;}}}
void service(bool drained,std::uint32_t sent,std::uint32_t reused,std::uint32_t failed) noexcept {
 if(!b)return;
 // Only two sidecar control bytes; they never reach the keyboard/product router.
 if(!b->ending&&!b->done&&Serial.available()){
  const char ch=char(Serial.read());
  if(ch=='B'){std::memset(static_cast<void*>(b),0,sizeof(*b));b->internal_min=b->psram_min=b->stack_min=UINT32_MAX;b->active=true;b->sent=sent;b->reused=reused;b->failed=failed;}
  if(ch=='E'){
   b->active=false;b->ending=true;b->sent=sent-b->sent;b->reused=reused-b->reused;b->failed=failed-b->failed;
   BT_OBSERVE(stop());
  }
 }
 if(b->active&&clockUs()-b->last_memory>=1000000){b->last_memory=clockUs();b->internal_min=std::min(b->internal_min,std::uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));b->psram_min=std::min(b->psram_min,std::uint32_t(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));b->stack_min=std::min(b->stack_min,std::uint32_t(uxTaskGetStackHighWaterMark(nullptr)));}
 if(!b->ending||!drained)return;
 // Stop-and-wait applies only after E, outside the measured interval.
 // The collector acknowledges a complete, archived record. At most one
 // bounded record can be outstanding even when archive I/O stalls.
 if(b->awaiting_ack){
  if(!Serial.available()||Serial.read()!='A')return;
  b->awaiting_ack=false;
 }
 if(b->size){
  // IDF's tx_mux covers the WHOLE call (including FIFO refill waits), shared
  // by HardwareSerial. No application diagnostic can split this record.
  // tx_buffer_size=0: the buffer is consumed before the call returns.
  const int written=uart_write_bytes(UART_NUM_0,b->line,b->size);
  if(written!=int(b->size)){
   // Fail closed: never continue a damaged record or emit a success trailer.
   b->ending=false;b->done=true;return;
  }
  b->size=0;
  b->awaiting_ack=b->row<=Count+2; // BT_BENCH_END requires no acknowledgement.
  return;
 }
 b->format_ok=true;
 if(b->row==0){
  append("\n[BT_CAPTURE_END]\n[BT_BENCH] internal_min=%u psram_min=%u stack_hwm_min=%u reset=%u sent=%u reused=%u failed=%u retired_tickets=%u sample_capacity=%u profile=%u\n",unsigned(b->internal_min),unsigned(b->psram_min),unsigned(b->stack_min),unsigned(esp_reset_reason()),unsigned(b->sent),unsigned(b->reused),unsigned(b->failed),unsigned(b->retired_tickets),Capacity,DRAFTOR_TELEMETRY_PROFILE);
 }else if(b->row<=Count){const auto& s=b->samples[b->row-1];
  append("[BT_SAMPLES] metric=%s count=%llu max_us=%llu kept=%u values=",names[b->row-1],(unsigned long long)s.count,(unsigned long long)s.max,unsigned(std::min(s.count,std::uint64_t(Capacity))));
  const unsigned kept=unsigned(std::min(s.count,std::uint64_t(Capacity)));
  unsigned emitted=0;
  for(;emitted<kept&&b->format_ok;++emitted)append("%s%u",emitted?",":"",unsigned(s.values[emitted]));
  append("\n");
  b->format_ok=b->format_ok&&emitted==kept;
 }else if(b->row==Count+1){
#if DRAFTOR_TELEMETRY_PROFILE
  if(const auto* h=behavior::health())append("[BT_HEALTH_FINAL] attempted=%llu enqueued=%llu dropped_queue=%llu dropped_invalid=%llu sink_busy=%llu dropped_sink_failed=%llu dropped_encode=%llu transport_failures=%llu queue_high_water=%u frames_accepted=%llu bytes_accepted=%llu\n",(unsigned long long)h->attempted,(unsigned long long)h->enqueued,(unsigned long long)h->dropped_queue,(unsigned long long)h->dropped_invalid,(unsigned long long)h->sink_busy,(unsigned long long)h->dropped_sink_failed,(unsigned long long)h->dropped_encode,(unsigned long long)h->transport_failures,unsigned(h->queue_high_water),(unsigned long long)h->frames_accepted,(unsigned long long)h->bytes_accepted);
#endif
 }else if(b->row==Count+2){append("[BT_BENCH_END]\n");}
 else {b->ending=false;b->done=true;}
 if(!b->format_ok){b->size=0;b->ending=false;b->done=true;return;}
 ++b->row;
}
}
#endif
