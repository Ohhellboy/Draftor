// R2 proofs deliberately exercise the repaired legacy Host birth helper.
#define PHASE2C_REUSE_RIG
#include "runtime_layout_2c.cpp"
#include <set>
namespace {
using Rules=hanime_extension::SchemeRules;
struct Snapshot {
 BodyFact body;std::string raw,selected;std::uint64_t candidates;std::uint32_t query;
 bool find,files;std::size_t arrows,queued;
 explicit Snapshot(SwitchRig& r):body(r.app),raw(r.ime.raw()),selected(r.ime.selected()),
  candidates(digest(*r.ime.view())),query(r.ime.window()->querySerial()),find(r.app.findOpen()),
  files(r.app.filesOpen()),arrows(r.ime.queuedArrows()),queued(r.ime.queuedInput()){}
 void check(SwitchRig& r)const{
  body.check(r.app);CHECK(r.ime.raw()==raw&&r.ime.selected()==selected);
  CHECK(digest(*r.ime.view())==candidates&&r.ime.window()->querySerial()==query);
  CHECK(r.app.findOpen()==find&&r.app.filesOpen()==files);
  CHECK(r.ime.queuedArrows()==arrows&&r.ime.queuedInput()==queued&&!r.ime.hasWork());
 }
};
void reject(SwitchRig& r,const std::vector<s3hid::Event>& stored){
 const Snapshot before(r);
 for(const auto& e:stored){const auto owner=e.resource_owner,context=e.ime_context;
  CHECK(owner!=r.ime.resourceOwner());r.deliver(e);
  CHECK(e.resource_owner==owner&&e.ime_context==context);before.check(r);
 }
 for(unsigned i=0;i<12;++i)r.step();before.check(r);
}
void lifetime(){
 SwitchRig r;r.begin("ui");CHECK(r.ime.candidateAvailable());
 const auto initial=r.ime.resourceOwner();std::set<std::uint32_t> owners{initial};
 std::vector<s3hid::Event> old;
 for(const auto key:{std::uint8_t(44),std::uint8_t(41),std::uint8_t(29),std::uint8_t(30)}){
  const auto down=r.event(key);auto up=down;up.down=false;up.ms=++r.now;
  CHECK(down.resource_owner==initial&&down.ime_context==r.ime.ingressContext());old.push_back(down);old.push_back(up);
 }
 const auto oldFind=r.event(9,true,1);old.push_back(oldFind);auto oldFindUp=oldFind;oldFindUp.down=false;oldFindUp.ms=++r.now;old.push_back(oldFindUp);
 r.tap(44,1);r.settle();CHECK(r.resources.rules()==Rules::FullPinyin);
 CHECK(owners.insert(r.ime.resourceOwner()).second);
 auto fresh=r.event(22);CHECK(fresh.resource_owner==r.ime.resourceOwner()&&fresh.resource_owner!=initial);
 CHECK(fresh.ime_context==r.ime.ingressContext());r.deliver(fresh);r.deliver(r.event(22,false));r.type("hi");r.settle();
 CHECK(r.ime.raw()=="shi"&&r.ime.candidateAvailable());reject(r,old);
 std::cout<<"PASS R2.1 fresh after switch uses current owner; R2.2 stored input/cancel/selection/App shortcut rejected with raw/body/candidates unchanged\n";
 r.tap(44,1);r.settle();CHECK(r.resources.rules()==Rules::DirectExact);CHECK(owners.insert(r.ime.resourceOwner()).second);reject(r,old);
 r.tap(44,1);r.settle();CHECK(r.resources.rules()==Rules::R1Xiaohe);CHECK(owners.insert(r.ime.resourceOwner()).second);
 CHECK(r.ime.resourceOwner()!=initial);reject(r,old);
 r.type("ui");r.settle();CHECK(r.ime.raw()=="ui"&&r.ime.candidateAvailable());reject(r,old);
 r.tap(44);r.settle();CHECK(r.app.document.text()=="是"&&r.app.editor.undoCount()==1);reject(r,old);
 // EN fallthrough is guarded as well; delayed old letter cannot insert body text.
 r.tap(57);CHECK(!r.ime.chinese());reject(r,old);
 std::cout<<"PASS R2.3 return to same scheme never revives owner; R2.4 fresh returned-XH input commits once; old EN input cannot fall through\n";
}
void release(){
 SwitchRig r;r.tap(57);r.settle();CHECK(r.ime.raw().empty());
 const auto down=r.event(4);auto up=down;up.down=false;up.ms=++r.now;
 const auto original=down.resource_owner;r.deliver(down);r.settle();CHECK(r.ime.raw()=="a");
 r.tap(44,1);r.settle();CHECK(r.ime.resourceOwner()!=original&&r.ime.raw().empty());
 r.deliver(up);CHECK(up.resource_owner==original&&up.ime_context==down.ime_context);
 const auto fresh=r.event(4);r.deliver(fresh);r.settle();CHECK(r.ime.raw()=="a");
 // Replaying the old release must not unlatch this newer held press.
 r.deliver(up);auto repeat=fresh;repeat.ms=++r.now;r.deliver(repeat);r.settle();CHECK(r.ime.raw()=="a");
 r.deliver(r.event(4,false));r.tap(4);r.settle();CHECK(r.ime.raw()=="aa");
 CHECK(up.resource_owner==original&&repeat.resource_owner==fresh.resource_owner);
 std::cout<<"PASS R2.5 retained old up releases original press; replay cannot unlatch newer held press; fresh up permits next press; no direct held-state reset\n";
}
}
int main(){try{lifetime();release();CHECK(metrics.resource_handles==0&&metrics.document_handles==0);return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
