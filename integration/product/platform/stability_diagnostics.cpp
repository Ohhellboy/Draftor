#include "stability_diagnostics.hpp"
#include "stability_buffer.hpp"
#ifdef ARDUINO_ARCH_ESP32
#include "ble_keyboard_host.hpp"
#include <Arduino.h>
#include <driver/uart.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <new>
namespace s3app::stability {
namespace {
constexpr unsigned Internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,External=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
Buffer* buffer=nullptr;
DRAM_ATTR BufferControl bufferControl; // Atomic words always internal, payload/format storage PSRAM.
// Only this mailbox is touched from the allocation failure callback. No heap
// inspection, PSRAM, static-local initialization, locks or UART in that callback.
DRAM_ATTR FailureMailbox failures;
AllocationFailure observed{};
std::uint32_t lastSampleMs=0,sampleSequence=0;
void IRAM_ATTR allocationFailed(std::size_t size,std::uint32_t caps,const char* function) {
 failures.put(size,caps,reinterpret_cast<std::uintptr_t>(function));
}
Record record(Kind kind,std::uint32_t a,std::uint32_t b,std::uint32_t c) noexcept {
 Record r;r.ms=std::uint32_t(esp_timer_get_time()/1000);r.kind=std::uint32_t(kind);r.a=a;r.b=b;r.c=c;return r;
}
}
extern "C" __attribute__((used)) const volatile std::uint32_t s3_stability_abi[]={sizeof(Buffer),sizeof(Record),Buffer::Capacity,sizeof(FailureMailbox)+sizeof(observed)+sizeof(buffer)+sizeof(bufferControl)+sizeof(lastSampleMs)+sizeof(sampleSequence),Internal,External};
void setup() noexcept {
 const int hook=heap_caps_register_failed_alloc_callback(allocationFailed);
 if(void* p=heap_caps_malloc(sizeof(Buffer),External))buffer=new(p)Buffer(bufferControl);
 // Boot only, before input/telemetry starts; the steady-state transport below
 // is FIFO-only. Keep this line within HardwareSerial's small printf buffer.
 Serial.printf("[ST_INIT] ok=%u bytes=%u hook=%d reset=%u\n",buffer!=nullptr,unsigned(sizeof(Buffer)),hook,unsigned(esp_reset_reason()));
 capture(Kind::Boot,unsigned(esp_reset_reason()),Internal,External);
}
void event(Kind k,std::uint32_t a,std::uint32_t b,std::uint32_t c) noexcept {if(buffer)buffer->push(record(k,a,b,c));}
bool capture(Kind k,std::uint32_t a,std::uint32_t b,std::uint32_t c,bool integrity) noexcept {
 // Do not inspect more heap metadata or touch the PSRAM record buffer once
 // corruption is known. The caller enters its internal-only stop path.
 if(integrity&&(!heap_caps_check_integrity(Internal,false)||!heap_caps_check_integrity(External,false)))return false;
 Record r=record(k,a,b,c);r.sampled=1;
 r.internal_free=heap_caps_get_free_size(Internal);r.internal_largest=heap_caps_get_largest_free_block(Internal);r.internal_min=heap_caps_get_minimum_free_size(Internal);
 r.psram_free=heap_caps_get_free_size(External);r.psram_largest=heap_caps_get_largest_free_block(External);r.psram_min=heap_caps_get_minimum_free_size(External);
 r.loop_hwm_bytes=uxTaskGetStackHighWaterMark(nullptr);r.ble_worker_hwm_bytes=keyboard::workerStackMinimumBytes();
 if(integrity)r.integrity=1;
 if(buffer)buffer->push(r);
 return true;
}
void periodic(std::uint32_t now_ms,std::uint32_t phase) noexcept {
 const std::uint32_t elapsed=now_ms-lastSampleMs;
 if(elapsed<2000)return;
 lastSampleMs=now_ms;
 Record r=record(Kind::Memory,++sampleSequence,phase,elapsed/2000-1);r.sampled=1;
 r.internal_free=heap_caps_get_free_size(Internal);r.internal_largest=heap_caps_get_largest_free_block(Internal);r.internal_min=heap_caps_get_minimum_free_size(Internal);
 r.psram_free=heap_caps_get_free_size(External);r.psram_largest=heap_caps_get_largest_free_block(External);r.psram_min=heap_caps_get_minimum_free_size(External);
 // Zero stack/integrity fields mean NOT_SAMPLED, not a zero watermark or PASS.
 if(buffer)buffer->push(r);
}
bool service(bool input_pending) noexcept {
 if(!buffer)return false;
 AllocationFailure latest;bool detail=false;
 if(failures.take(latest,detail)){
  if(detail){event(Kind::HeapFailure,latest.size,latest.caps,latest.function);}
  const auto missing=latest.count-observed.count-unsigned(detail);
  if(missing||latest.lost!=observed.lost)event(Kind::FailureMailboxLoss,latest.count,missing,latest.lost);
  observed=latest;
 }
 return buffer->service(input_pending,[](const char* p,unsigned n){const int written=uart_tx_chars(UART_NUM_0,p,n);return written>0?unsigned(written):0u;});
}
}
#else
namespace s3app::stability {
void setup() noexcept {}
void event(Kind,std::uint32_t,std::uint32_t,std::uint32_t) noexcept {}
bool capture(Kind,std::uint32_t,std::uint32_t,std::uint32_t,bool) noexcept {return true;}
void periodic(std::uint32_t,std::uint32_t) noexcept {}
bool service(bool) noexcept {return false;}
}
#endif
extern "C" void draftor_ble_read_event(std::uint16_t connection,std::uint16_t handle,std::uint8_t stage,int status,std::uint16_t bytes) {
 s3app::stability::event(s3app::stability::Kind::BleRead,(unsigned(connection)<<16)|handle,(stage<<24)|(unsigned(status)&0xffffffu),unsigned(bytes));
}
