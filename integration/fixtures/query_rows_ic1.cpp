// Real production resources + DeviceIme + font pixel widths. Host elapsed times
// are not device latency. Off/on allocate exactly the same bounded window.
#include "device_ime.hpp"
#include "chinese_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace ime_device;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
struct Rig {
 fs::path path; s3fs::Fs files;hanime::documents::ArduinoSdStoragePort storage;ChineseFont font;s3app::App app;Resources resources;DeviceIme ime;uint32_t now=0,frame=0;
 static fs::path temp(){char n[]="query-rows-XXXXXX";CHECK(mkdtemp(n));return fs::absolute(n);}
 Rig(hanime_extension::StorageBackend backend={}):path(temp()),files(path.string()),storage(files),app(storage,{},7511,font),resources(RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT),ime(app,resources,font,nullptr,backend){CHECK(font.begin());storage.noteMounted();CHECK(app.startup(++now));}
 ~Rig(){std::error_code ec;fs::remove_all(path,ec);}
 void tap(uint8_t key,uint8_t mods=0){for(bool down:{true,false}){s3hid::Event e{1,++now,key,mods,down,0,frame,ime.attemptSerial()};e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();if(!ime.event(e))app.key(e);}}
 void type(std::string_view s){for(char ch:s)tap(uint8_t(ch-'a'+4));}
 void step(bool bg=false){ime.service(bg);app.service(++now,false);CHECK(resources.state()!=ResourceState::Error);}
 void settle(){unsigned n=0;do{step();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());frame=ime.framePresented();}
 void begin(bool py,const char* raw,bool bg){tap(0x39);settle();if(py){tap(0x2c,1);settle();}ime.window()->setPrefetch(bg);type(raw);settle();CHECK(resources.optimized());CHECK(ime.composition()->memory_usage().rank.ordered_cursor_bytes>0);}
 void background(){for(unsigned n=0;n<2000000;++n){step(true);if(!ime.window()->prefetchPending())return;}CHECK(false);}
};
uint64_t digest(const CandidateView& v){
 uint64_t h=1469598103934665603ULL;auto mix=[&](uint64_t n){for(unsigned i=0;i<8;++i){h^=(n>>(i*8))&255;h*=1099511628211ULL;}};
 mix(v.count);mix(v.row);mix(v.col);mix(v.at_end);
 for(size_t i=0;i<v.count;++i){const auto& r=v.rows[i];CHECK(r.count<=6&&r.width<=284);mix(r.index);mix(r.count);mix(r.width);
  for(size_t j=0;j<r.count;++j){const auto& x=r.items[j];for(auto b:x.key.bytes)mix(b);mix(x.ref);mix(x.begin);mix(x.end);mix(x.page);mix(x.slot);for(size_t k=0;k<x.bytes;++k)mix(uint8_t(x.text[k]));}
 }return h;
}
void cost(const char* label,bool bg,const IoCounters& before,uint64_t start){
 const auto d=metrics.io()-before;std::cout<<"ROW_COST label="<<label<<" prefetch="<<bg<<" bytes="<<d.bytes<<" reads="<<d.reads<<" seeks="<<d.seeks<<" logical="<<d.requested_bytes<<" host_us="<<clockUs()-start<<'\n';
}
uint64_t walk(bool py,bool bg){
 Rig r;const auto io=metrics.io();const auto t=clockUs();r.begin(py,py?"yi":"ui",bg);cost(py?"py_first":"xh_first",bg,io,t);
 auto h=digest(*r.ime.view());if(bg){r.background();CHECK(r.ime.window()->costs().prefetch_starts==0);}
 for(size_t row:{1u,2u,3u,4u,3u,2u,7u,8u,7u,0u,24u,25u,24u,0u}){
  const auto beforeBg=metrics.io();const auto tb=clockUs();if(bg)r.background();cost("background",bg,beforeBg,tb);
  const auto before=metrics.io();const auto start=clockUs();r.ime.window()->request(row,0,true);r.settle();if(r.ime.window()->state()!=WindowState::Ready)std::cout<<"ROW_FAIL requested="<<row<<" state="<<int(r.ime.window()->state())<<" page_size="<<r.ime.composition()->page().size<<" notice="<<r.ime.notice()<<'\n';CHECK(r.ime.window()->state()==WindowState::Ready);h^=digest(*r.ime.view());h*=1099511628211ULL;
  cost("navigation",bg,before,start);std::cout<<"ROW_STATE py="<<py<<" row="<<row<<" first="<<r.ime.view()->rows[0].index<<" count="<<r.ime.view()->count<<" hash="<<digest(*r.ime.view())<<'\n';
  CHECK(r.ime.window()->preparedRows()<=1);
 }
 const auto& c=r.ime.window()->costs();CHECK(c.reused_rows>0);if(bg)CHECK(c.prefetch_hits>0);
 std::cout<<"ROW_TOTAL py="<<py<<" prefetch="<<bg<<" hash="<<h<<" reuse="<<c.reused_rows<<" built="<<c.built_rows<<" hits="<<c.prefetch_hits<<" misses="<<c.prefetch_misses<<" starts="<<c.prefetch_starts<<" range_moves="<<c.range_moves<<" anchors_fallback="<<c.anchor_fallbacks<<" window_bytes="<<sizeof(CandidateWindow)<<'\n';
 return h;
}
uint64_t choose(bool background,unsigned partial){
 Rig r;r.begin(false,"ui",background);r.ime.window()->request(1,0,true);r.settle();const auto* v=r.ime.view();const auto item=v->rows[v->row-v->rows[0].index].items[0];
 if(background){if(partial){
  const auto ranges=r.ime.window()->costs().range_moves;r.step(true);CHECK(r.ime.window()->prefetchPending());
  if(partial==2){unsigned n=0;while(r.ime.window()->prefetchPending()&&r.ime.window()->costs().range_moves==ranges&&n++<100000)r.step(true);CHECK(r.ime.window()->prefetchPending()&&r.ime.window()->costs().range_moves>ranges);}
 }else r.background();}
 // Stage through the public window API to inspect identity before submission.
 // User digit/Space final selection is covered by space_commit.
 const auto before=metrics.io();const auto start=clockUs();r.ime.window()->choose(0,r.frame);r.settle();cost(partial==2?"choice_pending_range":partial?"choice_inflight":"choice_ready",background,before,start);
 const auto s=r.ime.composition()->snapshot();CHECK(s.member_count==1&&s.members[0].key==item.key&&s.members[0].ref==item.ref&&r.app.document.text().empty());
 CHECK(r.ime.selected()==std::string_view(item.text.data(),item.bytes));
 r.tap(0x28);r.settle();CHECK(r.app.document.text()==std::string_view(item.text.data(),item.bytes));
 return (metrics.io()-before).bytes;
}
void cancellation(){
 Rig r;r.begin(false,"ui",true);r.ime.window()->request(1,0,true);r.settle();r.step(true);CHECK(r.ime.window()->prefetchPending());const auto old=r.ime.composition()->operation();
 r.tap(0x29);CHECK(r.ime.active()&&r.ime.rawEditing()&&r.ime.raw()=="ui");r.ime.interrupt(); /* separate explicit reset */r.type("wj");const auto newer=r.ime.composition()->operation();
 CHECK(r.ime.composition()->cancel(old)==hanime_extension::Status::Stale);(void)newer;r.settle();CHECK(r.ime.raw()=="wj"&&r.ime.window()->state()==WindowState::Ready);
 r.step(true);r.tap(0x2c,1);r.settle();CHECK(r.ime.raw().empty()&&r.resources.rules()==hanime_extension::SchemeRules::FullPinyin&&metrics.resource_handles==5);
 std::cout<<"PASS background original cancellation, old ticket cannot cancel new raw, switch discards and closes old order before new\n";
}
void allRows(bool py){
 Rig r;r.begin(py,py?"yi":"ui",false);size_t total=0,rows=0;
 for(size_t row=0;row<200;++row){
  r.ime.window()->request(row,0,true);r.settle();CHECK(r.ime.window()->state()==WindowState::Ready);
  const auto& v=*r.ime.view();CHECK(v.count<=3&&v.row==row);const auto& selected=v.rows[row-v.rows[0].index];total+=selected.count;++rows;
  if(v.at_end&&row==v.rows[0].index+v.count-1)break;
 }
 CHECK(total==(py?434u:222u));r.ime.window()->request(rows,0,true);CHECK(r.ime.window()->state()==WindowState::Boundary);
 // Beyond the 32-anchor ring, returning to zero must remain correct/bounded.
 r.ime.window()->request(0,0,true);r.settle();CHECK(r.ime.window()->state()==WindowState::Ready&&r.ime.view()->rows[0].index==0);
 std::cout<<"PASS all_pixel_rows py="<<py<<" candidates="<<total<<" rows="<<rows<<" boundary_and_reverse_after_anchor_eviction=1\n";
}
void missingTopRow(bool py){
 Rig r;r.begin(py,py?"yi":"ui",false);
 CandidateView earlier{};
 for(unsigned target=1;target<=4;++target){
  r.tap(0x51);r.settle();CHECK(r.ime.view()->row==target);
  if(target==2)earlier=*r.ime.view();
 }
 r.tap(0x52);r.settle();CHECK(r.ime.view()->rows[0].index==2);
 const auto io=metrics.io();const auto costs=r.ime.window()->costs();
 r.tap(0x52);r.settle();
 const auto delta=metrics.io()-io;const auto after=r.ime.window()->costs();
 std::cout<<"MISSING_TOP py="<<py<<" first="<<r.ime.view()->rows[0].index
  <<" reused="<<after.reused_rows-costs.reused_rows<<" built="<<after.built_rows-costs.built_rows
  <<" bytes="<<delta.bytes<<" reads="<<delta.reads<<" seeks="<<delta.seeks<<'\n';
 // The expected content was independently captured on the forward traversal.
 // A missing top row must not make the two resident following rows new work.
 CHECK(digest(*r.ime.view())==digest(earlier));
 CHECK(after.reused_rows-costs.reused_rows==2);
 CHECK(after.built_rows-costs.built_rows==1);
 CHECK(delta.bytes<70000); // One missing row, not the former three-row 148480 B.
 // Continue forwards after the skipped cached rows: no stale overflow item may
 // be attached to another logical row when the text stream resumes.
 r.tap(0x51);r.settle();r.tap(0x52);r.settle();
 CHECK(digest(*r.ime.view())==digest(earlier));
}
void chooseDuringMissingTop(){
 Rig r;r.begin(false,"ui",false);
 for(unsigned target=1;target<=4;++target){r.tap(0x51);r.settle();}
 r.tap(0x52);r.settle();
 const auto view=*r.ime.view();
 const auto expected=view.rows[view.row-view.rows[0].index].items[0];
 r.tap(0x52);CHECK(r.ime.window()->busy());
 r.ime.window()->choose(0,r.frame);r.settle(); // Choose the still displayed row, not the requested one.
 const auto s=r.ime.composition()->snapshot();
 CHECK(s.member_count==1&&s.members[0].key==expected.key&&s.members[0].ref==expected.ref);
 CHECK(r.app.document.text().empty());
 r.tap(0x28);r.settle();
 CHECK(r.app.document.text()==std::string_view(expected.text.data(),expected.bytes));
 std::cout<<"PASS missing-top in-flight old-frame choice reauthorizes full identity once\n";
}
void prefixContext(){
 Rig r;r.begin(true,"xian",false);
 r.ime.window()->request(1,0,true);r.settle();
 CHECK(r.ime.view()->count==3&&r.ime.view()->rows[1].items[0].end==4);
 // A public shorter-prefix query does not edit the raw input or advance its
 // revision. Cached display rows still need the current query range as well.
 const auto revision=r.ime.composition()->snapshot().revision;
 r.ime.window()->query(2);r.settle();
 CHECK(r.ime.composition()->snapshot().revision==revision);
 r.ime.window()->request(1,0,true);r.settle();
 for(size_t i=0;i<r.ime.view()->count;++i)
  for(size_t j=0;j<r.ime.view()->rows[i].count;++j){
   const auto& item=r.ime.view()->rows[i].items[j];
   CHECK(item.begin==0&&item.end==2);
  }
 std::cout<<"PASS same-raw shorter-prefix query does not reuse old range rows\n";
}
void apiBoundary(){
 Resources r(RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT);r.start(hanime_extension::SchemeRules::R1Xiaohe);for(unsigned i=0;!r.ready()&&i<256;++i)r.service();CHECK(r.ready());
 hanime_extension::Composition c(r.scheme,r.shared,r.rules());CHECK(c.edit_raw(0,0,"ui")==hanime_extension::Status::Ready);CHECK(c.query_longest()==hanime_extension::Status::Working);
 auto op=c.operation();for(unsigned i=0;c.status()==hanime_extension::Status::Working&&i<100000;++i)c.tick(op,Resources::Budget);CHECK(c.status()==hanime_extension::Status::PageReady);
 auto selected=c.selection(0);CHECK(c.request_page(hanime::RangeDirection::After)==hanime_extension::Status::Working);auto prefetch=c.operation();
 CHECK(c.cancel(prefetch)==hanime_extension::Status::Cancelled);CHECK(!c.page().size);CHECK(c.select(selected)==hanime_extension::Status::Stale);CHECK(c.request_page(hanime::RangeDirection::Before)==hanime_extension::Status::Stale);
 std::cout<<"API_GAP cancel_pending_range clears current page and rank lineage; old Selection and Before return Stale. Needs extension-owned staging/cancel contract for zero-wait foreground interruption.\n";
}
void foregroundCosts(){
 Rig r;r.tap(0x39);r.settle();
 auto before=metrics.io();auto start=clockUs();r.type("ui");r.settle();cost("ui_ready_first",false,before,start);
 unsigned action=0;
 for(uint8_t key:{0x51,0x51,0x51,0x51,0x52,0x52}){
  before=metrics.io();start=clockUs();const auto old=r.ime.window()->costs();r.tap(key);r.settle();cost(key==0x51?"down":"up",false,before,start);
  const auto now=r.ime.window()->costs();
  std::cout<<"FOREGROUND action="<<++action<<" row="<<r.ime.view()->row<<" first="<<r.ime.view()->rows[0].index<<" reused="<<now.reused_rows-old.reused_rows<<" built="<<now.built_rows-old.built_rows<<" digest="<<digest(*r.ime.view())<<'\n';
 }
}
int main(int argc,char** argv){try{
 if(argc==2&&std::string_view(argv[1])=="--foreground-costs"){foregroundCosts();return 0;}
 if(argc==2&&std::string_view(argv[1])=="--missing-top"){
  missingTopRow(false);missingTopRow(true);chooseDuringMissingTop();prefixContext();return 0;
 }
 missingTopRow(false);missingTopRow(true);chooseDuringMissingTop();prefixContext();
 for(bool py:{false,true}){const auto off=walk(py,false),on=walk(py,true);CHECK(off==on);}
 const auto off=choose(false,false),ready=choose(true,false),inflight=choose(true,1),pending=choose(true,2);
 std::cout<<"PREFETCH_CHOICE off_bytes="<<off<<" ready_bytes="<<ready<<" inflight_bytes="<<inflight<<" pending_range_bytes="<<pending<<" default=OFF device_latency=OPEN\n";
 cancellation();allRows(false);allRows(true);apiBoundary();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS actual pixel rows off/on identity equivalence, bounded storage, original visible selection, separate commit, cancellation and public API boundary\n";
return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
