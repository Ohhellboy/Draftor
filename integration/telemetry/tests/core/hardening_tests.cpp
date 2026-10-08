#include "draftor/telemetry/TelemetryEmitter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using namespace draftor::telemetry;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); std::abort(); } } while (false)

BehaviorEvent input() {
    BehaviorEvent e{}; e.type=EventType::InputReceived;
    e.envelope.action_id=1; e.payload.input_received={1,0}; return e;
}
BehaviorEvent candidate() {
    BehaviorEvent e{}; e.type=EventType::CandidateDataReady;
    e.envelope.action_id=1; e.envelope.composition_id=2;
    e.envelope.composition_revision=1; e.envelope.candidate_snapshot_id=3;
    e.payload.candidate_data_ready={1,0,0,0,0,0,0}; return e;
}
BehaviorEvent state(EventType type=EventType::SessionStart) {
    BehaviorEvent e{}; e.type=type;
    if (type==EventType::SessionStart) e.payload.session_start={1,1,1,1,0,0,0,0};
    if (type==EventType::StateSnapshot) e.payload.state_snapshot={1,1,1,1,0,0,0,0};
    if (type==EventType::StateChange) e.payload.state_change={1,1,1,1,0,0,0,0};
    if (type==EventType::SyncSnapshot) e.payload.sync_snapshot={1,1,1,1,0,0,0,0};
    return e;
}
std::uint64_t sequence(const TestTelemetrySink& sink) {
    std::uint8_t packet[kMaxPacketBytes]{}; std::size_t size=0;
    for (std::size_t i=1;i+1<sink.last_size;++i) {
        auto byte=sink.last_frame[i];
        if (byte==0xdb) { const auto escaped=sink.last_frame[++i]; byte=escaped==0xdc ? 0xc0 : escaped==0xdd ? 0xdb : 0xc1; }
        packet[size++]=byte;
    }
    CHECK(size>=56); std::uint64_t value=0;
    for (int i=0;i<8;++i) value|=static_cast<std::uint64_t>(packet[16+i])<<(8*i);
    return value;
}
void rejected_once(const BehaviorEvent& invalid) {
    TelemetryEmitter emitter(1,PrivacyLevel::Detailed); TestTelemetrySink sink;
    CHECK(emitter.try_emit(invalid)==EmitResult::DroppedInvalid);
    CHECK(emitter.queued()==0 && emitter.health().attempted==1 && emitter.health().dropped_invalid==1);
    CHECK(emitter.health().dropped_queue==0 && emitter.health().dropped_sink_failed==0);
    CHECK(emitter.health().transport_failures==0 && emitter.health().enqueued==0 && !emitter.snapshot_required());
    emitter.service(sink); CHECK(sink.accepted_frames==0 && sink.last_size==0 && emitter.health().frames_accepted==0);
    CHECK(emitter.try_emit(input())==EmitResult::Enqueued); emitter.service(sink);
    CHECK(sequence(sink)==1); // Rejection never reserved sequence.
}
void missing_identities() {
    auto e=input(); e.envelope.action_id=0; rejected_once(e); // C01
    e=candidate(); e.envelope.composition_id=0; rejected_once(e); // C02
    e=candidate(); e.envelope.candidate_snapshot_id=0; rejected_once(e); // C03
    e=candidate(); e.envelope={}; rejected_once(e); // C04: counted once
    e=candidate(); e.envelope.composition_revision=0; rejected_once(e);
}
void legitimate_zero_and_conditions() { // C06: ordinary revisions/counts may start at zero.
    TelemetryEmitter emitter(2,PrivacyLevel::Basic); TestTelemetrySink sink;
    auto e=state(); CHECK(emitter.try_emit(e)==EmitResult::Enqueued);
    e=input(); e.type=EventType::DocumentMutation; e.payload.document_mutation={};
    CHECK(emitter.try_emit(e)==EmitResult::Enqueued); // document_revision=0
    e=input(); e.type=EventType::ViewReady; e.payload.view_ready={1,0};
    CHECK(emitter.try_emit(e)==EmitResult::Enqueued); // document/layout revision=0
    e={}; e.type=EventType::FileOpBegin; e.payload.file_op_begin={1,1};
    CHECK(emitter.try_emit(e)==EmitResult::Enqueued); // action_id legitimately absent
    for (const auto type:{EventType::SessionStart,EventType::StateSnapshot,EventType::StateChange,EventType::SyncSnapshot}) {
        e=state(type); e.envelope.composition_id=7; rejected_once(e);
        e.envelope.composition_revision=1; CHECK(emitter.try_emit(e)==EmitResult::Enqueued);
        e=state(type);
        if(type==EventType::SessionStart) e.payload.session_start.candidate_valid=1;
        if(type==EventType::StateSnapshot) e.payload.state_snapshot.candidate_valid=1;
        if(type==EventType::StateChange) e.payload.state_change.candidate_valid=1;
        if(type==EventType::SyncSnapshot) e.payload.sync_snapshot.candidate_valid=1;
        rejected_once(e); e.envelope.candidate_snapshot_id=3;
        CHECK(emitter.try_emit(e)==EmitResult::Enqueued);
    }
    while(emitter.queued()) emitter.service(sink);
    CHECK(emitter.health().dropped_invalid==0 && !emitter.snapshot_required());
}
void full_queue_classification() {
    TelemetryEmitter emitter(3,PrivacyLevel::Basic); TestTelemetrySink sink;
    for(std::size_t i=0;i<kQueueCapacity;++i) CHECK(emitter.try_emit(input())==EmitResult::Enqueued);
    auto invalid=input(); invalid.envelope.action_id=0;
    CHECK(emitter.try_emit(invalid)==EmitResult::DroppedInvalid);
    invalid=input(); invalid.payload.input_received.physical_key=255;
    CHECK(emitter.try_emit(invalid)==EmitResult::DroppedInvalid);
    BehaviorEvent detail{}; detail.type=EventType::CandidateItemDetail;
    CHECK(emitter.try_emit(detail)==EmitResult::Suppressed);
    CHECK(emitter.queued()==kQueueCapacity && emitter.health().dropped_invalid==2 && emitter.health().dropped_queue==0);
    CHECK(!emitter.snapshot_required());
    CHECK(emitter.try_emit(input())==EmitResult::DroppedQueueFull);
    CHECK(emitter.health().dropped_queue==1 && emitter.snapshot_required());
    for(std::size_t i=0;i<kQueueCapacity;++i) { emitter.service(sink); CHECK(sequence(sink)==i+1); }
    CHECK(emitter.try_emit(input())==EmitResult::Enqueued); emitter.service(sink);
    CHECK(sequence(sink)==kQueueCapacity+2); // Only the valid full-queue attempt leaves a hole.
}
void contiguous_stream(const char* output) { // C07/C08: Host checks this actual C++ wire stream too.
    auto* file=output ? std::fopen(output,"wb") : nullptr; CHECK(!output || file);
    TelemetryEmitter emitter(400,PrivacyLevel::Basic); TestTelemetrySink sink;
    const auto send=[&](BehaviorEvent e,std::uint64_t expected) {
        e.envelope.device_monotonic_us=expected*10;
        CHECK(emitter.try_emit(e)==EmitResult::Enqueued); emitter.service(sink);
        CHECK(sequence(sink)==expected);
        if(file) CHECK(std::fwrite(sink.last_frame,1,sink.last_size,file)==sink.last_size);
    };
    send(state(),1); send(input(),2);
    auto invalid=candidate(); invalid.envelope.action_id=0;
    CHECK(emitter.try_emit(invalid)==EmitResult::DroppedInvalid);
    invalid=input(); invalid.payload.input_received.physical_key=255;
    CHECK(emitter.try_emit(invalid)==EmitResult::DroppedInvalid);
    BehaviorEvent detail{}; detail.type=EventType::CandidateItemDetail; detail.detail.text_length=255;
    CHECK(emitter.try_emit(detail)==EmitResult::Suppressed);
    CHECK(emitter.health().dropped_invalid==2 && emitter.health().attempted==4);
    CHECK(!emitter.snapshot_required());
    send(input(),3); send(emitter.health_event(40),4);
    CHECK(emitter.health().attempted==6 && emitter.health().enqueued==4);
    if(file) CHECK(std::fclose(file)==0);
}
void golden_r2_unchanged() { // C05: frozen Phase 1.1 H01 first two frames.
    const std::uint8_t golden0[]={0xc1,0x44,0x54,0x1,0x21,0x1,0x0,0x11,0x0,0x65,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x1,0x1,0x1,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x5b,0x4a,0xc0};
    const std::uint8_t golden1[]={0xc1,0x44,0x54,0x1,0x21,0x6,0x0,0x2,0x0,0x65,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x14,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x1,0x0,0x93,0x69,0xc0};
    TelemetryEmitter emitter(101,PrivacyLevel::Basic); TestTelemetrySink sink;
    auto e=state(); e.envelope.device_monotonic_us=10; e.envelope.document_revision=1; e.envelope.layout_revision=1;
    CHECK(emitter.try_emit(e)==EmitResult::Enqueued); emitter.service(sink);
    CHECK(sink.last_size==sizeof(golden0) && std::memcmp(sink.last_frame,golden0,sizeof(golden0))==0);
    e=input(); e.envelope.device_monotonic_us=20; e.envelope.document_revision=1; e.envelope.layout_revision=1;
    CHECK(emitter.try_emit(e)==EmitResult::Enqueued); emitter.service(sink);
    CHECK(sink.last_size==sizeof(golden1) && std::memcmp(sink.last_frame,golden1,sizeof(golden1))==0);
}
int main(int argc,char** argv) {
    CHECK(argc<=2);
    missing_identities(); legitimate_zero_and_conditions(); full_queue_classification();
    golden_r2_unchanged(); contiguous_stream(argc==2 ? argv[1] : nullptr);
    std::puts("PASS C01-C08: required identity, single invalid accounting, unchanged r2 golden, legitimate zeros, privacy/sequence, full-queue classification");
}
