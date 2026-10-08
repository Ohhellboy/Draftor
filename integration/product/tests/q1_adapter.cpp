#define main original_query_rows_main
#include "query_rows.cpp"
#undef main

void drainWindow(CandidateWindow& window){
 unsigned steps=0;while(window.busy()){window.service();CHECK(++steps<2000000);}
}
void queryStates(){
 Rig r;r.begin(true,"wo",false);
 // Independent compositions share the one port only while the other is idle.
 for(unsigned kind=0;kind<4;++kind){
  hanime_extension::Config cfg;
  if(kind==0)cfg.planning.ranges=1;
  if(kind==1)cfg.planning.local_work=1;
  if(kind==2)cfg.planning.core_work=1;
  if(kind==3)cfg.planning.read_bytes=1;
  hanime_extension::Composition c(r.resources.scheme,r.resources.shared,r.resources.rules(),cfg);
  CandidateWindow window(c,r.font);
  CHECK(c.edit_raw(0,0,"wojintian")==hanime_extension::Status::Ready);
  window.query();drainWindow(window);
  CHECK(window.state()==WindowState::Limited&&window.planningLimited());
  const auto before=metrics.io();window.request(1,0,true);drainWindow(window);
  CHECK(window.state()==WindowState::Limited&&metrics.io().reads==before.reads);
  CHECK(c.snapshot().raw=="wojintian"&&r.app.document.text().empty());
  window.query(2);drainWindow(window);CHECK(window.state()==WindowState::Ready);
  CHECK(window.view().rows[0].items[0].end==2);
 }
 {
  hanime_extension::Composition c(r.resources.scheme,r.resources.shared,r.resources.rules());
  CandidateWindow window(c,r.font);CHECK(c.edit_raw(0,0,"qq")==hanime_extension::Status::Ready);
  window.query();drainWindow(window);CHECK(window.state()==WindowState::Unresolved);
  window.request(0,1,false);drainWindow(window);CHECK(window.state()==WindowState::Unresolved);
  CHECK(c.edit_raw(0,2,"q")==hanime_extension::Status::Ready);
  // An explicit one-letter prefix has no full-pinyin mapping. Production "a"
  // does have candidates, unlike the extension's deliberately sparse fixture.
  window.query(1);drainWindow(window);CHECK(window.state()==WindowState::NoResults);
  CHECK(c.edit_raw(0,1,"wojintian")==hanime_extension::Status::Ready);
  window.query();const auto old=c.operation();window.service();window.cancel();
  CHECK(!c.plan_is_current(c.planning_report()));
  CHECK(c.edit_raw(0,9,"wo")==hanime_extension::Status::Ready);
  window.query();CHECK(c.cancel(old)==hanime_extension::Status::Stale);drainWindow(window);
  CHECK(window.state()==WindowState::Ready&&window.view().rows[0].items[0].end==2);
 }
 std::cout<<"PASS adapter Limited/Unresolved/NoResults distinct; raw retained, manual prefix usable, original cancellation isolated\n";
}
void rapidAndOldFrame(bool full){
 Rig r;r.begin(full,"wo",false);
 const auto oldPlan=r.ime.composition()->planning_report();
 const auto oldOp=r.ime.composition()->operation();const auto oldFrame=r.frame;
 r.type(full?"jintian":"jbtm");r.settle();
 CHECK(!r.ime.composition()->plan_is_current(oldPlan));
 CHECK(r.ime.composition()->cancel(oldOp)==hanime_extension::Status::Stale);
 CHECK(r.ime.window()->choose(0,oldFrame)==ChoiceResult::ExpiredInput);
 CHECK(r.app.document.text().empty());
 r.ime.window()->query();r.settle();
 CHECK(r.ime.view()->rows[0].items[0].end==2);
 const auto plan=r.ime.composition()->planning_report();
 r.tap(0x2c,1);r.settle();
 CHECK(r.ime.raw().empty()&&r.app.document.text().empty());
 CHECK(!r.ime.composition()->plan_is_current(plan));
 std::cout<<"PASS rapid raw append old plan/op/frame isolation and scheme discard full="<<full<<'\n';
}
int main(){try{queryStates();rapidAndOldFrame(false);rapidAndOldFrame(true);return 0;}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
