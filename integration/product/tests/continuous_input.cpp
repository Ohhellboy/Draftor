#define main existing_query_rows_main
#include "query_rows.cpp"
#undef main
void immediate(){
 for(bool py:{false,true}){
  Rig r;r.begin(py,py?"shi":"ui",false);r.tap(0x29);r.settle();
  r.type(py?"shi":"ui");r.tap(0x2c);r.type(py?"shi":"ui");r.tap(0x2c);
  r.settle();CHECK(r.app.document.text()=="是是"&&r.ime.raw().empty());
 }
 std::cout<<"PASS early default plus next raw: both schemes, two atomic accepts\n";
}
void timing(){
 for(bool py:{false,true}){
  Rig r;r.begin(py,py?"shi":"ui",false);r.tap(0x29);r.settle();
  r.type(py?"wandanle":"wjdjle");r.tap(0x2c);r.tap(0x2c);r.settle();
  CHECK(r.app.document.text()=="完蛋了"&&r.ime.raw().empty());
  r.type(py?"shi":"ui");r.tap(0x28);r.tap(0x28);r.settle();CHECK(r.app.document.text()=="完蛋了是");
  r.type(py?"shi":"ui");r.tap(0x28);r.settle();CHECK(r.app.document.text()=="完蛋了是"&&r.ime.selected()=="是");
  r.tap(0x28);r.settle();CHECK(r.app.document.text()=="完蛋了是是");
  r.type(py?"shi":"ui");while(r.ime.hasWork())r.step(); // LCD intentionally not published.
  CHECK(r.ime.view()->generation!=r.frame);r.tap(0x2c);r.settle();CHECK(r.app.document.text()=="完蛋了是是是");
 }
 std::cout<<"PASS early suffix/default and delayed LCD; two real Enters retain two stages\n";
}
void cancellationAndCapacity(){
 for(uint8_t key:{uint8_t(0x29),uint8_t(0x2a),uint8_t(0x50)}){
  Rig r;r.tap(0x39);r.type("ui");r.tap(0x2c);r.type("ui");CHECK(!r.ime.composition());
  r.tap(key);r.settle();CHECK(r.app.document.text().empty()&&r.ime.queuedInput()==0);
 }

 {Rig r;r.begin(false,"ui",false);r.tap(0x2c);while(!r.ime.attemptSerial())r.step();r.type("ui");
  const auto queued=r.ime.queuedInput();CHECK(queued==2);
  s3hid::Event old{1,++r.now,0x29,0,true,0,r.frame,UINT32_MAX};old.ime_context=r.ime.ingressContext();old.resource_owner=r.ime.resourceOwner();r.ime.event(old);CHECK(r.ime.queuedInput()==queued);
  old.down=false;old.ms=++r.now;r.ime.event(old);r.settle();CHECK(r.app.document.text()=="是"&&r.ime.raw()=="ui");}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");r.tap(0x2c);
  while(r.app.document.text().empty())r.step();
  CHECK(r.ime.raw().empty()&&r.ime.queuedInput()>0&&r.ime.active());
  r.tap(0x39);CHECK(r.ime.chinese());r.settle();CHECK(r.app.document.text()=="是是");}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");r.tap(0x2c);
  while(r.app.document.text().empty())r.step();
  r.app.key(s3hid::Event{1,++r.now,0x50,0,true});r.settle();CHECK(r.app.document.text()=="是"&&r.ime.queuedInput()==0);}
 for(uint8_t cancel:{uint8_t(0x29),uint8_t(0x2a),uint8_t(0x50)}){
  Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");r.tap(cancel);r.settle();
  CHECK(r.app.document.text().empty()&&r.ime.queuedInput()==0);
  CHECK(r.ime.raw()!=(cancel==0x29?"ui":"uiui"));
 }
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");
  CHECK(r.app.commitText("body",r.app.editStamp()));r.settle();CHECK(r.app.document.text()=="body"&&r.ime.queuedInput()==0);}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");r.ime.interrupt();r.settle();CHECK(r.app.document.text().empty());}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type(std::string(33,'a'));
  CHECK(r.ime.queuedInput()==32&&r.ime.inputOverflow()==1);r.tap(0x29);r.settle();CHECK(r.ime.raw().empty());}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("q");r.tap(0x2c);r.type("ui");r.tap(0x2c);r.settle();CHECK(r.app.document.text().empty()&&r.ime.queuedInput()==0);}
 std::cout<<"PASS edit/Esc/target/interrupt/no-result retire successors; bounded capacity rejects newest\n";
}
void digitsAndOldFrames(){
 {Rig r;r.tap(0x39);r.type("ui");r.tap(0x2c);r.type("ui");r.tap(0x2c);r.settle();CHECK(r.app.document.text()=="是是");}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x2c);r.type("ui");
  const auto stamp=r.ime.composition()->raw_input().stamp;r.ime.window()->query();CHECK(r.ime.composition()->input_is_current(stamp));
  r.settle();CHECK(r.app.document.text().empty()&&r.ime.queuedInput()==0);}
 {Rig r;r.begin(false,"wjdjle",false);r.tap(0x1e);r.tap(0x2c);r.settle();CHECK(r.app.document.text()=="完蛋了");}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");r.tap(0x1e);r.settle();CHECK(r.app.document.text()=="是");}
 {Rig r;r.begin(false,"ui",false);const auto old=r.frame;r.tap(0x29);r.type("wj");r.settle();
  r.type("dj");const auto live=r.frame;r.frame=old;r.tap(0x2c);r.frame=live;r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected().empty());}
 {Rig r;r.begin(false,"ui",false);r.tap(0x29);r.type("ui");s3hid::Event e{1,++r.now,0x2c,0,true,0,r.frame,r.ime.attemptSerial()};
  e.ime_context=r.ime.ingressContext();e.resource_owner=r.ime.resourceOwner();r.ime.event(e);r.ime.event(e);e.ms=++r.now;r.ime.event(e);e.down=false;e.ms=++r.now;r.ime.event(e);r.settle();CHECK(r.app.document.text()=="是"&&!r.ime.active());}
 std::cout<<"PASS digits fixed displayed row, unknown number refusal, old external frame and repeats isolated\n";
}
void selectionCost(){
 for(bool py:{false,true})for(bool early:{false,true}){
  Rig r;r.begin(py,py?"shi":"ui",false);r.tap(0x29);r.settle();
  const auto io=metrics.io();const auto rows=r.ime.window()->costs().built_rows;
  r.type(py?"shi":"ui");if(!early)r.settle();r.tap(0x2c);r.settle();
  CHECK(r.app.document.text()=="是");const auto d=metrics.io()-io;
  std::cout<<"INTENT_COST py="<<py<<" early="<<early<<" bytes="<<d.bytes<<" reads="<<d.reads<<" seeks="<<d.seeks<<" rows="<<r.ime.window()->costs().built_rows-rows<<" same_output=1 official_page_and_submission=1\n";
 }
}
int main(){try{immediate();timing();cancellationAndCapacity();digitsAndOldFrames();selectionCost();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
