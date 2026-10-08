#pragma once
#include "tf_resources.hpp"
namespace ime_device {
struct ProbeResult {
 unsigned id=0;size_t request_bytes=0;
 uint32_t crc=0,expected=0;
 uint64_t first_us=0,last_us=0,exec_us=0,max_read_us=0,max_seek_us=0;
 IoCounters io{};bool ok=false;
};
// Explicit serial diagnostic, disabled by default. No resource activation or
// document write. One bounded read per service, separate from startup timing.
class TfProbe {
 std::string path_;std::shared_ptr<const Mount> mount_;uint64_t epoch_=0;
 std::unique_ptr<TfReader> reader_;std::array<uint8_t,4096> buffer_{};
 ProbeResult result_{};unsigned sample_=0,case_=0;uint32_t crc_=0xffffffffU;
 uint64_t requested_=0;bool pending_=false,finished_=false;const char* error_="";
 void fail(const char*);
 public:
 static constexpr unsigned Cases=14,Samples=8;
 static constexpr uint64_t LimitUs=60000000;
 // Diagnostic object/buffer has an explicit PSRAM lifetime on the device.
 static void* operator new(size_t n){auto* p=draftor::memory::allocate(n);if(!p)throw std::bad_alloc();return p;}
 static void operator delete(void* p)noexcept{draftor::memory::release(p);}
 TfProbe(std::string,std::shared_ptr<const Mount>);
 void service();
 void expireAt(uint64_t now){if(!finished_&&now-requested_>=LimitUs)fail("60s observation limit; no automatic retry");}
 uint64_t requestedUs()const{return requested_;}
 bool hasResult()const{return pending_;}
 const ProbeResult& result()const{return result_;}
 void acknowledge(){pending_=false;}
 bool finished()const{return finished_;}
 const char* error()const{return error_;}
 void cancel(){fail("cancelled; resource owners retained");}
 static bool cached(unsigned c){return c>=8&&c<12;}
 static const char* pattern(unsigned c){return c<4||c==12?"sequential":c<8||c==13?"random":"cached_repeat";}
 static size_t requestSize(unsigned c){constexpr size_t sizes[]={16,64,256,1024};return c>=12?4096:sizes[c%4];}
 static size_t sampleOffset(unsigned c,unsigned sample);
};
}
