#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
void firstLeft(){
 Rig r;r.begin(false,"dzbbvidcui",false);
 r.ime.window()->query(2);r.settle();r.tap(44);r.settle();
 CHECK(r.ime.selected()=="都");r.tap(0x50);
 std::cout<<"FIRST_LEFT cursor="<<r.ime.rawCursor()<<" expected=4\n";
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==4);
 r.tap(0x2a);r.type("u");r.settle();
 CHECK(r.ime.raw()=="dzbuvidcui"&&r.ime.selected()=="都");
 CHECK(r.ime.candidateAvailable());
 const auto selected=std::string(r.ime.selected());const auto io=metrics.io();
 for(unsigned i=0;i<20;++i){r.ime.selectedViewport(r.font);}
 CHECK(r.ime.selected()==selected&&(metrics.io()-io).bytes==0);
 r.tap(44);r.settle();CHECK(r.ime.selected().substr(0,6)=="都不");
}
void invalidDown(){
 Rig r;r.begin(false,"bu",false);r.tap(0x50);r.tap(0x4f);r.tap(0x2a);r.type("u");r.tap(0x2a);r.settle(); // b at cursor1
 r.type("u");r.settle();r.tap(0x2a);r.type("u"); // caret2; create caret1 via Delete/reinsert
 r.tap(0x50);r.tap(0x4c);r.type("b");r.settle();
 CHECK(r.ime.raw()=="bu"&&r.ime.rawCursor()==1&&r.ime.candidateAvailable());
 const auto query=r.ime.window()->querySerial();const auto frame=digest(*r.ime.view());
 r.tap(0x51);r.settle();
 std::cout<<"INVALID_DOWN editing="<<r.ime.rawEditing()<<" candidates="<<r.ime.candidateAvailable()<<'\n';
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==1&&r.ime.candidateAvailable());
 CHECK(r.ime.window()->querySerial()==query&&digest(*r.ime.view())==frame);
 r.tap(44);r.settle();CHECK(r.app.document.text()=="不");
}
void manualFailure(bool pending,uint8_t key){
 Rig r;r.begin(true,"bu",false);r.tap(0x50);r.tap(0x4f);r.tap(0x50);
 // Replace the first scalar without changing final raw; cursor lies inside bu.
 r.tap(0x4c);r.type("b");if(!pending)r.settle();
 CHECK(r.ime.raw()=="bu"&&r.ime.rawCursor()==1);
 const auto query=r.ime.window()->querySerial();r.tap(0x51);
 r.settle(); // Invalid endpoint never started a manual operation.
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==1&&r.ime.raw()=="bu");
 CHECK(r.ime.candidateAvailable()&&r.ime.window()->querySerial()==query);
 const auto recovered=r.ime.window()->querySerial();
 CHECK(!r.ime.editTarget().valid());
 for(int i=0;i<30;++i){r.step();}CHECK(r.ime.window()->querySerial()==recovered);
 r.tap(key);r.settle();CHECK(r.app.document.text()=="不");
 std::cout<<"PASS incomplete endpoint leaves automatic query unchanged pending="<<pending<<" key="<<unsigned(key)<<'\n';
}
void continuation(bool py,bool incomplete,bool waiting){
 Rig r;r.begin(py,py?(incomplete?"doub":"doubuzhidaoshi"):(incomplete?"dzb":"dzbbvidcui"),false);
 r.ime.window()->query(py?3:2);r.settle();r.tap(44);
 if(!waiting)r.settle();else {for(unsigned n=0;n<2000000&&r.ime.selected().empty();++n)r.step();}
 CHECK(r.ime.selected()=="都");
 const auto focus=r.ime.composition()->snapshot().focus;
 hanime_extension::SyllableNavigator navigator;const auto nav=navigator.analyze(*r.ime.composition());
 auto expected=navigator.neighbor(nav.handle,*r.ime.composition(),focus,hanime_extension::NavigationDirection::Right);
 const auto ends=navigator.prefix_endpoints(nav.handle,*r.ime.composition());if(ends.count)expected.offset=ends.ends[0];
 r.tap(0x50);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==expected.offset);
 CHECK(expected.offset>focus);if(!incomplete)CHECK(expected.offset<r.ime.raw().size());
 r.tap(0x2a);r.type("u");r.settle();CHECK(r.ime.selected()=="都");
 std::cout<<"PASS continuation py="<<py<<" incomplete="<<incomplete<<" pending="<<waiting<<" cursor="<<expected.offset<<'\n';
}
void selectedRecovery(bool py,bool immediate,uint8_t key){
 Rig r;r.begin(py,py?"doubu":"dzbu",false);
 r.ime.window()->query(py?3:2);r.settle();r.tap(44);r.settle();CHECK(r.ime.selected()=="都");
 r.tap(0x50);r.tap(0x50);r.tap(0x4c);r.type("b");r.settle();
 CHECK(r.ime.rawCursor()==(py?4u:3u)&&r.ime.selected()=="都");
 const auto query=r.ime.window()->querySerial();r.tap(0x51);
 CHECK(r.ime.rawEditing()&&r.ime.window()->querySerial()==query&&!r.ime.editTarget().valid());
 if(!immediate)r.settle();
 CHECK(r.ime.rawEditing()&&r.ime.selected()=="都");
 r.tap(key);r.settle();CHECK(r.app.document.text()=="都不"&&r.ime.raw().empty());
 std::cout<<"PASS incomplete suffix preserves selected prefix py="<<py<<" immediate="<<immediate<<" key="<<unsigned(key)<<'\n';
}
void pendingChoice(){
 for(uint8_t key:{uint8_t(44),uint8_t(30)}){
  Rig r;r.begin(false,"bu",false);r.tap(0x50);r.tap(0x4f);r.tap(0x50);r.tap(0x4c);r.type("b");
  CHECK(r.ime.window()->busy());r.tap(0x51);r.tap(key);
  CHECK(r.ime.rawEditing()&&r.ime.selected().empty()&&r.app.document.text().empty());
  r.settle();CHECK(r.ime.rawEditing()&&r.ime.raw()=="bu"&&r.app.document.text().empty());
  r.tap(key);r.settle();CHECK(r.app.document.text()=="不");
 }
 std::cout<<"PASS unadmitted manual range never selects old automatic window or replays choice\n";
}
int main(int argc,char** argv){try{
 if(argc>1&&std::string(argv[1])=="invalid")invalidDown();
 else {pendingChoice();for(bool py:{false,true})for(bool immediate:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)})selectedRecovery(py,immediate,key);for(bool py:{false,true})for(bool incomplete:{false,true})for(bool waiting:{false,true})continuation(py,incomplete,waiting);firstLeft();invalidDown();for(bool pending:{false,true})for(uint8_t key:{uint8_t(44),uint8_t(30)})manualFailure(pending,key);}
 std::cout<<"PASS current edit\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
