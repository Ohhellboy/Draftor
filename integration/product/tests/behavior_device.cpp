#include "behavior_observer.hpp"
#if DRAFTOR_TELEMETRY_PROFILE
#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include <draftor/telemetry/SerialTelemetrySink.h>
namespace bt=ime_device::behavior;
namespace dt=draftor::telemetry;
struct Wire final:dt::TelemetrySink {
 std::ofstream file;
 explicit Wire(const char* name):file(name,std::ios::binary){}
 dt::SinkResult try_write_frame(const std::uint8_t* bytes,std::size_t n) noexcept override {file.write(reinterpret_cast<const char*>(bytes),n);return dt::SinkResult::Accepted;}
 dt::SinkPollResult poll() noexcept override{return dt::SinkPollResult::NoLoss;}
};
struct ObservedRig:SwitchRig {
 Wire wire;
 // Host-owned allocation is test scaffolding; S3 uses one PSRAM allocation.
 std::vector<std::max_align_t> observer;
 ObservedRig(const char* path):wire(path),observer((bt::storageBytes()+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t)) {bt::bind(observer.data(),0x53594e5448455449ULL,app,&ime);drain();}
 void drain(){for(unsigned i=0;i<40;++i)bt::service(wire,false);}
 void step(){SwitchRig::step();bt::service(wire,false);}
 void settle(){unsigned n=0;do{step();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());paint();}
 void paint(){bt::displayBegin(app,&ime,ime.candidateAvailable());SwitchRig::draw();bt::displayEnd(true);drain();}
 void tap(std::uint8_t key,std::uint8_t mods=0){SwitchRig::tap(key,mods);drain();}
 void type(std::string_view raw){for(char c:raw)tap(std::uint8_t(c-'a'+4));}
};
int main(int argc,char** argv){try {
 CHECK(argc==2||argc==3);ObservedRig r(argv[1]);
 r.tap(4);r.tap(42); // Actual English document insert/backspace.
 r.tap(57);r.settle();
 r.type("ui");r.settle();CHECK(r.ime.view()->rows[0].count>=2);
 const auto expected=r.ime.view()->rows[0].items[1];
 r.tap(79);r.settle();CHECK(r.ime.view()->row==0&&r.ime.view()->col==1);
 r.tap(31);r.settle();CHECK(r.ime.raw().empty());CHECK(r.app.document.text()==std::string_view(expected.text.data(),expected.bytes));
 r.tap(42);r.settle();
 r.type("wj");r.settle();
 const auto input=r.ime.composition()->snapshot().input_id;
 const auto revision=r.ime.composition()->snapshot().revision;
 r.tap(61);r.settle();CHECK(!r.app.horizontalActive());
 CHECK(r.ime.composition()->snapshot().input_id==input&&r.ime.composition()->snapshot().revision==revision);
 r.tap(61);r.settle();CHECK(r.app.horizontalActive());
 r.tap(61);r.settle();r.tap(81);r.settle();r.tap(44);r.settle();CHECK(r.ime.raw().empty());
 r.tap(42);r.settle();r.tap(82);r.settle();r.tap(81);r.settle();
 r.type("ui");r.settle();r.tap(42);r.settle();r.tap(42);r.settle();CHECK(r.ime.raw().empty());
 r.tap(57);r.tap(5);r.tap(42);r.drain();
 const bool overload=argc==3;
 if(overload){
  const auto sent=bt::health()->frames_accepted;
  r.SwitchRig::type("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
  bt::service(r.wire,true);CHECK(bt::health()->frames_accepted==sent);
  CHECK(bt::health()->dropped_queue>0);r.drain();r.paint();
 }
 const auto old_session=r.app.session.sessionSerial();
 r.tap(17,1);r.settle(); // Existing Ctrl+N; asynchronous file install must retain one ID.
 for(unsigned i=0;r.app.commandBarrier()||r.app.persistence.backgroundSaveActive();++i){CHECK(i<100000);r.step();r.drain();}
 std::cerr<<"FILE_FIXTURE old="<<old_session<<" new="<<r.app.session.sessionSerial()<<" bytes="<<r.app.document.text().size()<<" notice="<<r.app.notice<<" status="<<unsigned(r.app.lastOperation.status)<<" stage="<<r.app.lastOperation.stage<<" barrier="<<r.app.commandBarrier()<<"\n";
 CHECK(r.app.session.sessionSerial()!=old_session&&r.app.document.text().empty());r.drain();
 const auto* h=bt::health();CHECK(h&&h->dropped_invalid==0&&h->dropped_encode==0&&(overload||h->dropped_queue==0));
 std::cout<<"HOST_PRODUCT_PIPELINE profile="<<DRAFTOR_TELEMETRY_PROFILE<<" events="<<h->enqueued<<" high_water="<<h->queue_high_water<<" observer_bytes="<<bt::storageBytes()<<" product_semantics_checked=1 REAL_DEVICE=0\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#else
int main(){return 0;}
#endif
