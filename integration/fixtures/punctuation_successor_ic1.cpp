#define main existing_query_rows_main
#include "query_rows.cpp"
#undef main
void finalPunctuation(){
 for(bool py:{false,true})for(uint8_t choice:{uint8_t(44),uint8_t(30)})for(bool early:{false,true}){
  Rig r;r.begin(py,py?"shi":"ui",false);
  if(early){r.ime.interrupt(); /* fixture warmup reset */r.type(py?"shi":"ui");}
  r.tap(choice);r.tap(54);r.type(py?"shi":"ui");r.tap(choice);r.tap(55);r.settle();
  CHECK(r.app.document.text()=="是，是。"&&r.ime.raw().empty()&&r.ime.queuedInput()==0);
 }
 {Rig r;r.begin(false,"ui",false);r.tap(44);r.tap(54);r.tap(54);r.settle();CHECK(r.app.document.text()=="是，，");r.tap(55);CHECK(r.app.document.text()=="是，，。");}
 for(bool py:{false,true}){
  Rig r;r.begin(py,py?"wandanle":"wjdjle",false);
  r.tap(30);r.tap(55);r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected()=="完蛋");
  r.tap(44);r.tap(55);r.settle();CHECK(r.app.document.text()=="完蛋了。");
 }
 std::cout<<"PASS final Space/digit, early/ready both schemes, punctuation-letter order, real double punctuation, partial blocked\n";
}
void boundaries(){
 for(unsigned change=0;change<7;++change){
  Rig r;r.begin(false,"ui",false);r.tap(44);r.tap(54);r.type("ui");
  if(change==0)r.tap(41);
  if(change==1)r.ime.window()->query();
  if(change==2)CHECK(r.app.commitText("body",r.app.editStamp()));
  if(change==3)r.app.disconnected();
  if(change==4)r.ime.interrupt();
  if(change==5)r.tap(44,1);
  if(change==6){while(r.app.document.text().empty())r.step();r.app.key({1,++r.now,80,0,true});}
  r.settle();CHECK(r.ime.queuedInput()==0&&r.app.document.text().find("，")==std::string_view::npos);
 }
 {Rig r;r.begin(false,"ui",false);r.tap(44);for(unsigned i=0;i<33;++i){r.tap(54);}CHECK(r.ime.queuedInput()==32&&r.ime.inputOverflow()==1);r.settle();CHECK(std::string(r.app.document.text())==std::string("是")+[](){std::string s;for(unsigned i=0;i<32;++i)s+="，";return s;}());}
 {Rig r;r.begin(false,"ui",false);r.tap(40);r.tap(54);r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected()=="是");r.tap(40);r.tap(54);r.settle();CHECK(r.app.document.text()=="是，");}
 {Rig r;r.begin(false,"ui",false);r.tap(44);s3hid::Event ev{1,++r.now,54,0,true,0,r.frame,r.ime.attemptSerial()};ev.ime_context=r.ime.ingressContext();ev.resource_owner=r.ime.resourceOwner();r.ime.event(ev);r.ime.event(ev);auto repeat=ev;repeat.ms=++r.now;r.ime.event(repeat);ev.down=false;ev.ms=++r.now;r.ime.event(ev);r.settle();CHECK(r.app.document.text()=="是，");repeat.ms=ev.ms-1;r.ime.event(repeat);r.settle();CHECK(r.app.document.text()=="是，");}
 {Rig r;r.begin(false,"ui",false);r.tap(44);s3hid::Event ev{1,++r.now,54,0,true,0,r.frame,0};ev.ime_context=UINT32_MAX;ev.resource_owner=r.ime.resourceOwner();r.ime.event(ev);ev.down=false;ev.ms=++r.now;r.ime.event(ev);r.settle();CHECK(r.app.document.text()=="是");}
 std::cout<<"PASS Esc/query/target/disconnect/switch/interrupt, post-Accepted target change, capacity32/reject-newest, Enter and stale/repeat isolation\n";
}
#ifndef IME_BASELINE_PROBE
void feedback(){
 for(bool py:{false,true}){
  Rig r;r.begin(py,py?"wojintianwandanle":"wojbtmwjdjle",false);r.tap(80);while(r.ime.rawCursor()>2)r.tap(80);r.tap(81);r.settle();
  for(unsigned i=0;i<3;++i){r.tap(44);r.settle();}
  r.tap(80);while(r.ime.rawCursor()>2)r.tap(80);
  const auto before=r.ime.editTarget();CHECK(before.valid());char caption[64];r.ime.editCaption(caption,sizeof(caption));CHECK(std::string_view(caption).find('[')==std::string_view::npos);
  r.tap(81);r.settle();CHECK(r.ime.reselecting());CHECK(r.ime.editTarget().begin==before.begin&&r.ime.editTarget().end==before.end);
  r.ime.editCaption(caption,sizeof(caption));CHECK(std::string_view(caption).find("RESELECT")!=std::string_view::npos);
  r.tap(41);CHECK(r.ime.rawEditing());const auto after=r.ime.editTarget();CHECK(after.valid()&&after.begin==0&&after.end==2);
  r.tap(81);r.settle();r.tap(41);while(r.ime.rawCursor())r.tap(80);CHECK(!r.ime.editTarget().valid());
  r.ime.editCaption(caption,sizeof(caption));CHECK(std::string_view(caption).find("move L/R")!=std::string_view::npos);r.tap(81);CHECK(r.ime.rawEditing()&&!r.ime.editTarget().valid());
 }
 {Rig r;r.begin(false,"ui",false);r.ime.interrupt(); /* fixture warmup reset */r.type(std::string("ui")+std::string(60,'a'));r.tap(80);while(r.ime.rawCursor()>2)r.tap(80);r.tap(81);r.settle();const auto target=r.ime.editTarget();CHECK(target.begin==0&&target.end==2);const auto viewport=r.ime.editViewport(r.font);CHECK(viewport.begin==0&&viewport.end>=2&&viewport.width<284);r.tap(41);CHECK(r.ime.rawCursor()==2);}
 std::cout<<"PASS target captions, both scheme Down/Esc recalculation, zero range unchanged, short reselected prefix visible despite long tail\n";
}
#endif
#ifdef IME_FAULT_CONSUMER
int commit_fault=0;
void failures(){
 for(int mode:{1,2}){
  Rig r;r.begin(false,"ui",false);commit_fault=mode;r.tap(44);r.tap(54);r.type("ui");r.settle();commit_fault=0;
  CHECK(r.ime.queuedInput()==0);
  if(mode==1)CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect&&r.app.document.text().empty());
  else CHECK(r.ime.receipt().status==hanime::CompositionStatus::UnknownOutcome&&r.app.document.text()=="是");
  for(unsigned i=0;i<30;++i){r.step();}CHECK(r.app.document.text().find("，")==std::string_view::npos);
  if(mode==1){r.tap(44);r.settle();CHECK(r.app.document.text()=="是");}
  else {r.tap(41);CHECK(r.ime.receipt().status==hanime::CompositionStatus::UnknownOutcome);r.tap(44);r.settle();CHECK(r.app.document.text()=="是 "&&r.ime.receipt().status==hanime::CompositionStatus::UnknownOutcome); /* Existing Unknown interruption allows explicit English Space; no IME replay. */}
 }
 std::cout<<"PASS test-only real body consumer rejection and throw-after-acceptance: no punctuation replay, Unknown preserved\n";
}
#endif
int main(){try{finalPunctuation();
#ifndef IME_BASELINE_PROBE
boundaries();feedback();
#endif
#ifdef IME_FAULT_CONSUMER
failures();
#endif
return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
