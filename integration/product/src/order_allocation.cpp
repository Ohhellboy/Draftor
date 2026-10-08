#include "order_allocation.hpp"
#if defined(ARDUINO_ARCH_ESP32)
#include <atomic>
#include <new>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace {
ime_device::OrderAllocationSnapshot observation;
std::atomic<TaskHandle_t> observer{nullptr};
void record(void* ptr,std::size_t bytes,uint32_t kind){
 const auto task=observer.load(std::memory_order_acquire);
 if(!ptr||!task||task!=xTaskGetCurrentTaskHandle())return;
 const auto address=reinterpret_cast<std::uintptr_t>(ptr);
 for(uint32_t i=0;i<observation.count;++i)if(observation.records[i].address==address){observation.records[i].kind|=kind;return;}
 if(observation.count==8){++observation.overflow;return;}
 observation.records[observation.count++]={address,bytes,kind,esp_ptr_external_ram(ptr)?2u:esp_ptr_internal(ptr)?1u:0u};
}
}
// GNU ld wrappers call the original operators once. Nothrow keeps its original
// contract. Nested scalar/array/nothrow paths are deduplicated by the returned pointer.
extern "C" void* __real__Znwj(std::size_t);
extern "C" void* __real__Znaj(std::size_t);
extern "C" void* __real__ZnwjRKSt9nothrow_t(std::size_t,const std::nothrow_t&) noexcept;
extern "C" void* __real__ZnajRKSt9nothrow_t(std::size_t,const std::nothrow_t&) noexcept;
extern "C" void* __wrap__Znwj(std::size_t n){auto p=__real__Znwj(n);record(p,n,1);return p;}
extern "C" void* __wrap__Znaj(std::size_t n){auto p=__real__Znaj(n);record(p,n,2);return p;}
extern "C" void* __wrap__ZnwjRKSt9nothrow_t(std::size_t n,const std::nothrow_t& t) noexcept{auto p=__real__ZnwjRKSt9nothrow_t(n,t);record(p,n,5);return p;}
extern "C" void* __wrap__ZnajRKSt9nothrow_t(std::size_t n,const std::nothrow_t& t) noexcept{auto p=__real__ZnajRKSt9nothrow_t(n,t);record(p,n,6);return p;}
namespace ime_device {
OrderAllocationScope::OrderAllocationScope(){const auto next=observation.generation+1;observation={};observation.generation=next;observer.store(xTaskGetCurrentTaskHandle(),std::memory_order_release);}
OrderAllocationScope::~OrderAllocationScope(){observer.store(nullptr,std::memory_order_release);}
const OrderAllocationSnapshot& orderAllocationSnapshot(){return observation;}
}
#else
namespace ime_device {
OrderAllocationScope::OrderAllocationScope()=default;
OrderAllocationScope::~OrderAllocationScope()=default;
const OrderAllocationSnapshot& orderAllocationSnapshot(){static const OrderAllocationSnapshot unobserved{};return unobserved;}
}
#endif
