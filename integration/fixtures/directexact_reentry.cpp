#define main inherited_wubi_main
#include "wubi_product.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_ime.hpp"
struct R5Rig:Rig {
 s3app::ProductInput product;
 explicit R5Rig(unsigned id,const fs::path& root=REAL_RESOURCES,ReleasePins pins=productionPins()):Rig(root,id,pins){}
 void deliver(const s3hid::Event& e){if(!product.layout(app,e)&&!product.ime(app,&ime,e))app.key(e);}
 void key(uint8_t u,uint8_t mods=0){for(bool down:{true,false})deliver(freshEvent(u,down,mods));}
 void letters(std::string_view s){for(char c:s)key(uint8_t(c-'a'+4));}
 // Publish a real current Vertical presentation before the physical report.
 // Rig::settle alone exercises the generic Host frame completion helper.
 void settle(){
  Rig::settle();
  if(ime.raw().empty()||!ime.candidateAvailable())return;
  for(unsigned n=0;n<4&&!prepareImePresentation(&ime,app);++n)Rig::settle();
  CHECK(prepareImePresentation(&ime,app));
  if(app.horizontalActive())return;
  struct Paint{unsigned raw=0,candidates=0;} painted;
  draftor::vertical::AbPort draw;draw.context=&painted;
  draw.line=[](void*,draftor::vertical::Rect,bool){};
  draw.run=[](void* p,const draftor::vertical::AbRun& run){
   auto& out=*static_cast<Paint*>(p);
   if(run.role==draftor::vertical::AbRole::Composition)++out.raw;
   if(run.role==draftor::vertical::AbRole::CurrentCandidate){CHECK(run.identity);++out.candidates;}
  };
  const auto receipt=verticalIme(&ime,app,now,0,draw);
  CHECK(receipt.layout.status==draftor::vertical::AbStatus::Ready);
  CHECK(receipt.candidate_frame&&painted.raw&&painted.candidates);
  frame=publishVerticalFrame(&ime,app,receipt);CHECK(frame);
  std::cout<<"vertical_published frame="<<frame<<" generation="<<receipt.generation<<" revision="<<receipt.revision<<std::endl;
 }
 void setup(){key(57);settle();key(44,1);settle();key(44,1);settle();CHECK(resources.rules()==R::DirectExact);}
 void trace(const char* stage){const auto s=ime.composition()->snapshot();std::cout<<stage<<" layout="<<(app.horizontalActive()?"H":"V")<<" raw="<<ime.raw()<<" rawEditing="<<ime.rawEditing()<<" cursor="<<ime.rawCursor()<<" revision="<<s.revision<<" query="<<ime.window()->querySerial()<<" body_bytes="<<app.document.text().size()<<std::endl;}
};


