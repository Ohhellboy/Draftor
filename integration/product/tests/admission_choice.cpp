#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
void reviewerCases(){
 for(bool wait:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)}){
  Rig r;r.begin(false,"bu",false);r.tap(0x50);r.tap(0x4f);r.tap(0x2a);r.type("u");
  CHECK(r.ime.raw()=="bu"&&r.ime.rawCursor()==2&&r.ime.window()->busy());
  if(wait)r.settle();
  r.tap(0x51);r.tap(key);r.settle();
  std::cout<<"VALID_DOWN wait="<<wait<<" key="<<unsigned(key)<<" body="<<r.app.document.text()<<" raw="<<r.ime.raw()<<" available="<<r.ime.candidateAvailable()<<'\n';
  CHECK(r.app.document.text()=="不");
 }
 std::cout<<"PASS four independent reviewer cases\n";
}
void pending(Rig& r,bool py=false,bool selected=false){
 r.begin(py,selected?(py?"doubu":"dzbu"):"bu",false);
 if(selected){r.ime.window()->query(py?3:2);r.settle();r.tap(44);r.settle();CHECK(r.ime.selected()=="都");}
 r.tap(80);while(r.ime.rawCursor()<r.ime.raw().size())r.tap(79);
 r.tap(42);r.type("u");CHECK(r.ime.window()->busy());r.tap(81);
}
void ordered(){
 for(bool py:{false,true})for(bool selected:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)})for(bool admitted:{false,true}){
  Rig r;pending(r,py,selected);const auto prior=r.ime.window()->querySerial();
  if(admitted){unsigned n=0;while(r.ime.window()->querySerial()==prior){r.step();CHECK(++n<2000000);}CHECK(r.ime.window()->busy());}
  r.tap(key);r.tap(54);r.type(py?"shi":"ui");r.tap(key);r.tap(55);
  CHECK(r.ime.raw()==(selected?(py?"doubu":"dzbu"):"bu"));
  CHECK(r.ime.queuedInput()>=4);r.settle();
  CHECK(r.app.document.text()==(selected?"都不，是。":"不，是。"));CHECK(!r.ime.active());
 }
 std::cout<<"PASS admission/formal Working, both schemes, selected prefix, ordered punctuation/raw and atomic commit\n";
}
void failureIsolation(){
 for(bool py:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)}){
  Rig r;r.begin(py,"bu",false);r.tap(80);r.tap(79);r.tap(80);r.tap(76);r.type("b");
  CHECK(r.ime.rawCursor()==1&&r.ime.window()->busy());r.tap(81);r.tap(key);r.tap(54);r.type("ui");
  CHECK(r.ime.queuedInput()==3);r.settle();CHECK(r.ime.raw()=="bu"&&r.ime.selected().empty()&&r.app.document.text().empty()&&r.ime.queuedInput()==0);
  CHECK(r.ime.candidateAvailable());r.tap(key);r.settle();CHECK(r.app.document.text()=="不");
 }
 std::cout<<"PASS invalid odd and legal no-match retire original demand+successors before automatic recovery\n";
}
void admissionCancellation(){
 for(bool admitted:{false,true})for(unsigned change=0;change<10;++change){
  Rig r;pending(r);r.tap(44);r.tap(54);r.type("ui");
  const auto old=r.ime.composition()->operation();const auto query=r.ime.window()->querySerial();
  if(admitted){unsigned n=0;while(r.ime.window()->querySerial()==query){r.step();CHECK(++n<2000000);}}
  if(change==0)r.tap(80);
  if(change==1)r.tap(79);
  if(change==2)r.tap(81); // New Down, no old choice carried into it.
  if(change==3)r.tap(41);
  if(change==4)r.tap(42);
  if(change==5)r.ime.window()->query();
  if(change==6)CHECK(r.app.commitText("body",r.app.editStamp()));
  if(change==7)r.tap(44,1);
  if(change==8)r.ime.interrupt();
  if(change==9){r.ime.window()->cancel();CHECK(r.ime.composition()->edit_raw(1,2,"a")==hanime_extension::Status::Ready);r.ime.window()->query();}
  r.settle();CHECK(r.app.document.text()==(change==6?"body":""));CHECK(r.ime.queuedInput()==0);
  if(change==5||change==7||change==9)CHECK(r.ime.composition()->cancel(old)==hanime_extension::Status::Stale);
 }
 {Rig r;pending(r);r.tap(44);r.tap(54);++r.resources.mount()->epoch;for(unsigned n=0;n<8;++n)r.ime.service();CHECK(r.ime.queuedInput()==0&&r.app.document.text().empty());}
 std::cout<<"PASS pending/Working move/new Down/Esc/edit/query/target/scheme/interrupt/resource invalidation\n";
}
void frameAndCapacity(){
 {Rig r;r.begin(false,"wjdjle",false);r.tap(80);r.tap(80);r.tap(42);r.type("j");
  CHECK(r.ime.rawCursor()==2);r.tap(81);r.tap(44);r.settle();CHECK(r.ime.selected()=="玩"&&r.ime.composition()->snapshot().focus==2&&r.app.document.text().empty());}
 {Rig r;r.begin(false,"wjdj",false);CHECK(r.ime.view()->rows[0].count<6);r.tap(80);r.tap(79);r.tap(42);r.type("j");r.tap(81);r.tap(35);r.tap(54);r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected().empty()&&r.ime.queuedInput()==0);}
 {Rig r;pending(r);r.tap(44);for(unsigned i=0;i<33;++i)r.tap(54);CHECK(r.ime.queuedInput()==32&&r.ime.inputOverflow()==1);r.settle();CHECK(std::string(r.app.document.text())==std::string("不")+[](){std::string s;for(unsigned i=0;i<32;++i)s+="，";return s;}());}
 {Rig r;pending(r);const auto context=r.ime.ingressContext();
  s3hid::Event e{1,++r.now,44,0,true,0,r.frame,r.ime.attemptSerial()};e.ime_context=context;e.resource_owner=r.ime.resourceOwner();r.ime.event(e);r.ime.event(e);auto repeated=e;repeated.ms=++r.now;r.ime.event(repeated);e.down=false;e.ms=++r.now;r.ime.event(e);
  // Same HID batch ingress on the successor; no service between events.
  s3hid::Event comma{1,++r.now,54,0,true,0,r.frame,0};comma.ime_context=context;comma.resource_owner=r.ime.resourceOwner();r.ime.event(comma);comma.down=false;comma.ms=++r.now;r.ime.event(comma);
  r.settle();CHECK(r.app.document.text()=="不，");r.ime.event(repeated);r.settle();CHECK(r.app.document.text()=="不，");}
 {Rig r;pending(r);s3hid::Event stale{1,++r.now,41,0,true,0,r.frame,0};stale.ime_context=r.ime.ingressContext();stale.resource_owner=r.ime.resourceOwner();r.tap(44);const auto q=r.ime.window()->querySerial();while(r.ime.window()->querySerial()==q)r.step();
  r.ime.window()->query();r.ime.event(stale);stale.down=false;stale.ms=++r.now;r.ime.event(stale);r.settle();CHECK(r.app.document.text().empty()&&!r.ime.raw().empty());r.tap(41);CHECK(r.ime.raw().empty());}
 std::cout<<"PASS different auto identity/range, absent number, capacity32, same-batch/repeat/release and old Esc\n";
}
int main(){try{reviewerCases();ordered();failureIsolation();admissionCancellation();frameAndCapacity();
 std::cout<<"PASS admission choice R1 sizeof_controller="<<sizeof(DeviceIme)<<" sizeof_window="<<sizeof(CandidateWindow)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
