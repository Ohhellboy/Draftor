#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#ifdef ARDUINO_ARCH_ESP32
#include <esp_cpu.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#define DRAFTOR_ST_INLINE __attribute__((always_inline)) inline
#else
#define DRAFTOR_ST_INLINE inline
#endif
namespace s3app::stability {
// Fixed S3 SDK atomics support external addresses through software paths, so
// std::atomic<uint32_t> does not promise always-lock-free. These control words
// MUST stay in internal DRAM. Each admission uses one SDK hardware CAS only.
class TryFlag {
 public:
 DRAFTOR_ST_INLINE bool test_and_set(std::memory_order order) noexcept {
#ifdef ARDUINO_ARCH_ESP32
  (void)order;const bool acquired=esp_cpu_compare_and_set(&word_,0,1);
  __asm__ __volatile__("memw" ::: "memory");return !acquired;
#else
  return flag_.test_and_set(order);
#endif
 }
 DRAFTOR_ST_INLINE void clear(std::memory_order order) noexcept {
#ifdef ARDUINO_ARCH_ESP32
  (void)order;__asm__ __volatile__("memw" ::: "memory");word_=0;
#else
  flag_.clear(order);
#endif
 }
 private:
#ifdef ARDUINO_ARCH_ESP32
 volatile std::uint32_t word_=0;
#else
 std::atomic_flag flag_=ATOMIC_FLAG_INIT;
#endif
};
// Each S3 core owns its counter. Brief local interrupt masking prevents a
// same-core ISR preemption from losing increments; there is no cross-core
// spin, retry, semaphore or heap. Snapshot reads may observe different times.
class LossCounter {
 public:
 DRAFTOR_ST_INLINE void fetch_add(unsigned n) noexcept {
#ifdef ARDUINO_ARCH_ESP32
  const auto level=portSET_INTERRUPT_MASK_FROM_ISR();
  const auto core=esp_cpu_get_core_id();counts_[core]=counts_[core]+n;
  portCLEAR_INTERRUPT_MASK_FROM_ISR(level);
#else
  count_.fetch_add(n);
#endif
 }
 unsigned load()const noexcept {
#ifdef ARDUINO_ARCH_ESP32
  __asm__ __volatile__("memw" ::: "memory");return counts_[0]+counts_[1];
#else
  return count_.load();
#endif
 }
 private:
#ifdef ARDUINO_ARCH_ESP32
 static_assert(portNUM_PROCESSORS==2,"diagnostics is fixed to the dual-core S3");
 volatile unsigned counts_[2]{};
#else
 std::atomic<unsigned> count_{0};
#endif
};
struct AllocationFailure {std::uint32_t size{},caps{},function{},count{},lost{};};
class FailureMailbox {
 public:
 static constexpr unsigned Capacity=4;
 DRAFTOR_ST_INLINE void put(std::uint32_t size,std::uint32_t caps,std::uint32_t function) noexcept {
  if(lock_.test_and_set(std::memory_order_acquire)){lost_.fetch_add(1);return;}
  ++total_;
  // Keep the first failures, including the pair seen in T01 B. Never overwrite
  // an unread tuple. Overflow remains visible as a sequence gap at drain time.
  if(count_<Capacity){
   auto& item=records_[(head_+count_)%Capacity];
   item.size=size;item.caps=caps;item.function=function;item.count=total_;
   ++count_;
  }
  lock_.clear(std::memory_order_release);
 }
 bool take(AllocationFailure& out,bool& detail) noexcept {
  if(lock_.test_and_set(std::memory_order_acquire))return false;
  detail=count_!=0;
  if(detail){out=records_[head_];head_=(head_+1)%Capacity;--count_;}
  else {out={};out.count=total_;}
  out.lost=lost_.load();lock_.clear(std::memory_order_release);return true;
 }
 private:
 TryFlag lock_;
 LossCounter lost_;
 std::array<AllocationFailure,Capacity> records_{};
 unsigned head_{},count_{},total_{};
};

struct Record {
 std::uint32_t ms{},kind{},a{},b{},c{},internal_free{},internal_largest{},internal_min{},psram_free{},psram_largest{},psram_min{},loop_hwm_bytes{},ble_worker_hwm_bytes{},sampled{},integrity{};
};
// A bounded, nonblocking diagnostic sidecar, allocated in PSRAM on S3. Writers
// never wait for each other or UART; contention/full drops NEW records visibly.
struct BufferControl {TryFlag lock;LossCounter lost;};
class Buffer {
 public:
 explicit Buffer(BufferControl& control) noexcept:control_(control){}
 static constexpr unsigned Capacity=16;
 bool push(const Record& r) noexcept {
  if(control_.lock.test_and_set(std::memory_order_acquire)){control_.lost.fetch_add(1);return false;}
  const bool room=count_<Capacity;
  if(room){records_[(head_+count_)%Capacity]=r;++count_;}else control_.lost.fetch_add(1);
  control_.lock.clear(std::memory_order_release);return room;
 }
 template<class Write> bool service(bool input_pending,Write write) noexcept {
  if(!remaining_){
   if(input_pending)return false;
   Record r;
   if(control_.lock.test_and_set(std::memory_order_acquire))return false;
   if(!count_){control_.lock.clear(std::memory_order_release);return false;}
   r=records_[head_];head_=(head_+1)%Capacity;--count_;control_.lock.clear(std::memory_order_release);
   const int n=std::snprintf(line_.data(),line_.size(),"[ST] seq=%u ms=%u kind=%u a=%u b=%u c=%u sampled=%u i_free=%u i_largest=%u i_min=%u p_free=%u p_largest=%u p_min=%u loop_hwm_bytes=%u ble_worker_hwm_bytes=%u integrity=%u dropped=%u\n",++sequence_,unsigned(r.ms),unsigned(r.kind),unsigned(r.a),unsigned(r.b),unsigned(r.c),unsigned(r.sampled),unsigned(r.internal_free),unsigned(r.internal_largest),unsigned(r.internal_min),unsigned(r.psram_free),unsigned(r.psram_largest),unsigned(r.psram_min),unsigned(r.loop_hwm_bytes),unsigned(r.ble_worker_hwm_bytes),unsigned(r.integrity),unsigned(control_.lost.load()));
   if(n<0||unsigned(n)>=line_.size()){control_.lost.fetch_add(1);return false;}
   remaining_=unsigned(n);offset_=0;
  }
  // A partially emitted line retains the turn while input is pending. Nothing
  // else may split it; no FIFO wait or formatting happens on the event path.
  if(!input_pending){const unsigned n=std::min(remaining_,64u);const auto used=std::min<unsigned>(n,write(line_.data()+offset_,n));offset_+=used;remaining_-=used;}
  return true;
 }
 unsigned dropped()const noexcept{return control_.lost.load();}
 private:
 std::array<Record,Capacity> records_{};
 std::array<char,512> line_{};
 BufferControl& control_;
 unsigned head_{},count_{},remaining_{},offset_{},sequence_{};
};
}

#undef DRAFTOR_ST_INLINE
