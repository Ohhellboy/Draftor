#pragma once
#include <cstdint>
#include <cstddef>
#ifndef DRAFTOR_WS_HID_DIAG
#define DRAFTOR_WS_HID_DIAG 0
#endif
namespace hidprobe {
enum Stat:unsigned { LoopBegin,LoopEnd,MainPoint,MainPointMs,LastKey,RawCount,RxPoint,RxPointMs,Decoded,Bad,AwaitDrop,Enqueued,QueueOverflow,Popped,AdmitDrop,RouteDrop,BleState,Epoch,Handle,RawDrop,MainDrop,HeapStop,KeyDone,NotifySeen,ConnRejected,ConnectedRejected,ActualConn,ExpectedConn,NotifyHandle,HostEntered,HostExited,HostType,HostPoint,HostPointMs,ClientFlags,OtherInput,StatCount };
enum Point:unsigned { Enter=1,Status=2,Visual=3,EpochCheck=4,Present=5,Pop=6,Admit=7,Route=8,Usb=9,Layout=10,Ime=11,AppKey=12,Service=13,Background=14,FinalPresent=15,End=16,Stopped=17 };
enum Rx:unsigned { Gap=1,CacheWait=2,CacheDone=3,Callback=4,CallbackDone=5,GapDone=6,StateLockWait=7,StateLockHeld=8,DecodeFailed=9,AwaitRelease=10,Dispatch=11,NotifyDone=12,CacheAcquired=13,CacheStored=14 };
// R4 subscription receipts are produced only by the existing keyboard worker.
enum InputStatus:unsigned {InputFound=1,InputEnabled=2,InputVerified=3,InputWriteFailed=4,InputReadFailed=5,InputMismatch=6,InputDisconnected=7};
enum SetupStatus:unsigned {SetupPending=0,SetupReport=1,SetupBoot=2,SetupNoService=3,SetupNoRef=4,SetupBadRef=5,SetupNoNotify=6,SetupNoCccd=7,SetupInputCapacity=8,SetupKeyboardCapacity=9,SetupProtocolFailed=10,SetupWriteFailed=11,SetupReadFailed=12,SetupMismatch=13,SetupDisconnected=14,SetupNoBoot=15,SetupAllocationFailed=16,SetupWrongContext=17};
#if DRAFTOR_WS_HID_DIAG
void inc(Stat) noexcept;
void hostEvent(unsigned type,bool entering) noexcept;
void notifyGate(uint16_t actual,uint16_t expected,bool connected,uint16_t handle,uint32_t flags) noexcept;
void discovery() noexcept;
void inputSubscription(uint16_t handle,uint16_t cccd,uint8_t id,bool keyboard,InputStatus status,size_t readLength=0,uint16_t value=0) noexcept;
void setupDone(SetupStatus status,uint16_t handle,uint32_t epoch) noexcept;
void bind(uint32_t epoch) noexcept;
void causal(unsigned action,uint32_t value) noexcept;
void point(Point) noexcept;
void rxPoint(Rx,uint16_t handle=0) noexcept;
void raw(uint16_t handle,const uint8_t* bytes,size_t head,size_t total,size_t copied) noexcept;
void map(const uint8_t*,size_t,bool parsed) noexcept;
void reference(uint16_t handle,uint8_t id,uint8_t type,uint16_t bits,bool selected) noexcept;
void key(uint8_t usage,uint8_t mods,bool down) noexcept;
void keyDone() noexcept;
void r1(unsigned stage,uint32_t value) noexcept;
void monitor(unsigned phase,uint32_t epoch,bool accepting) noexcept;
void stopTransport() noexcept;
#else
inline void hostEvent(unsigned,bool) noexcept{}
inline void notifyGate(uint16_t,uint16_t,bool,uint16_t,uint32_t) noexcept{}
inline void inputSubscription(uint16_t,uint16_t,uint8_t,bool,InputStatus,size_t=0,uint16_t=0) noexcept{}
inline void setupDone(SetupStatus,uint16_t,uint32_t) noexcept{}
inline void discovery() noexcept{} inline void bind(uint32_t) noexcept{} inline void causal(unsigned,uint32_t) noexcept{}
inline void inc(Stat) noexcept{} inline void point(Point) noexcept{} inline void rxPoint(Rx,uint16_t=0) noexcept{}
inline void raw(uint16_t,const uint8_t*,size_t,size_t,size_t) noexcept{} inline void map(const uint8_t*,size_t,bool) noexcept{}
inline void reference(uint16_t,uint8_t,uint8_t,uint16_t,bool) noexcept{}
inline void key(uint8_t,uint8_t,bool) noexcept{} inline void keyDone() noexcept{} inline void r1(unsigned,uint32_t) noexcept{}
inline void monitor(unsigned,uint32_t,bool) noexcept{} inline void stopTransport() noexcept{}
#endif
struct HostScope {unsigned type;explicit HostScope(unsigned t)noexcept:type(t){hostEvent(type,true);}~HostScope(){hostEvent(type,false);}};
struct LoopScope {LoopScope()noexcept{inc(LoopBegin);point(Enter);}~LoopScope(){inc(LoopEnd);} };
struct KeyScope {KeyScope(uint8_t u,uint8_t m,bool d)noexcept{key(u,m,d);}~KeyScope(){keyDone();}};
}
