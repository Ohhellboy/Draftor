#include "idle_log.hpp"
#include "ui_trace.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ime_device;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)

struct Port : LogPort {
    bool online = true;
    size_t room = 4096, accept = 4096, largest = 0, calls = 0;
    std::string output;
    bool connected() const override { return online; }
    size_t available() override { return room; }
    size_t write(const char* text, size_t bytes) override {
        ++calls;
        largest = std::max(largest, bytes);
        const auto written = std::min(bytes, accept);
        output.append(text, written);
        return written;
    }
};


void busyTrace(){
 std::array<UiTraceRecord,UiTrace::Capacity> oldStorage{},newStorage{};
 UiTrace oldTrace(oldStorage.data()),trace(newStorage.data());oldTrace.setDetailed(true);
 IdleLog output;Port port;port.room=12;UiTraceRecord active;unsigned part=UiTraceParts;
 for(unsigned tick=0;tick<10000;++tick){
  if(tick%100==0)for(auto kind:{UiTraceKind::KeyDown,UiTraceKind::KeyUp,UiTraceKind::Duplicate,UiTraceKind::RowRequest,UiTraceKind::WindowReady,UiTraceKind::Lcd}){
   UiTraceRecord r;r.kind=kind;r.action=tick/100+1;r.row_count=3;r.ref=110039;r.key.bytes[31]=1;
   oldTrace.push(r);trace.push(r);
  }
  // Old transport was entirely gated by ongoing query work; new one runs after
  // the work slice. Model twelve FIFO bytes per turn, including occasional HID/save gates.
  const LogGate gate{tick%100==10,true,tick%100==20,false};
  output.service(port,gate);
  if(gate.transportReady())drainUiTracePart(trace,active,part,output);
 }
 for(unsigned tick=0;tick<10000&&(output.size()||trace.size()||part<UiTraceParts);++tick){
  output.service(port,{});drainUiTracePart(trace,active,part,output);
 }
 CHECK(oldTrace.lost()>0&&trace.lost()==0&&output.costs().dropped==0);
 CHECK(trace.aggregatedKeyups()==100&&trace.aggregatedDuplicates()==100);
 CHECK(port.output.find("action=100 stage=candidate_lcd")!=std::string::npos);
 CHECK(port.output.find("key=0000000000000000000000000000000000000000000000000000000000000001")!=std::string::npos);
 CHECK(port.largest<=IdleLog::WriteBytesPerTurn);
 // A full byte queue must not consume a record/part before it can be appended.
 for(unsigned i=0;i<IdleLog::Capacity;++i)CHECK(output.append("full\n"));
 UiTraceRecord r;r.kind=UiTraceKind::Lcd;trace.push(r);const auto queued=trace.size();
 CHECK(!drainUiTracePart(trace,active,part,output)&&trace.size()==queued&&part==UiTraceParts);
 std::cout<<"HOST_DIAGNOSTIC_MODEL old_no_query_drain_lost="<<oldTrace.lost()<<" repaired_lost="<<trace.lost()
          <<" normal_aggregated_keyups="<<trace.aggregatedKeyups()<<" duplicates="<<trace.aggregatedDuplicates()
          <<" queue_capacity_unchanged=1 not_device_timing=1\n";
}
int main() {
    try {
        busyTrace();
        IdleLog log;
        Port port;
        for (unsigned i = 0; i < IdleLog::Capacity; ++i) CHECK(log.printf("record=%u\n", i));
        CHECK(!log.append("overflow\n") && log.costs().dropped == 1);
        for (const auto gate : {LogGate{true, false, false, false},
                               LogGate{false, false, true, false}, LogGate{false, false, false, true}}) {
            for (unsigned i = 0; i < 100; ++i) log.service(port, gate);
        }
        CHECK(port.calls == 0 && log.size() == IdleLog::Capacity);
        port.online = false;
        for (unsigned i = 0; i < 100; ++i) log.service(port, {});
        CHECK(port.calls == 0 && log.costs().backpressure == 100);
        port.online = true; port.room = 0; log.service(port, {});
        CHECK(port.calls == 0);
        port.room = 200; port.accept = 3;
        for (unsigned i = 0; log.size() && i < 2000; ++i) log.service(port, {});
        CHECK(log.size() == 0 && log.costs().short_writes > 0);
        std::string expected;
        for (unsigned i = 0; i < IdleLog::Capacity; ++i) expected += "record=" + std::to_string(i) + '\n';
        CHECK(port.output == expected && port.largest <= IdleLog::WriteBytesPerTurn);
        CHECK(!log.append(std::string(IdleLog::LineBytes, 'x') + '\n'));
        CHECK(!log.printf("%s\n", std::string(IdleLog::LineBytes, 'x').c_str()));
        CHECK(log.costs().oversized == 2 && log.size() == 0);
        CHECK(log.append(std::string(700, 'a') + '\n'));
        port.accept = 4096; const auto calls = port.calls;
        log.service(port, {});
        CHECK(port.calls == calls + 1 && log.size() == 1);
        while (log.size()) log.service(port, {});
        CHECK(port.output == expected + std::string(700, 'a') + '\n');
        std::cout << "PASS bounded queue; keys/save/barrier priority; query transport bounded; disconnected/full/short writes; no line splicing\n";

        LoopYield yields;
        CHECK(yields.delayMs(100, true) == 1);
        CHECK(yields.delayMs(1100, true) == 0);
        CHECK(yields.delayMs(3100, true) == 0);
        CHECK(yields.delayMs(4100, true) == 1);
        CHECK(yields.delayMs(5100, false) == 1);
        CHECK(yields.delayMs(55100, true) == 1); // An SD call can exceed the interval.
        std::cout << "PASS foreground yield keeps periodic one-tick blocking; synchronous-call overrun not hidden\n";
        std::cout << "HOST_ONLY log_payload=" << IdleLog::Capacity * IdleLog::LineBytes
                  << " owner=" << sizeof(IdleLog) << " write_per_turn=" << IdleLog::WriteBytesPerTurn << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
