#include "operation_profile.hpp"
#include "behavior_observer.hpp"
#include <cstdio>
#ifdef ARDUINO
#include <esp_timer.h>
#include <hanime_sd_trace.h>
#include <cstring>
#include <Arduino.h>
#define PROFILE_PRINTF BT_PRINTF
#else
#include <chrono>
#define PROFILE_PRINTF std::printf
#endif
namespace s3profile {
Record record;
bool active = false;
uint64_t clockUs() noexcept {
#ifdef ARDUINO
 return esp_timer_get_time();
#else
 return std::chrono::duration_cast<std::chrono::microseconds>(
  std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
void begin(const char* name) noexcept {
 record={};record.operation=name;record.started=clockUs();active=true;
#ifdef ARDUINO
 hanime_sd_trace::stats={};hanime_sd_trace::enabled=true;
#endif
}
Record finish() noexcept {
 record.total_us=clockUs()-record.started;active=false;
#ifdef ARDUINO
 hanime_sd_trace::enabled=false;static_assert(sizeof(record.sd)==sizeof(hanime_sd_trace::stats));
 std::memcpy(static_cast<void*>(&record.sd), &hanime_sd_trace::stats, sizeof(record.sd));
#endif
 return record;
}
Scope::Scope(Stage stage) noexcept :stage_(stage),enabled_(active),start_(enabled_?clockUs():0),covered_(record.covered_us){}
Scope::~Scope(){
 if(!enabled_)return;
 // Child stages have already charged their exclusive time. Charge only this stage's remainder.
 auto elapsed=clockUs()-start_;auto children=record.covered_us-covered_;
 auto own=elapsed>children?elapsed-children:0;record.us[stage_]+=own;record.covered_us+=own;
}
void print(const Record& r,const char* status,size_t bytes,size_t cursor){
 const auto log_started=clockUs();
 PROFILE_PRINTF("[OPEN_PROFILE] op=%s status=%s bytes=%zu cursor=%zu total_us=%llu access_us=%llu read_us=%llu write_us=%llu verify_us=%llu install_us=%llu layout_us=%llu draw_us=%llu other_us=%llu main_step_max_us=%llu\n",
 r.operation,status,bytes,cursor,(unsigned long long)r.total_us,
 (unsigned long long)r.us[Access],(unsigned long long)r.us[Read],(unsigned long long)r.us[Write],
 (unsigned long long)r.us[Verify],(unsigned long long)r.us[Install],(unsigned long long)r.us[Layout],
 (unsigned long long)r.us[Draw],(unsigned long long)(r.total_us>r.covered_us?r.total_us-r.covered_us:0),(unsigned long long)r.max_main_step_us);
 PROFILE_PRINTF("[OPEN_IO] op=%s opens=%u entries=%u reads=%u read_bytes=%u max_read=%u read_le64=%u read_le512=%u read_le4096=%u read_gt4096=%u writes=%u write_bytes=%u crc_bytes=%u glyph_calls=%u glyph_probes=%u\n",
 r.operation,unsigned(r.opens),unsigned(r.entries),unsigned(r.reads),unsigned(r.read_bytes),unsigned(r.max_read),unsigned(r.read_sizes[0]),unsigned(r.read_sizes[1]),unsigned(r.read_sizes[2]),unsigned(r.read_sizes[3]),unsigned(r.writes),unsigned(r.write_bytes),unsigned(r.crc_bytes),unsigned(r.glyph_calls),unsigned(r.glyph_probes));
 const char* names[]={"open","close","stat","fstat","opendir","readdir","closedir","mkdir","unlink","rename","fsync"};
 uint64_t access_calls=0;
 for(unsigned i=0;i<CallCount;++i){
  if(i!=Fsync)access_calls+=r.calls[i].us;
  if(i%4==0)PROFILE_PRINTF("[ACCESS_CALLS] op=%s units=count/sum_us/max_us",r.operation);
  const auto& c=r.calls[i];PROFILE_PRINTF(" %s=%u/%llu/%llu",names[i],unsigned(c.count),(unsigned long long)c.us,(unsigned long long)c.max_us);
  if(i%4==3||i+1==CallCount)PROFILE_PRINTF("\n");
 }
 PROFILE_PRINTF("[ACCESS_OTHER] op=%s us=%llu (adapter/allocation/bookkeeping;fsync_in_write_us)\n",r.operation,
  (unsigned long long)(r.us[Access]>access_calls?r.us[Access]-access_calls:0));
#ifdef ARDUINO
 const char* sd_names[]={"disk_read","disk_write","status","sync","spi_acquire","busy_wait","token_wait","retry_delay"};
 for(unsigned i=0;i<hanime_sd_trace::PartCount;++i){
  if(i%4==0)PROFILE_PRINTF("[SD_CALLS] op=%s units=count/sum_us/max_us",r.operation);
  const auto& c=r.sd.part[i];PROFILE_PRINTF(" %s=%u/%llu/%llu",sd_names[i],unsigned(c.count),(unsigned long long)c.us,(unsigned long long)c.max_us);
  if(i%4==3)PROFILE_PRINTF("\n");
 }
 const auto& sd=r.sd;
 PROFILE_PRINTF("[SD_DETAIL] op=%s configured_hz=%u register_hz=%u read_sectors=%u write_sectors=%u busy_fail=%u token_fail=%u data_crc_fail=%u command_retry=%u\n",
 r.operation,unsigned(sd.configured_hz),unsigned(sd.register_hz),unsigned(sd.read_sectors),unsigned(sd.write_sectors),unsigned(sd.busy_fail),unsigned(sd.token_fail),unsigned(sd.data_crc_fail),unsigned(sd.command_retry));
 PROFILE_PRINTF("[SD_COMMANDS] op=%s",r.operation);
 for(unsigned i=0;i<64;++i)if(sd.commands[i])PROFILE_PRINTF(" cmd%u=%u",i,unsigned(sd.commands[i]));
 PROFILE_PRINTF("\n");
#endif
 PROFILE_PRINTF("[PROFILE_LOG] op=%s log_us=%llu\n",r.operation,(unsigned long long)(clockUs()-log_started));
}
}
