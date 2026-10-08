#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
s3hid::Event choiceEvent(Rig& r,uint8_t key){
 s3hid::Event e{1,++r.now,key,0,true,0,r.frame,r.ime.attemptSerial()};e.ime_context=r.ime.ingressContext();e.resource_owner=r.ime.resourceOwner();return e;
}
void sendChoice(Rig& r,s3hid::Event e){r.ime.event(e);e.down=false;e.ms=++r.now;r.ime.event(e);}
void readyWithoutLcd(Rig& r){unsigned n=0;while(r.ime.hasWork()){r.step();CHECK(++n<2000000);}}
void earlyReselection(bool py,uint8_t key,bool repaint){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);
 r.tap(0x50);r.tap(0x50);const auto end=py?3u:2u;CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==end);
 r.tap(0x51);CHECK(!r.ime.rawEditing());const auto query=r.ime.window()->querySerial();
 const auto e=choiceEvent(r,key);
 if(repaint){readyWithoutLcd(r);r.frame=r.ime.framePresented();}
 sendChoice(r,e);r.settle();
 CHECK(!r.ime.selected().empty()&&r.app.document.text().empty());
 CHECK(r.ime.composition()->snapshot().focus==end&&r.ime.window()->querySerial()>query);
 for(unsigned i=0;i<3&&!r.ime.raw().empty();++i){r.tap(key==44?30:44);r.settle();}CHECK(!r.app.document.text().empty()&&r.ime.raw().empty());
 std::cout<<"RESELECT early py="<<py<<" key="<<unsigned(key)<<" repaint="<<repaint<<" PASS\n";
}
void editedRange(bool py,unsigned replacement){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);r.tap(0x50);r.tap(0x50);r.tap(0x51);
 const auto old=choiceEvent(r,44);const auto oldOp=r.ime.composition()->operation();
 if(replacement==0)r.ime.window()->query(py?6:4);
 else if(replacement==1){r.ime.window()->request(1,0,true);}
 else if(replacement==2){r.tap(0x29);r.type("wo");}
 else if(replacement==3){r.type("a");}
 else if(replacement==4){r.app.commitText("x",r.app.editStamp());}
 else if(replacement==5){r.tap(0x2c,1);}
 else {++r.resources.mount()->epoch;}
 sendChoice(r,old);if(replacement!=6)r.settle();
 CHECK(r.app.document.text()==(replacement==4?"x":""));
 if(replacement==0){
  CHECK(r.ime.window()->state()==WindowState::Ready);const auto current=r.ime.composition()->operation();
  CHECK(r.ime.composition()->cancel(oldOp)==hanime_extension::Status::Stale);(void)current;
  CHECK(r.ime.composition()->page().end==(py?6u:4u));
 }
 std::cout<<"RESELECT stale py="<<py<<" replacement="<<replacement<<" PASS\n";
}
void existingBoundary(bool py){
 Rig r;r.begin(py,py?"wojintian":"wojbtm",false);r.tap(44);r.settle();
 CHECK(r.ime.selected()=="我");r.tap(0x50);r.tap(0x50);CHECK(r.ime.rawCursor()==2);
 const auto before=r.ime.composition()->snapshot();const auto member=before.members[0];
 const auto target=r.ime.editTarget();CHECK(target.kind==EditTargetKind::Member&&target.begin==0&&target.end==2);
 const auto query=r.ime.window()->querySerial();r.tap(0x51);
 CHECK(!r.ime.rawEditing()&&r.ime.window()->querySerial()>query&&r.ime.reselecting());
 auto reopened=r.ime.composition()->snapshot();CHECK(reopened.members[0].key==member.key&&!reopened.members[0].valid);
 r.settle();CHECK(r.ime.composition()->page().begin==0&&r.ime.composition()->page().end==2);
 r.tap(0x29);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==2);
 reopened=r.ime.composition()->snapshot();CHECK(reopened.members[0].key==member.key&&!reopened.members[0].valid);
 r.tap(0x51);r.settle();r.tap(30);r.settle();r.tap(44);r.settle();CHECK(r.app.document.text()=="我今天");
 std::cout<<"CONFIRMED boundary py="<<py<<" PASS\n";
}
void escapeReturn(bool py,unsigned timing){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);
 r.tap(0x50);r.tap(0x50);const auto cursor=r.ime.rawCursor();const auto raw=std::string(r.ime.raw());r.tap(0x51);
 if(timing==1)readyWithoutLcd(r);
 if(timing==2)r.settle();
 if(timing==3){r.tap(44);r.type("a");} // Early choice and successor, neither may survive Esc.
 if(timing==4){r.settle();r.tap(0x4f);r.settle();}
 if(timing==5){r.settle();r.tap(44);} // Captured choice has not been published to members.
 const auto escape=choiceEvent(r,0x29);sendChoice(r,escape);
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==cursor&&r.ime.raw()==raw&&!r.ime.reselecting());
 r.settle();CHECK(r.ime.raw()==raw&&r.ime.selected().empty()&&r.app.document.text().empty());
 // Duplicate and release of this Esc cannot leave raw mode or cancel input.
 sendChoice(r,escape);CHECK(r.ime.rawEditing()&&r.ime.raw()==raw);
 r.tap(0x50);CHECK(r.ime.rawCursor()==0);r.tap(0x51);CHECK(r.ime.rawEditing()); // no query(0)
 r.tap(0x4f);CHECK(r.ime.rawCursor()==cursor);
 r.tap(0x28);CHECK(r.ime.rawEditing()&&r.ime.raw()==raw); // Enter unchanged.
 r.tap(0x51);r.settle();r.tap(30);r.settle();
 for(unsigned i=0;i<3&&!r.ime.raw().empty();++i){r.tap(44);r.settle();}
 CHECK(!r.app.document.text().empty()&&r.ime.raw().empty());
 const auto body=std::string(r.app.document.text());r.tap(0x29);CHECK(r.app.document.text()==std::string_view(body));
 r.type(py?"shi":"ui");r.settle();r.tap(0x29);CHECK(r.ime.raw().empty()); // ordinary Esc cancels.
 std::cout<<"CONFIRMED escape py="<<py<<" timing="<<timing<<" PASS\n";
}
void staleEscape(bool py,bool afterLcd){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);r.tap(0x50);r.tap(0x50);r.tap(0x51);
 const auto escape=choiceEvent(r,0x29);const auto old=r.ime.composition()->operation();
 r.ime.window()->query(py?6:4);const auto query=r.ime.window()->querySerial();
 if(afterLcd)r.settle();
 sendChoice(r,escape);
 CHECK(r.ime.window()->querySerial()==query&&!r.ime.rawEditing()&&!r.ime.raw().empty());
 CHECK(r.ime.composition()->cancel(old)==hanime_extension::Status::Stale);
 r.settle();CHECK(r.ime.composition()->page().end==(py?6u:4u)&&r.ime.window()->state()==WindowState::Ready);
 // E1: rejecting the old event must not leave current input uncancellable.
 r.tap(0x29);CHECK(!r.ime.active()&&!r.ime.rawEditing()&&r.ime.raw().empty());
 r.type(py?"shi":"ui");const auto replacement=r.ime.window()->querySerial();
 CHECK(r.ime.composition()->cancel(old)==hanime_extension::Status::Stale);
 CHECK(r.ime.composition()->tick(old,Resources::Budget).status==hanime_extension::Status::Stale);
 r.settle();CHECK(r.ime.window()->querySerial()==replacement&&r.ime.window()->state()==WindowState::Ready);
 r.tap(44);r.settle();CHECK(r.app.document.text()=="是");
 std::cout<<"CONFIRMED stale Esc py="<<py<<" lcd="<<afterLcd<<" PASS\n";
}
void editAfterReturn(bool py,bool eraseRight){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);
 const size_t end=py?6:4;
 r.ime.window()->query(end);r.settle();CHECK(r.ime.window()->choose(0,r.frame)==ChoiceResult::Captured);r.settle();
 r.ime.window()->query(0);r.settle();CHECK(r.ime.window()->choose(0,r.frame)==ChoiceResult::Captured);r.settle();
 const auto before=r.ime.composition()->snapshot();CHECK(before.member_count==2);
 const auto first=before.members[0],last=before.members[1];const auto raw=std::string(r.ime.raw());
 r.tap(0x50);r.tap(0x50);const auto cursor=r.ime.rawCursor();r.tap(0x51);r.settle();r.tap(0x29);
 const auto returned=r.ime.composition()->snapshot();CHECK(returned.member_count==2);
 CHECK(returned.members[0].key==first.key&&returned.members[0].ref==first.ref&&!returned.members[0].valid);
 CHECK(returned.members[1].key==last.key&&returned.members[1].ref==last.ref&&!returned.members[1].valid);
 CHECK(returned.members[1].begin==last.begin&&returned.members[1].end==last.end);
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==cursor&&r.ime.raw()==raw);
 const auto index=eraseRight?cursor:cursor-1;r.tap(eraseRight?0x4c:0x2a);
 auto expected=raw;expected.erase(index,1);CHECK(r.ime.raw()==expected);
 r.type(std::string_view(raw.data()+index,1));CHECK(r.ime.raw()==raw);r.settle();
 // Syllable navigation still reaches both raw endpoints and stops there.
 for(unsigned i=0;i<6;++i)r.tap(0x4f);
 CHECK(r.ime.rawCursor()==raw.size());
 for(unsigned i=0;i<6;++i)r.tap(0x50);
 CHECK(r.ime.rawCursor()==0);
 const auto query=r.ime.window()->querySerial();r.tap(0x51);CHECK(r.ime.rawEditing()&&r.ime.window()->querySerial()==query);
 r.tap(0x4f);r.tap(0x51);r.settle();
 for(unsigned i=0;i<4&&!r.ime.raw().empty();++i){r.tap(i%2?30:44);r.settle();}
 CHECK(!r.app.document.text().empty()&&r.ime.raw().empty());
 std::cout<<"CONFIRMED edit after return py="<<py<<" delete_right="<<eraseRight<<" PASS\n";
}
void retainedMembers(bool py,bool adjacent){
 Rig r;r.begin(py,py?"wandanle":"wjdjle",false);
 const size_t first=py?3:2,second=py?6:4,last=py?8:6;
 const std::vector<size_t> ends=adjacent?std::vector<size_t>{first,second,last}:std::vector<size_t>{second,last};
 for(const auto end:ends){
  r.ime.window()->query(end);r.settle();CHECK(r.ime.window()->choose(0,r.frame)==ChoiceResult::Captured);r.settle();
 }
 const auto old=r.ime.composition()->snapshot();CHECK(old.member_count==ends.size());
 const auto a=old.members[0],b=old.members[1];const auto text=std::string(old.selected_text);
 CHECK(r.app.document.text().empty());r.tap(0x50);r.tap(0x50);CHECK(r.ime.rawCursor()==first);
 const auto target=r.ime.editTarget();CHECK(target.kind==EditTargetKind::Member);
 CHECK(target.begin==(adjacent?first:0)&&target.end==(adjacent?second:first));
 r.tap(0x51);const auto reopened=r.ime.composition()->snapshot();
 CHECK(reopened.member_count==old.member_count&&reopened.members[0].key==a.key&&reopened.members[1].key==b.key);
 CHECK(reopened.members[0].valid==adjacent&&!reopened.members[1].valid&&r.app.document.text().empty());
 if(adjacent)CHECK(!reopened.members[2].valid);
 r.settle();CHECK(r.ime.composition()->page().begin==target.begin&&r.ime.composition()->page().end==target.end);
 if(adjacent){r.tap(30);r.settle();CHECK(r.app.document.text()==std::string_view(text)&&r.ime.raw().empty());}
 std::cout<<"RESELECT retained members py="<<py<<" adjacent="<<adjacent<<" PASS\n";
}
int main(int argc,char** argv){try{
 if(argc>1){if(std::string_view(argv[1])=="boundary")existingBoundary(false);else escapeReturn(false,0);return 0;}
 for(bool py:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)})for(bool repaint:{false,true})earlyReselection(py,key,repaint);
 for(bool py:{false,true}){for(bool right:{false,true})editAfterReturn(py,right);for(unsigned t=0;t<6;++t)escapeReturn(py,t);for(bool lcd:{false,true})staleEscape(py,lcd);retainedMembers(py,false);retainedMembers(py,true);existingBoundary(py);for(unsigned i=0;i<7;++i)editedRange(py,i);}
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
