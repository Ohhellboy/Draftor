#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
#include <functional>
struct Saved {
 std::string raw,text,body;std::vector<hanime_extension::Member> members;
 uint64_t input,revision;size_t focus;
 explicit Saved(LongRig& r){const auto s=r.ime.composition()->snapshot();raw=s.raw;text=s.selected_text;body=r.app.document.text();members.assign(s.members,s.members+s.member_count);input=s.input_id;revision=s.revision;focus=s.focus;}
 void cancelled(LongRig& r)const{
  const auto s=r.ime.composition()->snapshot();CHECK(r.ime.raw().empty()&&!r.ime.rawEditing()&&!r.ime.active());
  CHECK(s.member_count==0&&s.selected_text.empty()&&!r.ime.candidateAvailable()&&!r.ime.hasWork());
  CHECK(std::string_view(r.app.document.text())==body);
 }
 void check(LongRig& r)const{
  const auto s=r.ime.composition()->snapshot();CHECK(s.raw==raw&&s.selected_text==text&&s.input_id==input&&s.revision==revision&&s.focus==focus&&s.member_count==members.size());CHECK(std::string_view(r.app.document.text())==body);
  for(size_t i=0;i<members.size();++i){const auto& a=members[i];const auto& b=s.members[i];CHECK(a.key==b.key&&a.ref==b.ref&&a.begin==b.begin&&a.end==b.end&&a.valid==b.valid&&a.invocation==b.invocation&&a.form_begin==b.form_begin&&a.form_size==b.form_size&&a.text_begin==b.text_begin&&a.text_size==b.text_size);}
 }
};
void at(LongRig& r,size_t end){
 if(!r.ime.rawEditing())r.key(0x50);
 CHECK(r.ime.rawEditing());
 for(size_t n=0;r.ime.rawCursor()!=end;++n){CHECK(n<70);const auto p=r.ime.rawCursor();r.key(p<end?0x4f:0x50);CHECK(r.ime.rawCursor()!=p);}
}
void target(LongRig& r,EditTargetKind kind,size_t begin,size_t end,size_t member=0){const auto t=r.ime.editTarget();std::cout<<"TARGET cursor="<<r.ime.rawCursor()<<" kind="<<int(t.kind)<<" span="<<t.begin<<':'<<t.end<<" member="<<t.member<<'\n';CHECK(t.kind==kind&&t.begin==begin&&t.end==end);if(kind==EditTargetKind::Member)CHECK(t.member==member);const auto v=r.ime.editViewport(r.font);CHECK(v.cursor_x==v.at(r.ime.rawCursor()));}
void choosePrefix(LongRig& r,size_t end,uint8_t choice=44){r.ime.window()->query(end);r.ready();CHECK(r.ime.window()->state()==WindowState::Ready);const auto item=r.ime.view()->rows[0].items[0];CHECK(item.end==end);r.key(choice);r.ready();const auto s=r.ime.composition()->snapshot();CHECK(s.focus==end&&s.member_count&&s.members[s.member_count-1].key==item.key&&s.members[s.member_count-1].ref==item.ref);}
void adjacent(bool py,bool reopen){
 LongRig r("ic1_adjacent");r.start(py,py?"nihaoma":"wowowo");choosePrefix(r,2);choosePrefix(r,py?5:4);at(r,2);
 const auto before=r.ime.editTarget();std::cout<<"BEFORE_REOPEN raw="<<r.ime.raw().size()<<" cursor=2 target="<<before.begin<<':'<<before.end<<'\n';
 if(!reopen){target(r,EditTargetKind::Member,0,2);return;}
 r.key(0x51);r.ready();r.key(0x29);r.ready();CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==2);
 const auto after=r.ime.editTarget();std::cout<<"AFTER_REOPEN raw="<<r.ime.raw().size()<<" cursor=2 target="<<after.begin<<':'<<after.end<<'\n';CHECK(before.begin==0&&before.end==2&&after.begin==0&&after.end==2);
}
void geometry(bool py){
 LongRig r("ic1_geometry");r.start(py,py?"nihaoma":"wjdjwo");
 at(r,0);target(r,EditTargetKind::Boundary,0,0);
 at(r,r.ime.raw().size());target(r,EditTargetKind::Prefix,0,r.ime.raw().size());
 // Real selection creates one multi-unit member; no private state fabrication.
 r.key(0x51);r.ready();choosePrefix(r,py?5:4); // Explicit raw range load for geometry; Esc now cancels.
 at(r,py?5:4);target(r,EditTargetKind::Member,0,py?5:4);
 at(r,2);target(r,EditTargetKind::Member,0,2);
 r.key(0x51);r.ready();CHECK(r.ime.editTarget().begin==0&&r.ime.editTarget().end==2);
 r.key(44);r.ready();CHECK(r.ime.composition()->snapshot().focus==2);
 at(r,2);target(r,EditTargetKind::Member,0,2);
 at(r,py?5:4);target(r,EditTargetKind::Prefix,2,py?5:4);
}
void escapes(bool py,unsigned kind){
 LongRig r("ic1_esc");r.start(py,py?"nihaoa":"wowowo");
 if(kind==1||kind==2)choosePrefix(r,2);
 if(kind==2){at(r,2);r.key(0x51);r.ready();CHECK(r.ime.reselecting());}
 const Saved saved(r);const auto visible=r.ime.editTarget();const size_t origin=r.ime.rawCursor();
 const auto context=r.ime.ingressContext();const auto oldframe=r.frame;
 r.key(0x29);saved.check(r);CHECK(r.ime.rawEditing());CHECK(r.ime.rawCursor()==(kind==2?origin:visible.end));CHECK(r.ime.ingressContext()!=context);
 CHECK(r.ime.window()->state()==WindowState::Empty&&!r.ime.hasWork());
 // Old physical candidate choice must not revive across either mode transition.
 s3hid::Event old{1,++r.now,44,0,true,0,oldframe,0};old.ime_context=context;old.resource_owner=r.ime.resourceOwner();
 r.ime.event(old);old.down=false;++old.ms;r.ime.event(old);r.drain();saved.check(r);
 r.key(0x29);CHECK(!r.ime.rawEditing());r.ready();saved.cancelled(r);
 old.down=true;old.ms=++r.now;r.ime.event(old);old.down=false;old.ms=++r.now;r.ime.event(old);r.drain();r.ready();saved.cancelled(r);
 for(unsigned i=0;i<6;++i){r.key(0x29);r.ready();saved.cancelled(r);}
 CHECK(r.accepted==0);
}
void busy(bool py){LongRig r("ic1_busy");r.start(py,py?"ni":"wo");r.letters(py?"hao":"wo");CHECK(r.ime.window()->busy());const Saved s(r);r.key(0x29);s.check(r);CHECK(r.ime.rawEditing()&&!r.ime.hasWork());CHECK(r.ime.editTarget().valid());r.key(0x29);r.ready();s.cancelled(r);}
void invalidTail(bool py){LongRig r("ic1_tail");r.start(py,py?"fangengq":"wjdjl");at(r,r.ime.raw().size());CHECK(!r.ime.editTarget().valid());const Saved s(r);r.key(0x51);s.check(r);CHECK(r.ime.rawEditing());r.key(42);r.ready();CHECK(r.ime.editTarget().valid());r.letters(py?"q":"l");r.ready();CHECK(!r.ime.editTarget().valid());}
void splitPy(size_t end){LongRig r("ic1_py");r.start(true,"fangeng");at(r,end);target(r,EditTargetKind::Prefix,0,end);r.key(0x51);r.ready();CHECK(r.ime.editTarget().end==end);r.key(44);r.ready();CHECK(r.ime.composition()->snapshot().focus==end);at(r,7);target(r,EditTargetKind::Prefix,end,7);r.key(0x51);r.ready();CHECK(r.ime.editTarget().begin==end);r.key(30);r.ready();CHECK(r.accepted==1&&r.ime.raw().empty());}
void accepted(bool py){LongRig r("ic1_accepted");r.start(py,py?"ni":"wo");r.key(44);r.ready();CHECK(r.accepted==1);const std::string body(r.app.document.text());for(unsigned i=0;i<4;++i)r.key(0x29);CHECK(r.accepted==1&&std::string_view(r.app.document.text())==body&&r.ime.raw().empty());}
void retryable(){LongRig r("ic1_retry");EditorSink fill(r.app);while(r.app.document.text().size()<65535){CHECK(fill.bind(100,1));CHECK(fill.commit(std::string(std::min(size_t(8192),65535-r.app.document.text().size()),'x')));}r.start(false,"wjdj");r.key(44);r.ready();CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect);const Saved s(r);r.key(0x29);s.check(r);CHECK(r.ime.rawEditing()&&r.ime.receipt().status==hanime::CompositionStatus::Cancelled);r.key(0x29);r.ready();s.cancelled(r);CHECK(r.accepted==0);}
void staleEsc(){
 LongRig r("ic1_stale");r.start(false,"wjdjle");r.enqueue(0x29,true);r.enqueue(0x29,false);
 r.ime.window()->query(2);r.ready();const Saved before(r);const auto layout=r.ime.window()->layoutSerial();
 r.dispatch();before.check(r);CHECK(!r.ime.rawEditing()&&r.ime.window()->layoutSerial()==layout);
 r.key(0x29);before.check(r);CHECK(r.ime.rawEditing());
}
void e1Esc(){
 LongRig r("ic1_e1");r.start(false,"wo");r.key(44);r.ready();const std::string body(r.app.document.text());
 r.letters("wo");r.ready();const auto owner=r.ime.resourceOwner();r.enqueue(0x29,true);r.enqueue(0x29,false);
 r.resources.mount()->available=false;r.ime.service();r.drain();CHECK(r.ime.resourceOwner()!=owner&&!r.ime.chinese());
 r.dispatch();size_t retired=0;for(const auto& e:r.records)if(e.kind==UiTraceKind::Intent&&e.detail==ResourceOwnerRetired)++retired;
 CHECK(retired==1&&std::string_view(r.app.document.text())==body);r.key(4);CHECK(std::string_view(r.app.document.text())==body+"a");
}
void pendingCommit(bool prepared){
 LongRig r("ic1_pending_commit");r.start(false,"wo");r.key(44);
 unsigned n=0;while(r.ime.composition()->snapshot().state!=hanime_extension::CompositionState::AllSelected){r.step();r.drain();CHECK(++n<100000);}
 if(prepared){while(!r.ime.attemptSerial()){r.step();r.drain();CHECK(++n<100000);}CHECK(r.ime.receipt().status==hanime::CompositionStatus::Preparing||r.ime.receipt().status==hanime::CompositionStatus::Ready);}
 const Saved s(r);r.key(0x29);s.check(r);CHECK(r.ime.rawEditing()&&r.accepted==0&&!r.ime.hasWork());
 r.key(0x29);r.ready();s.cancelled(r);CHECK(r.accepted==0);r.letters("wo");r.ready();r.key(44);r.ready();CHECK(r.accepted==1&&r.ime.raw().empty()); // Fresh composition proves continued input after cancel.
}
void zeroReselection(){
 LongRig r("ic1_zero_reselect");r.start(false,"wjdjle");at(r,2);r.key(0x51);r.ready();CHECK(r.ime.reselecting());
 const Saved before(r);const auto layout=r.ime.window()->layoutSerial();
 s3hid::Event e{1,++r.now,0x29,0,true,0,r.frame,r.ime.attemptSerial()};e.ime_context=0;e.resource_owner=r.ime.resourceOwner();
 r.ime.event(e);e.down=false;e.ms=++r.now;r.ime.event(e);r.drain();before.check(r);
 CHECK(r.ime.reselecting()&&!r.ime.rawEditing()&&r.ime.window()->layoutSerial()==layout);
 r.key(0x29);before.check(r);CHECK(r.ime.rawEditing());
}
void noIoMovement(){LongRig r("ic1_no_io");r.start(true,"fangeng");at(r,0);const auto io=metrics.io();for(unsigned n=0;n<4;++n){at(r,3);target(r,EditTargetKind::Prefix,0,3);at(r,4);target(r,EditTargetKind::Prefix,0,4);}const auto delta=metrics.io()-io;CHECK(delta.reads==0&&delta.seeks==0&&delta.bytes==0);}
int main(){unsigned passed=0,failed=0;auto run=[&](const std::string& n,const std::function<void()>& f){try{f();++passed;std::cout<<"PASS "<<n<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<n<<' '<<e.what()<<'\n';}};
 for(bool py:{false,true}){const std::string s=py?"_py":"_xh";
 run("boundary_left_owner"+s,[&]{adjacent(py,false);});run("same_cursor_after_reopen"+s,[&]{adjacent(py,true);});run("geometry_member_suffix_zero_end"+s,[&]{geometry(py);});
 for(unsigned k=0;k<3;++k)run("esc_preserves_"+std::to_string(k)+s,[&]{escapes(py,k);});
 run("esc_busy_query"+s,[&]{busy(py);});run("invalid_tail_edit_recovery"+s,[&]{invalidTail(py);});run("accepted_body_immutable"+s,[&]{accepted(py);});}
 run("py1_fan_geng",[]{splitPy(3);});run("py1_fang_eng",[]{splitPy(4);});run("esc_retryable_no_effect",retryable);
 run("esc_stale_query_context",staleEsc);run("esc_stale_context_e1",e1Esc);run("esc_before_prepare",[]{pendingCommit(false);});run("esc_during_prepare",[]{pendingCommit(true);});run("raw_target_movement_no_io",noIoMovement);run("zero_ingress_reselection_esc",zeroReselection);
 std::cout<<"IC1 cases="<<passed+failed<<" pass="<<passed<<" fail="<<failed<<'\n';return failed?1:0;}