struct State {
 std::string raw, selected, body; size_t cursor, undo, focus;
 uint64_t input, revision, body_revision; uint32_t owner;
 std::vector<hanime_extension::Member> members;
 explicit State(R5Rig& r): raw(r.ime.raw()),body(r.app.document.text()),
 cursor(r.app.document.cursorByte()),undo(r.app.editor.undoCount()),
 body_revision(r.app.document.textRevision()),owner(r.ime.resourceOwner()) {
  const auto s=r.ime.composition()->snapshot();selected=s.selected_text;
  input=s.input_id;revision=s.revision;focus=s.focus;
  members.assign(s.members,s.members+s.member_count);
 }
 void bodySame(R5Rig& r)const {
  CHECK(std::string_view(r.app.document.text())==body);
  CHECK(r.app.document.cursorByte()==cursor&&r.app.document.textRevision()==body_revision);
  CHECK(r.app.editor.undoCount()==undo);
 }
 void retained(R5Rig& r)const {
  bodySame(r);const auto s=r.ime.composition()->snapshot();
  CHECK(s.raw==raw&&s.selected_text==selected&&s.input_id==input&&s.revision==revision&&s.focus==focus&&s.member_count==members.size());
  for(size_t i=0;i<members.size();++i){const auto& a=members[i];const auto& b=s.members[i];
   CHECK(a.key==b.key&&a.ref==b.ref&&a.begin==b.begin&&a.end==b.end&&a.valid==b.valid&&a.invocation==b.invocation&&a.form_begin==b.form_begin&&a.form_size==b.form_size&&a.text_begin==b.text_begin&&a.text_size==b.text_size);
  }
  CHECK(r.ime.resourceOwner()==owner);
 }
 void cancelled(R5Rig& r)const {
  bodySame(r);const auto s=r.ime.composition()->snapshot();
  CHECK(r.ime.raw().empty()&&!r.ime.rawEditing()&&!r.ime.active());
  CHECK(s.member_count==0&&s.selected_text.empty());
  CHECK(!r.ime.candidateAvailable()&&!r.ime.hasWork()&&!r.ime.window()->busy()&&!r.ime.window()->selecting());
  CHECK(r.ime.resourceOwner()==owner);
 }
};
void start(R5Rig& r,unsigned scheme,bool vertical,const char* code=nullptr){
 r.key(57);r.settle();for(unsigned i=0;i<scheme;++i){r.key(44,1);r.settle();}
 if(vertical)r.key(61);
 r.letters(code?code:(scheme==2?"aaar":scheme==1?"shi":"ui"));r.settle();
 CHECK(r.ime.candidateAvailable()&&!r.ime.rawEditing());
}
void first(R5Rig& r,const State& before){
 const auto end=r.ime.editTarget().end;const auto frame=r.frame;const auto context=r.ime.ingressContext();
 r.key(41);r.Rig::settle();r.trace("first_esc");before.retained(r);
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==end&&r.ime.ingressContext()!=context);
 CHECK(r.ime.window()->state()==WindowState::Empty&&!r.ime.hasWork());
 const auto* old=r.ime.window()->presented(frame);CHECK(!old||old->count==0);
}
void second(R5Rig& r,const State& before){r.key(41);r.Rig::settle();r.trace("second_esc");before.cancelled(r);}
void replay(R5Rig& r,s3hid::Event old){r.deliver(old);old.down=false;old.ms=++r.now;r.deliver(old);r.Rig::settle();}
void fullCurrent(R5Rig& r,const State& s){
 s.retained(r);CHECK(!r.ime.rawEditing()&&r.ime.candidateAvailable());
 const auto snap=r.ime.composition()->snapshot();const auto page=r.ime.composition()->page();
 CHECK(page.begin==0&&page.end==r.ime.raw().size());
 const auto& v=*r.ime.view();CHECK(v.count);
 for(size_t i=0;i<v.count;++i)for(size_t j=0;j<v.rows[i].count;++j){const auto& c=v.rows[i].items[j];
  CHECK(c.begin==0&&c.end==r.ime.raw().size());
  CHECK(c.input==snap.input_id&&c.revision==snap.revision&&c.scheme==snap.scheme_epoch);
  CHECK(c.activation.mount_epoch()==snap.activation.mount_epoch()&&c.activation.shared_bundle_id()==snap.activation.shared_bundle_id());
 }
 r.trace("full_current_candidate");
}
void returnRaw(R5Rig& r){r.key(r.app.horizontalActive()?81:80);r.settle();r.trace("raw_return_observed");}
void testReentry(unsigned id){
 if(id>=19&&id<=22){const bool vertical=id%2==0;const unsigned scheme=id<=20?0:1;
  R5Rig r(23600+id);start(r,scheme,vertical,scheme?"nihaoma":"wowowo");const State original(r);
  r.key(41);r.settle();while(r.ime.rawCursor()>2){r.key(vertical?82:80);r.settle();}
  CHECK(r.ime.editTarget().valid()&&r.ime.editTarget().end==2);
  returnRaw(r);CHECK(!r.ime.rawEditing());CHECK(r.ime.composition()->page().begin==0&&r.ime.composition()->page().end==2);
  original.retained(r);r.trace("phonetic_prefix_0_2_preserved");return;
 }
 bool vertical=id==2||id==5||id==8||id==9||id==10||id==23||id==24;
 const bool middle=id==14||id==24;
 const auto root=middle?fixture(true):fs::path(REAL_RESOURCES);
 R5Rig r(23600+id,root,middle?fixturePins():productionPins());start(r,2,vertical,middle?"aba":nullptr);const State original(r);
 if(id==8){r.key(80);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.window()->browsingExpanded());original.retained(r);return;}
 if(id==9){r.key(82);r.settle();CHECK(r.ime.rawEditing());original.retained(r);return;}
 if(id==10){r.key(80);r.settle();r.key(81);r.settle();r.key(82);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.window()->browsingExpanded());original.retained(r);return;}
 const auto oldframe=r.frame;auto oldevent=r.freshEvent(30,true);const auto oldselection=r.ime.composition()->selection(0);
 r.key(41);r.settle();CHECK(r.ime.rawEditing());original.retained(r);
 if(id==3||id==5||id==12||id==17||id==18){r.key(42);CHECK(r.ime.raw()=="aaa");if(id!=12&&id!=17)r.settle();}
 if(id==4||id==23){r.key(42);r.letters("b");r.settle();CHECK(r.ime.raw()=="aaab"&&r.ime.window()->state()==WindowState::NoResults);
  const State s(r);const auto cursor=r.ime.rawCursor();const auto q=r.ime.window()->querySerial();returnRaw(r);s.retained(r);
  CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==cursor&&!r.ime.candidateAvailable());CHECK(r.ime.window()->querySerial()==q);
  replay(r,oldevent);s.retained(r);CHECK(r.ime.window()->choose(0,oldframe)!=ChoiceResult::Captured);return;
 }
 if(id==11){r.key(80);CHECK(r.ime.rawCursor()==3);r.key(79);CHECK(r.ime.rawCursor()==4);r.key(42);r.settle();CHECK(r.ime.raw()=="aaa"&&r.ime.rawCursor()==3);r.key(80);r.key(76);r.settle();CHECK(r.ime.raw()=="aa"&&r.ime.rawCursor()==2);original.bodySame(r);}
 if(id==13){auto stale=r.freshEvent(81,true);r.key(41);r.settle();r.key(44,1);r.settle();CHECK(r.ime.resourceOwner()!=original.owner);r.letters("ui");r.settle();const State replaced(r);replay(r,stale);replay(r,oldevent);replaced.retained(r);return;}
 if(middle){ // Synthetic collision codes from the historical raw-edit fixture.
  r.key(vertical?82:80);r.settle();CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==2);
  r.letters("b");r.settle();CHECK(r.ime.raw()=="abba"&&r.ime.rawCursor()==3);
 }
 if(id==15){r.key(80);r.key(80);CHECK(r.ime.rawCursor()==2);}
 if(id==16){ // A true-unique four-code typed while raw must not gain a new auto trigger.
  r.key(42);r.key(42);r.key(42);r.key(80);r.letters("l");r.key(76);r.letters("tif");r.settle();
  CHECK(r.ime.raw()=="ltif"&&r.ime.rawEditing());
 }
 if(id==17){CHECK(r.ime.window()->busy());r.key(81);r.letters("b");r.settle();CHECK(r.ime.raw()=="aaab"&&r.ime.rawEditing()&&!r.ime.candidateAvailable());original.bodySame(r);return;}
 if(id==18){const State s(r);replay(r,oldevent);s.retained(r);CHECK(r.ime.composition()->select(oldselection)==S::Stale);CHECK(r.ime.window()->choose(0,oldframe)!=ChoiceResult::Captured);}
 const State s(r);const auto q=r.ime.window()->querySerial();const bool reuse=r.ime.window()->busy()||(r.ime.window()->state()==WindowState::Ready&&r.ime.window()->focusedCurrent());
 if(id==12){CHECK(r.ime.window()->busy());r.key(81);CHECK(r.ime.rawEditing());CHECK(r.ime.window()->querySerial()==q);r.settle();}
 else returnRaw(r);
 fullCurrent(r,s);if(reuse)CHECK(r.ime.window()->querySerial()==q);
 if(id==6){for(int n=0;n<3;++n){r.key(41);r.settle();CHECK(r.ime.rawEditing());returnRaw(r);fullCurrent(r,s);}}
 if(id==7){r.key(41);r.settle();CHECK(r.ime.rawEditing());s.retained(r);r.key(41);r.settle();s.cancelled(r);}
}
int main(int argc,char** argv){try{CHECK(argc==2);testReentry(unsigned(std::stoul(argv[1])));CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS R"<<argv[1]<<std::endl;return 0;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
