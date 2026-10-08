#pragma once
#include <cstdint>
namespace s3app::stability {
enum class Kind:std::uint32_t {Boot=1,Ready,BleBoundary,UsbBefore,UsbReady,UsbDenied,UsbPhase,HeapFailure,BleRead,Save,HeapCorrupt,FailureMailboxLoss,Memory,BlePhase,ImeMode,ImeReady};
void setup() noexcept;
// No allocation, no serial, bounded best effort. Safe in ordinary task context;
// the heap-failure hook uses a separate internal-only atomic mailbox.
void event(Kind,std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0) noexcept;
bool capture(Kind,std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0,bool integrity=false) noexcept;
// Product loop only, after its stop gate. Six same-caps statistics every 2 s;
// no integrity traversal, stack scan, allocation or catch-up samples.
void periodic(std::uint32_t now_ms,std::uint32_t phase) noexcept;
// Call only when the existing telemetry sink has no partial frame. A true
// result owns this loop's UART turn (including all partial record continuations).
bool service(bool input_pending) noexcept;
}
