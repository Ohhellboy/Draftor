#define main packaged_query_rows_main
#include "query_rows.cpp"
#undef main
#include "ui_trace.hpp"
#include <fstream>
void queryGaps(){
 for(size_t prefix:{size_t(0),size_t(4)}){
  Rig r;r.begin(false,"wojbtm",false);r.tap(0x29);r.type("wojbtm");r.tap(0x2c);r.tap(0x2c);
  unsigned steps=0;while(r.ime.selected().empty()){r.step();CHECK(++steps<2000000);}
  CHECK(r.ime.selected()=="我"&&r.ime.queuedInput()==1);
  const auto stamp=r.ime.composition()->raw_input().stamp;const auto serial=r.ime.window()->querySerial();
  r.ime.window()->query(prefix);CHECK(r.ime.composition()->input_is_current(stamp)&&r.ime.window()->querySerial()!=serial);
  r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected()=="我"&&r.ime.queuedInput()==0);
  CHECK(r.ime.window()->state()==WindowState::Ready&&r.ime.window()->view().count>0); // New query remains alive; old cleanup cannot cancel it.
 }
 {Rig r;r.begin(false,"wojbtm",false);r.tap(0x29);r.type("wojbtm");r.tap(0x2c);r.tap(0x2c);r.settle();CHECK(r.app.document.text()=="我今天");}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");r.tap(0x2c);
  while(r.app.document.text().empty()){r.step();}
  CHECK(r.ime.queuedInput()>0);
  r.ime.window()->query();r.settle();CHECK(r.app.document.text()=="是"&&r.ime.queuedInput()==0);}
 std::cout<<"PASS R1 normal suffix, different/same-range query gap, post-Accepted external query\n";
}
void navigationCancel(uint8_t stop){
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();UiTraceRecord rec;while(uiTrace.pop(rec)){}uiTrace.setMode(1);
 const auto lost=uiTrace.lost();std::vector<UiTraceRecord> records;std::ofstream out("navigation_stop_"+std::to_string(stop)+".log");
 out<<"[BOOT] HOST_R2\n[TRACE_SCHEMA] version=2 mode=1\n";
 auto drain=[&](){while(uiTrace.pop(rec)){records.push_back(rec);for(unsigned i=0;i<UiTraceParts;++i){char line[384];auto n=formatUiTrace(rec,i,line,sizeof(line));CHECK(n);out.write(line,n);}}};
 auto key=[&](uint8_t k){r.tap(k);drain();};
 for(unsigned i=0;i<6;++i)key(0x51);
 CHECK(r.ime.queuedArrows()==5);
 if(stop==0){r.ime.interrupt();drain();}else if(stop==1){r.ime.window()->query();r.step();drain();}else key(stop);
 CHECK(r.ime.queuedArrows()==0&&r.ime.arrowDiscarded()==5);
 size_t cancelled=0,queued=0;
 for(const auto& a:records)if(a.kind==UiTraceKind::Intent){if(a.detail==QueuedNavigation)++queued;if(a.detail==CancelledByUser||a.detail==InvalidContext)++cancelled;}
 CHECK(queued==6&&cancelled==6&&uiTrace.lost()==lost);
 r.settle();drain();std::cout<<"PASS R2 stop="<<unsigned(stop)<<" queued="<<queued<<" terminated="<<cancelled<<" lost_delta=0\n";
}
void reverseTrace(){
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();UiTraceRecord rec;while(uiTrace.pop(rec)){}uiTrace.setMode(1);
 const auto lost=uiTrace.lost();std::vector<uint32_t> applied,queued;size_t inherited=0;std::ofstream out("navigation_reverse.log");
 out<<"[BOOT] HOST_R2_REVERSE\n[TRACE_SCHEMA] version=2 mode=1\n";
 auto drain=[&](){while(uiTrace.pop(rec)){if(rec.kind==UiTraceKind::NavigationApplied)applied.push_back(rec.action);if(rec.kind==UiTraceKind::Intent&&rec.detail==QueuedNavigation)queued.push_back(rec.action);if(rec.kind==UiTraceKind::Intent&&rec.detail==NavigationInherited){CHECK(rec.parent>rec.action);++inherited;}for(unsigned i=0;i<UiTraceParts;++i){char line[384];const auto bytes=formatUiTrace(rec,i,line,sizeof(line));CHECK(bytes);out.write(line,bytes);}}};
 for(unsigned i=0;i<10;++i){r.tap(i<6?0x51:0x52);drain();}
 CHECK(queued.size()==10);unsigned steps=0;
 while(r.ime.hasWork()){r.step();drain();CHECK(++steps<2000000);}
 CHECK(r.ime.view()->row==3&&r.ime.view()->col==0&&inherited>0&&uiTrace.lost()==lost);
 for(auto action:queued)CHECK(std::find(applied.begin(),applied.end(),action)!=applied.end());
 std::cout<<"PASS R2 reversal: ten semantic applications, inherited target links="<<inherited<<" no simulated LCD/zero trace loss\n";
}
int main(){try{queryGaps();reverseTrace();for(uint8_t k:{uint8_t(0x29),uint8_t(0x2a),uint8_t(0x2c),uint8_t(0),uint8_t(1)})navigationCancel(k);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
