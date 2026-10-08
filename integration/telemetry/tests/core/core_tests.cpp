#include "draftor/telemetry/TelemetryEmitter.h"
#include "draftor/telemetry/SerialTelemetrySink.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace draftor::telemetry;
#define CHECK(expression) do { if (!(expression)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#expression); std::abort(); } } while (false)
struct WriterState { std::size_t calls{}, total{}, chunk{3}; bool blocked{}, disconnected{}; };
SerialWriteResult write_some(void* context, const std::uint8_t*, std::size_t size) noexcept {
    auto& state=*static_cast<WriterState*>(context); ++state.calls;
    if (state.disconnected) return {SerialWriteStatus::Disconnected, 0};
    if (state.blocked) return {SerialWriteStatus::WouldBlock, 0};
    const auto count=size < state.chunk ? size : state.chunk;
    state.total+=count; return {SerialWriteStatus::Progress,count};
}
BehaviorEvent input_event() {
    BehaviorEvent event{}; event.type=EventType::InputReceived; event.envelope.action_id=1; event.payload.input_received={1,0};
    event.detail.printable_codepoint=0x4e2d; event.detail.candidate_identity=0x12345678;
    event.detail.text_length=6; std::memcpy(event.detail.text,"SECRET",6);
    return event;
}
BehaviorEvent sync_event() {
    BehaviorEvent event{}; event.type=EventType::SyncSnapshot;
    event.payload.sync_snapshot={1,1,1,1,0,0,0,0}; return event;
}
void drain(TelemetryEmitter& emitter,SerialTelemetrySink& sink,WriterState& writer) {
    std::size_t budget=100000;
    while ((emitter.queued() || sink.pending()) && budget--) {
        const auto calls=writer.calls; emitter.service(sink);
        CHECK(writer.calls<=calls+1); // service never spins inside the writer.
    }
    CHECK(!emitter.queued() && !sink.pending());
}
void privacy_and_codec() {
    auto event=input_event(); BehaviorEvent filtered{};
    CHECK(filter_for_emission(event,PrivacyLevel::Basic,filtered)==FilterResult::Accepted);
    CHECK(filtered.detail.text_length==0 && filtered.detail.candidate_identity==0 && filtered.detail.printable_codepoint==0);
    CHECK(std::memcmp(filtered.detail.text,SensitiveDetail{}.text,kTextCapacity)==0);
    CHECK(filter_for_emission(event,PrivacyLevel::Detailed,filtered)==FilterResult::Accepted);
    CHECK(filtered.detail.text_length==6);
    TelemetryEmitter off(1,PrivacyLevel::Off);
    for (int i=0;i<1000;++i) CHECK(off.try_emit(event)==EmitResult::Disabled);
    CHECK(off.queued()==0 && off.health().attempted==0);
    TelemetryEmitter basic(2,PrivacyLevel::Basic);
    event.type=EventType::CandidateItemDetail;
    CHECK(basic.try_emit(event)==EmitResult::Suppressed && basic.health().attempted==0);
    event=input_event(); event.payload.input_received.physical_key=250;
    CHECK(basic.try_emit(event)==EmitResult::DroppedInvalid);
    event=sync_event(); event.payload.sync_snapshot.input_scheme=250;
    CHECK(basic.try_emit(event)==EmitResult::DroppedInvalid);
    TelemetryEmitter invalid(4,PrivacyLevel::Detailed); TestTelemetrySink sink;
    event.detail.text_length=97;
    CHECK(invalid.try_emit(event)==EmitResult::DroppedInvalid && !invalid.snapshot_required());
    event=sync_event(); event.detail.text_length=96; event.detail.path_length=64;
    std::memset(event.detail.text,0xc1,96); std::memset(event.detail.path,0xdb,64);
    CHECK(invalid.try_emit(event)==EmitResult::Enqueued); invalid.service(sink);
    CHECK(sink.last_size<=kMaxFrameBytes && sink.last_size>200);
    QueuedEvent item{}; item.event=event; item.privacy=PrivacyLevel::Detailed;
    WireScratch scratch;
    CHECK(encode_frame(item,scratch,1).status==EncodeStatus::BufferTooSmall);
    CHECK(encode_frame(item,scratch).status==EncodeStatus::Encoded);
    const std::uint8_t vector[]={'1','2','3','4','5','6','7','8','9'};
    CHECK(crc16_ccitt(vector,9)==0x29b1);
}
void partial_progress_retains_head() {
    TelemetryEmitter emitter(10,PrivacyLevel::Basic); WriterState writer;
    SerialTelemetrySink sink(write_some,&writer);
    for (std::size_t i=0;i<kQueueCapacity;++i) CHECK(emitter.try_emit(input_event())==EmitResult::Enqueued);
    emitter.service(sink); const auto queued=emitter.queued();
    emitter.service(sink);
    CHECK(emitter.queued()==queued && emitter.health().sink_busy==1);
    CHECK(!emitter.snapshot_required());
    drain(emitter,sink,writer);
    CHECK(emitter.health().frames_accepted==kQueueCapacity);
    CHECK(emitter.health().bytes_accepted==writer.total);
    CHECK(emitter.health().dropped_queue==0 && emitter.health().dropped_sink_failed==0);
    CHECK(emitter.health().queue_high_water==kQueueCapacity && !emitter.snapshot_required());
}
void backpressure_and_real_saturation() {
    TelemetryEmitter emitter(20,PrivacyLevel::Basic); WriterState writer; writer.blocked=true;
    SerialTelemetrySink sink(write_some,&writer);
    CHECK(emitter.try_emit(input_event())==EmitResult::Enqueued); emitter.service(sink);
    for (std::size_t i=0;i<kQueueCapacity;++i) {
        CHECK(emitter.try_emit(input_event())==EmitResult::Enqueued); emitter.service(sink);
        CHECK(emitter.health().dropped_queue==0 && !emitter.snapshot_required());
    }
    CHECK(emitter.queued()==kQueueCapacity);
    const auto calls=writer.calls;
    for (int i=0;i<1000000;++i) CHECK(emitter.try_emit(input_event())==EmitResult::DroppedQueueFull);
    CHECK(writer.calls==calls && emitter.health().dropped_queue==1000000);
    CHECK(emitter.health().dropped_sink_failed==0 && emitter.snapshot_required());
    writer.blocked=false; drain(emitter,sink,writer);
    CHECK(emitter.health().frames_accepted==kQueueCapacity+1);
    CHECK(emitter.health().bytes_accepted==writer.total);
    CHECK(emitter.try_emit(sync_event())==EmitResult::Enqueued); emitter.service(sink);
    CHECK(!emitter.snapshot_required());
    writer.disconnected=true; emitter.service(sink);
    CHECK(emitter.snapshot_required() && emitter.health().transport_failures==1);
    CHECK(emitter.health().dropped_sink_failed==0);
}
void failure_accounting_and_epochs() {
    TestTelemetrySink sink; TelemetryEmitter emitter(30,PrivacyLevel::Basic);
    emitter.try_emit(input_event()); sink.response=SinkResult::Busy;
    emitter.service(sink); CHECK(emitter.queued()==1 && !emitter.snapshot_required());
    sink.response=SinkResult::Failed; emitter.service(sink);
    CHECK(emitter.queued()==0 && emitter.health().dropped_sink_failed==1 && emitter.snapshot_required());
    emitter.try_emit(sync_event());
    for (std::size_t i=1;i<kQueueCapacity;++i) emitter.try_emit(input_event());
    emitter.try_emit(input_event()); sink.response=SinkResult::Accepted; emitter.service(sink);
    CHECK(emitter.snapshot_required()); // A pre-loss snapshot cannot acknowledge later loss.
    const auto health=emitter.health_event(5).payload.telemetry_health;
    CHECK(health.sink_busy==1 && health.dropped_sink_failed==1 && health.dropped_queue==1);
    CHECK(health.frames_accepted==1 && health.bytes_accepted==sink.last_size);
    CHECK(health.dropped_encode==0 && health.queue_high_water==kQueueCapacity);
}
int main() {
    privacy_and_codec(); partial_progress_retains_head(); backpressure_and_real_saturation(); failure_accounting_and_epochs();
    std::puts("PASS core: privacy/OFF/CRC/bounds, retained Busy head, lossless partial writes, one-million non-waiting queue drops, classified health, resync epoch, explicit scratch");
}
