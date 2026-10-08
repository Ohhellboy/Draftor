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
void test(unsigned id){
 if(id==16){R5Rig r(816);start(r,2,true);r.key(80);r.settle();r.key(81);r.settle();CHECK(r.ime.window()->browsingExpanded()&&r.ime.view()->col);const State s(r);first(r,s);second(r,s);return;}
 if(id<=6){unsigned scheme=(id-1)%3;bool vertical=id<=3;R5Rig r(800+id);start(r,scheme,vertical);
  const State s(r);r.trace("before");
  // Emit both old transitions before failing the counterexample assertion.
  r.key(41);r.Rig::settle();r.trace("observed_first");bool raw=r.ime.rawEditing()&&r.ime.raw()==s.raw;
  r.key(41);r.Rig::settle();r.trace("observed_second");CHECK(raw);s.cancelled(r);return;
 }
 for(unsigned scheme=0;scheme<3;++scheme)for(bool vertical:{false,true}){
  if(id==15&&scheme==2)continue; // DirectExact has no phonetic segmentation.
  R5Rig r(900+id*10+scheme*2+vertical);
  start(r,scheme,vertical,id==15?(scheme==1?"nihaoma":"wowowo"):nullptr);
  if(id==8){if(vertical){r.key(80);r.settle();}r.key(vertical?81:79);r.settle();CHECK(!vertical||r.ime.window()->browsingExpanded());CHECK(r.ime.view()->row||r.ime.view()->col);}
  if(id==10){r.key(44);r.settle();CHECK(!r.app.document.text().empty()&&r.ime.raw().empty());r.letters(scheme==2?"aaar":scheme==1?"shi":"ui");r.settle();}
  if(id==15){ // Select a real first segment through physical raw movement and load.
   r.key(vertical?82:80);r.settle();CHECK(r.ime.rawEditing());
   while(r.ime.rawCursor()!=2){const auto at=r.ime.rawCursor();r.key(vertical?(at>2?82:81):(at>2?80:79));r.settle();}
   r.key(vertical?80:81);r.settle();r.key(44);r.settle();
   CHECK(r.ime.composition()->snapshot().member_count&&r.app.document.text().empty());
   if(r.ime.rawEditing()){r.key(vertical?80:81);r.settle();}
   CHECK(!r.ime.rawEditing());
  }
  auto stale=r.freshEvent(30,true);const auto oldframe=r.frame;
  const State s(r);first(r,s);
  if(id==9){r.key(42);r.settle();CHECK(r.ime.raw()!=s.raw&&r.ime.rawEditing());}
  if(id==11){replay(r,stale);s.retained(r);CHECK(r.ime.rawEditing());
   const auto result=r.ime.window()->choose(0,oldframe);CHECK(result!=ChoiceResult::Captured);}
  if(id==11){
   // Raw edits can publish a newer frame after first Esc retired the old one.
   r.key(42);r.settle();r.letters(s.raw.substr(s.raw.size()-1));r.settle();
   CHECK(r.ime.raw()==s.raw&&r.ime.candidateAvailable());
   const auto receipt=r.ime.composition()->selection(0);const auto refreshed=r.frame;
   auto refreshed_number=r.freshEvent(30,true);second(r,s);
   replay(r,stale);replay(r,refreshed_number);s.cancelled(r);
   CHECK(r.ime.window()->choose(0,refreshed)!=ChoiceResult::Captured);
   CHECK(r.ime.composition()->select(receipt)==S::Stale);
   r.key(41);s.cancelled(r);
  }else second(r,s);
  if(id==12){const auto owner=r.ime.resourceOwner();auto old=r.freshEvent(4,true);r.key(44,1);r.settle();CHECK(r.ime.resourceOwner()!=owner);replay(r,old);CHECK(r.ime.raw().empty());s.bodySame(r);}
  if(id==13){r.key(44,1);r.settle();CHECK(!r.ime.active()&&r.ime.raw().empty());s.bodySame(r);}
  if(id==14){r.key(61);r.settle();CHECK(r.app.horizontalActive()==vertical);s.cancelled(r);}
 }
}
int main(int argc,char** argv){try{CHECK(argc==2);test(unsigned(std::stoul(argv[1])));
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS T"<<argv[1]<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
