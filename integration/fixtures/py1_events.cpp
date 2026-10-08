#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
#include <set>
void cursorTo(LongRig& r,size_t end){
 if(!r.ime.rawEditing())r.key(0x50);
 unsigned n=0;
 while(r.ime.rawCursor()!=end){const auto before=r.ime.rawCursor();r.key(before<end?0x4f:0x50);CHECK(r.ime.rawCursor()!=before);CHECK(++n<130);}
 const auto target=r.ime.editTarget();
 CHECK(target.valid()&&target.end==end);
 const auto view=r.ime.editViewport(r.font);
 CHECK(view.cursor_x==view.at(end));
 CHECK(view.at(target.end)>view.at(target.begin));
}
VisibleItem manual(LongRig& r,size_t end){
 cursorTo(r,end);const auto before=r.ime.editTarget();
 r.key(0x51);r.ready();
 CHECK(r.ime.window()->state()==WindowState::Ready&&r.ime.reselecting());
 const auto target=r.ime.editTarget();CHECK(target.begin==before.begin&&target.end==end);
 const auto item=r.ime.view()->rows[0].items[0];
 CHECK(item.begin==target.begin&&item.end==target.end&&r.ime.targetMatches(item));
 return item;
}
void split(const std::string& raw,size_t first,uint8_t choice){
 LongRig r("py1_events_"+raw+std::to_string(first));r.start(true,raw);
 const auto a=manual(r,first);std::string body(a.text.data(),a.bytes);
 r.key(choice);r.ready();const auto s=r.ime.composition()->snapshot();
 CHECK(s.focus==first&&s.member_count==1&&s.members[0].begin==0&&s.members[0].end==first);
 CHECK(s.members[0].key==a.key&&s.members[0].ref==a.ref&&r.accepted==0);
 const auto b=manual(r,raw.size());CHECK(b.begin==first);
 body.append(b.text.data(),b.bytes);r.key(choice==44?30:44);r.ready();
 CHECK(r.accepted==1&&r.ime.raw().empty()&&std::string_view(r.app.document.text())==body);
 std::cout<<"PASS PY1 split raw="<<raw<<" first="<<first<<" second="<<first<<':'<<raw.size()<<" accepted=1 identity_span=1\n";
}
void controls(){
 {LongRig r("geng");r.start(true,"geng");const auto a=manual(r,4);r.key(44);r.ready();CHECK(r.accepted==1&&std::string_view(r.app.document.text())==std::string_view(a.text.data(),a.bytes));}
 {LongRig r("ng_edit");r.start(true,"fangeng");cursorTo(r,7);r.key(42);r.ready();CHECK(r.ime.raw()=="fangen");r.letters("g");r.ready();CHECK(r.ime.raw()=="fangeng");const auto a=manual(r,3);CHECK(a.begin==0&&a.end==3);r.key(44);r.ready();CHECK(r.ime.composition()->snapshot().focus==3);}
 {LongRig r("incomplete");r.start(true,"fangengq");cursorTo(r,8);CHECK(r.ime.rawCursor()==8);r.key(0x51);r.ready();std::cout<<"CONTROL incomplete state="<<int(r.ime.window()->state())<<" notice="<<r.ime.notice()<<" members="<<r.ime.composition()->snapshot().member_count<<'\n';CHECK(r.accepted==0&&r.ime.raw()=="fangengq");}
 {LongRig r("old_screen_current_manual_intent");r.start(true,"fangeng");const auto old_selection=r.ime.composition()->selection(0);
  cursorTo(r,3);r.key(0x51);CHECK(r.ime.window()->busy());
  // Next physical choice is received while the Down query is still unfinished.
  // The physical LCD frame is still the old auto range; the intent is bound to
  // the new explicit manual range and cannot reuse the old Selection.
  r.key(44);r.ready();const auto s=r.ime.composition()->snapshot();
  CHECK(s.member_count==1&&s.focus==3&&s.members[0].begin==0&&s.members[0].end==3&&r.accepted==0);
  CHECK(r.ime.composition()->select(old_selection)==hanime_extension::Status::Stale);
  std::cout<<"PASS PY1 overlapping_down_space old_screen_new_manual_intent consume=0:3 old_selection_stale\n";
 }
 // Last one/two groups after confirmed prefix use the same public suffix endpoints.
 split("fangenggeng",3,44);split("genggeng",4,30);
 std::cout<<"PASS PY1 controls geng ng_delete_restore incomplete suffix_groups\n";
}
#ifndef PY1_EVENTS_LIBRARY
int main(){try{for(const std::string raw:{"fangeng","lingeng","mingeng","tangeng"})for(size_t end:{3u,4u})split(raw,end,end==3?44:30);controls();return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

#endif
