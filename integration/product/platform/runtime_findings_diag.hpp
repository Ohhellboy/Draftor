#pragma once
#include <cstdint>
#include <cstddef>
#ifndef DRAFTOR_WS_RUNTIME_DIAG
#define DRAFTOR_WS_RUNTIME_DIAG 0
#endif
namespace s3profile {struct Record;}
namespace s3app::runtime_diag {
// R1-only finite experiment. No text, names, keys other than F9, or TF writes.
enum class Stage : uint8_t {
 F9Pop=1, AdmissionDrop, RouteDrop, UsbKey, IntegrityBegin, IntegrityPass,
 IntegrityFail, AllocateBegin, AllocateDenied, AllocateReady, DispatchBegin,
 DispatchEnd, TickBegin, TickEnd, PaintBegin, PaintEnd, PresentBegin,
 PresentEnd, RoutePublished, Heartbeat, TransportEnd, AllocationException,
 OperationBegin, OperationTotalUs, VfsReadUs, VfsWriteUs, VfsSyncUs,
 VfsOtherUs, VerifyUs, InstallUs, LayoutUs, DrawUs, VfsReadCalls,
 VfsWriteCalls, VfsSyncCalls, LcdUs, LcdResult
};
struct Record {uint32_t magic=0x31524457,us=0;uint16_t sequence=0;Stage stage{};uint8_t lost=0;uint32_t value=0;};
static_assert(sizeof(Record)==16,"R1 wire ABI");
// Single producer/consumer: product loop only. Partial writes preserve the
// front packet. Overflow drops NEW diagnostics, never product work.
struct Ring {
 static constexpr unsigned Capacity=32, Budget=768;
 Record records[Capacity]{};unsigned head=0,count=0,offset=0,issued=0,dropped=0;
 bool push(Stage stage,uint32_t us,uint32_t value) noexcept {
  if(issued>=Budget)return false;
  const auto sequence=issued++;
  if(count==Capacity){++dropped;return false;}
  auto& r=records[(head+count)%Capacity];r={0x31524457,us,uint16_t(sequence),stage,uint8_t(dropped>255?255:dropped),value};++count;return true;
 }
 template<class Write> void service(Write write) noexcept {
  if(!count)return;
  const auto* bytes=reinterpret_cast<const uint8_t*>(&records[head]);
  auto n=write(bytes+offset,sizeof(Record)-offset);
  if(n>sizeof(Record)-offset)n=sizeof(Record)-offset;
  offset+=n;if(offset==sizeof(Record)){offset=0;head=(head+1)%Capacity;--count;}
 }
};
#if DRAFTOR_WS_RUNTIME_DIAG
void mark(Stage,uint32_t value=0) noexcept;
void service() noexcept;
void f9() noexcept;
void heartbeat() noexcept;
void endTransport() noexcept;
void tickBegin(unsigned phase) noexcept;
void tickEnd(unsigned phase) noexcept;
void operationBegin() noexcept;
void operationEnd(const s3profile::Record&) noexcept;
void lcd(uint32_t elapsed,bool success) noexcept;
#else
inline void mark(Stage,uint32_t=0) noexcept {}
inline void service() noexcept {}
inline void f9() noexcept {}
inline void heartbeat() noexcept {}
inline void endTransport() noexcept {}
inline void tickBegin(unsigned) noexcept {}
inline void tickEnd(unsigned) noexcept {}
inline void operationBegin() noexcept {}
inline void operationEnd(const s3profile::Record&) noexcept {}
inline void lcd(uint32_t,bool) noexcept {}
#endif
}
