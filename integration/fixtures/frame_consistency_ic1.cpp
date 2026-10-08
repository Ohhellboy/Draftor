// The firmware's Map/Tracker/dispatchReport -> bounded queue -> InputRoute ->
// DeviceIme path. render() models LCD completion; no BLE radio/LCD timing claim.
#include "device_ime.hpp"
#include "chinese_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include "input_route.hpp"
#ifndef HANIME_BASELINE_PROBE
#include "ui_trace.hpp"
#endif
#include <filesystem>
#include <deque>
#include <vector>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unistd.h>
using namespace ime_device;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
#ifdef HANIME_BASELINE_PROBE
// Frozen callback stamping, single-threaded Host probe. No claim to run its BLE
// synchronization; Event layout, Map, Tracker, route and DeviceIme are frozen.
namespace s3hid {
struct IngressContext {uint32_t route=1,frame=0,attempt=0;};
template<class Emit> void dispatchReport(Tracker& t,unsigned slot,const State& state,uint32_t epoch,uint32_t ms,IngressContext c,Emit emit){
 t.update(slot,state,epoch,ms,[&](Event e){e.route_epoch=c.route;e.ime_frame=c.frame;e.ime_attempt=c.attempt;emit(e);});
}
}
#endif
std::vector<UiTraceRecord>* observedTrace=nullptr;
void traceDrain(){
#ifndef HANIME_BASELINE_PROBE
 UiTraceRecord r;char line[256];while(uiTrace.pop(r)){if(observedTrace)observedTrace->push_back(r);for(unsigned p=0;p<UiTraceParts;++p){CHECK(formatUiTrace(r,p,line,sizeof(line)));std::cout<<line;}}
#endif
}
struct Scratch {
 fs::path path;
 Scratch(){char name[]="frame-doc-XXXXXX";const char* r=mkdtemp(name);CHECK(r);path=fs::absolute(r);}
 ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
struct Rig {
 Scratch scratch;s3fs::Fs files{scratch.path.string()};hanime::documents::ArduinoSdStoragePort storage{files};
 ChineseFont font;s3app::App app{storage,{},7000,font};Resources resources{RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT};DeviceIme ime{app,resources,font};
 s3hid::Map map;s3hid::Tracker tracker;s3hid::InputRoute route;s3hid::IngressContext ingress;
 std::deque<s3hid::Event> queue;uint32_t now=100,lastBodyAction=0;unsigned emitted=0;
 Rig(){
  // Standard boot keyboard report descriptor, decoded by the same parser as BLE.
  const uint8_t descriptor[]={0x05,1,0x09,6,0xa1,1,0x05,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,6,0x75,8,0x15,0,0x25,0x65,0x05,7,0x19,0,0x29,0x65,0x81,0,0xc0};
  CHECK(map.parse(descriptor,sizeof(descriptor)));CHECK(font.begin());storage.noteMounted();CHECK(app.startup(now));render();
 }
 void report(uint8_t key,uint8_t mods=0){
  ingress.ime_context=ime.ingressContext();ingress.resource_owner=ime.resourceOwner();
  uint8_t bytes[8]={mods,0,key,0,0,0,0,0};s3hid::State state;CHECK(map.report(0)&&s3hid::Map::decode(*map.report(0),bytes,sizeof(bytes),state));
  s3hid::dispatchReport(tracker,0,state,1,++now,ingress,[&](s3hid::Event e){CHECK(queue.size()<512);queue.push_back(e);++emitted;});
 }
 void deliver(){while(!queue.empty()){auto e=queue.front();queue.pop_front();if(route.accept(e,++now)){if(!ime.event(e))app.key(e);}}traceDrain();}
 void tap(uint8_t key,uint8_t mods=0){report(key,mods);report(0);deliver();}
 void render(){
  route.framePresented(app.focusToken());ingress.route=route.epoch();ingress.frame=ime.framePresented();ingress.attempt=ime.attemptSerial();
#ifndef HANIME_BASELINE_PROBE
  const auto* v=ime.view();const VisibleItem* item=nullptr;if(v&&v->count){const auto& row=v->rows[v->row-v->rows[0].index];if(v->col<row.count)item=&row.items[v->col];}
  traceUi(UiTraceKind::Lcd,ime.composition(),ime.window(),nullptr,item);
  if(ime.acceptedAction()!=lastBodyAction&&ime.acceptedDocumentCurrent()){
   lastBodyAction=ime.acceptedAction();
   traceUi(UiTraceKind::BodyLcd,ime.composition(),ime.window(),nullptr,nullptr,0,lastBodyAction,clockUs());
  }
#endif
  std::cout<<"HOST_LCD frame="<<ingress.frame<<" raw="<<ime.raw()<<" selected="<<ime.selected()<<'\n';traceDrain();
 }
 void step(){ime.service();app.service(++now,false);CHECK(resources.state()!=ResourceState::Error);traceDrain();}
 void settle(){unsigned n=0;while(n++<2000000){step();if(resources.ready()&&ime.composition()&&!ime.hasWork())break;}CHECK(n<2000000);render();}
 void cn(){tap(0x39);settle();CHECK(ime.chinese());}
 void text(std::string_view raw,bool slow=false){for(char c:raw){tap(uint8_t(c-'a'+4));if(slow)settle();else step();}}
 std::string first()const{const auto* v=ime.view();CHECK(v&&v->count&&v->rows[0].count);return {v->rows[0].items[0].text.data(),v->rows[0].items[0].bytes};}
};
void waitingAndSpace(){
 Rig r;r.cn();r.text("wjdjle");CHECK(r.ime.window()->busy());
 r.tap(0x2c);r.report(0x2c);r.deliver();r.settle();CHECK(r.app.document.text()=="完蛋了"&&r.ime.raw().empty());
 const auto held=r.emitted;r.report(0x2c);r.deliver();r.report(0x2c);r.deliver();CHECK(r.emitted==held);r.report(0);r.deliver();
 CHECK(r.app.document.text()=="完蛋了");
 std::cout<<"PASS two real early Spaces act on matched successive segments; held report does not repeat\n";
}
void slowAndStale(){
 Rig r;r.cn();r.text("wj",true);CHECK(r.first()=="玩");const auto frame=r.ingress.frame;
 const s3hid::Event stale{1,r.now,0x2c,0,true,r.ingress.route,frame,0,0,0,r.ime.ingressContext(),r.ime.resourceOwner()};
 r.text("djle",true);r.ime.event(stale);r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected().empty());
 auto release=stale;release.down=false;release.ms=++r.now;r.ime.event(release);
 r.tap(0x2c);r.settle();CHECK(r.ime.selected()=="完蛋"&&r.first()=="了");
 std::cout<<"PASS physical event predating edit rejected; subsequent new current selection succeeds\n";
}
void displayBoundary(){
 Rig r;r.cn();r.text("wj");r.settle();const auto old=r.ingress.frame;CHECK(r.first()=="玩");
 r.report(0x2c);r.report(0); // queued with old LCD frame; do not rewrite on dequeue
 r.ime.window()->request(0,1,false);CHECK(r.ime.view()->generation!=old);r.render();CHECK(r.ingress.frame!=old);
 r.deliver();r.settle();CHECK(r.app.document.text()=="玩"&&r.ime.raw().empty());
 std::cout<<"PASS queued Space at LCD boundary selects original complete candidate, not new highlight\n";
 Rig page;page.cn();page.text("wj");page.settle();page.ime.window()->request(5,0,true);CHECK(page.ime.window()->busy());
 page.tap(0x2c);page.settle();CHECK(page.app.document.text()=="玩"&&page.ime.raw().empty());
 std::cout<<"PASS old visible choice during page scan reauthorized by exact key/ref/range\n";
}
void immutableReport(){
 s3hid::Tracker tracker;s3hid::State state;state.set(4);state.set(5);s3hid::IngressContext live{7,11,13};unsigned count=0;
 s3hid::dispatchReport(tracker,0,state,2,123,live,[&](s3hid::Event e){CHECK(e.route_epoch==7&&e.ime_frame==11&&e.ime_attempt==13);live={8,12,14};++count;});CHECK(count==2);
 s3hid::InputRoute route;s3hid::Event old{2,123,4,0,true,route.epoch(),11,13};route.framePresented(9);CHECK(!route.accept(old,125)&&route.discarded==1);
#ifndef HANIME_BASELINE_PROBE
 std::array<UiTraceRecord,UiTrace::Capacity> storage;UiTrace bounded(storage.data());UiTraceRecord r;for(unsigned i=0;i<40;++i)bounded.push(r);CHECK(bounded.lost()==8);unsigned rows=0;while(bounded.pop(r))++rows;CHECK(rows==32);
 r={};r.us=r.input=r.revision=r.scheme=r.mount=r.bundle=r.item_input=r.item_revision=r.item_scheme=r.item_mount=r.item_bundle=r.page_generation=UINT64_MAX;
 r.sequence=r.event_ms=r.epoch=r.route=r.event_frame=r.screen_frame=r.view_frame=r.attempt=UINT32_MAX;r.begin=r.end=r.page=r.slot=r.raw=r.focus=UINT16_MAX;r.usage=r.detail=255;
 char line[384];for(unsigned p=0;p<UiTraceParts;++p)CHECK(formatUiTrace(r,p,line,sizeof(line))>0);
 std::cout<<"TRACE_MEMORY record="<<sizeof(UiTraceRecord)<<" ring="<<sizeof(UiTrace)<<" capacity=32 bytes; no dynamic allocation\n";
#endif
 std::cout<<"PASS one immutable ingress context per report, original route/attempt retained, bounded diagnostic overflow explicit\n";
}
void actionTiming(){
 std::vector<std::string> reference;
 uint64_t referenceReads=0,referenceBytes=0;
 for(bool detailed:{false,true}){
  uiTrace.setDetailed(detailed);
  Rig r;r.cn();const auto before=metrics.io();r.text("ui");r.settle();
  std::vector<UiTraceRecord> events;observedTrace=&events;
  std::vector<std::string> visible;
  auto verifyAction=[&](uint8_t usage,size_t target,size_t first,bool cacheOnly){
   events.clear();const auto io=metrics.io();r.tap(usage);r.settle();
   const auto down=std::find_if(events.begin(),events.end(),[&](const UiTraceRecord& e){return e.kind==UiTraceKind::KeyDown&&e.usage==usage;});
   CHECK(down!=events.end()&&down->action);
   const uint32_t action=down->action;
   uint64_t requested=0,ready=0,lcd=0;bool range=false;
   for(const auto& e:events){
    if(e.action!=action)continue;
    if(e.kind==UiTraceKind::RowRequest){CHECK(e.target_row==target);requested=e.us;}
    if(e.kind==UiTraceKind::RangeReady)range=true;
    if(e.kind==UiTraceKind::WindowReady){CHECK(e.first_row==first&&e.row_count==3);ready=e.us;}
    if(e.kind==UiTraceKind::Lcd){CHECK(e.first_row==first&&e.row_count==3);lcd=e.us;}
   }
   CHECK(requested&&ready>=requested&&lcd>=ready);
   if(cacheOnly)CHECK((metrics.io()-io).reads==0&&!range);
   else CHECK(range);
   const auto* v=r.ime.view();CHECK(v&&v->count==3);
   for(size_t row=0;row<3;++row){CHECK(v->rows[row].index==first+row&&v->rows[row].count);for(size_t col=0;col<v->rows[row].count;++col)visible.emplace_back(v->rows[row].items[col].text.data(),v->rows[row].items[col].bytes);}
   std::cout<<"ACTION_CHECK detailed="<<detailed<<" action="<<action<<" target="<<target<<" first="<<first<<" rows=3 cache_only="<<cacheOnly<<" ordered=1 lcd_scope=Host_model\n";
  };
  verifyAction(0x51,1,0,false); // Confirmed initial Down, three actual rows.
  verifyAction(0x51,2,1,false); // New bottom row; no blank/partial publication.
  verifyAction(0x52,1,0,true);  // All three rows retained, zero new physical read.
  r.ime.interrupt(); /* fixture warmup reset */r.text("ui");r.settle();events.clear();r.tap(0x2c);r.settle();
  CHECK(r.app.document.text()=="是");
  const auto action=r.ime.acceptedAction();uint64_t queued=0,accepted=0,lcd=0;
  for(const auto& e:events)if(e.action==action){
   if(e.kind==UiTraceKind::CommitQueued)queued=e.us;
   if(e.kind==UiTraceKind::BodyAccepted)accepted=e.us;
   if(e.kind==UiTraceKind::BodyLcd)lcd=e.us;
  }
  CHECK(action&&queued&&accepted>=queued&&lcd>=accepted&&r.ime.acceptedUs()==accepted);
  const auto cost=metrics.io()-before;
  if(!detailed){reference=visible;referenceReads=cost.reads;referenceBytes=cost.bytes;}
  else CHECK(visible==reference&&cost.reads==referenceReads&&cost.bytes==referenceBytes);
  observedTrace=nullptr;
 }
 // A body edit before rendering preserves Accepted but supersedes that frame.
 // It must not be measured as if the accepted text reached the LCD.
 Rig superseded;superseded.cn();superseded.text("ui");superseded.settle();
 superseded.tap(0x2c);
 unsigned steps=0;while(superseded.ime.hasWork()&&steps++<2000000)superseded.step();
 CHECK(steps<2000000&&superseded.app.document.text()=="是"&&superseded.ime.acceptedDocumentCurrent());
 superseded.tap(0x2a);CHECK(superseded.app.document.text().empty()&&!superseded.ime.acceptedDocumentCurrent());
 CHECK(superseded.ime.receipt().status==hanime::CompositionStatus::Accepted);
 uiTrace.setDetailed(false);
 std::cout<<"PASS direction/action -> first usable range -> complete visible rows -> Host LCD, zero-read cached return; explicit Space -> queued -> actual App acceptance -> Host LCD; normal/detail same identities/text/physical read counts\n";
}
int main(){try{std::cout<<"Host report/queue/route/LCD-completion-model only; timings synthetic; fixed API0100/Host06\n";waitingAndSpace();slowAndStale();displayBoundary();immutableReport();actionTiming();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS frame_consistency all four scenarios\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
