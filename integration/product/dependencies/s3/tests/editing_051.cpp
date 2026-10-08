#define main inherited_main
#include "host_tests.cpp"
#undef main
void nameEditing(){
 s3app::NameInput n;CHECK(n.assign("a中🙂z"));CHECK(n.cursor()==9);CHECK(n.left()&&n.cursor()==8);CHECK(n.backspace()&&std::string(n.text())=="a中z"&&n.cursor()==4);CHECK(n.left()&&n.cursor()==1);CHECK(n.deleteForward()&&std::string(n.text())=="az");CHECK(n.insert("£")&&std::string(n.text())=="a£z");CHECK(!n.insert("\xff"));
 Fixture f;f.start();f.key(4);auto body=f.app.document.text();f.key(59);f.key(21);auto stale=f.app.nameStamp();f.key(80);CHECK(!f.app.commitName("late",stale));CHECK(f.app.commitName("X",f.app.nameStamp()));f.key(76);CHECK(f.app.document.text()==body);f.key(41);CHECK(!f.app.renameOpen());
 // The single repeat engine runs both input owners. Delayed service emits one.
 for(uint8_t usage:{uint8_t(42),uint8_t(76),uint8_t(80),uint8_t(79)}){
  s3hid::Repeat r;s3hid::Event e;r.context(1,true);r.event({1,100,usage,0,true});CHECK(!r.poll(449,1,e));CHECK(r.poll(450,1,e)&&e.usage==usage);CHECK(!r.poll(450,1,e));CHECK(r.poll(2000,1,e));CHECK(!r.poll(2000,1,e));r.event({1,2001,usage,0,false});CHECK(!r.poll(9000,1,e));
  r.event({1,10000,usage,0,true});r.context(2,true);CHECK(!r.poll(11000,1,e));r.event({1,12000,usage,0,true});CHECK(!r.poll(13000,2,e));
 }
 // Actual body and name transactions, including undo after repeated deletion.
 f.key(41);CHECK(f.app.commitText("bcdef",f.app.editStamp()));s3hid::Repeat repeat;repeat.context(f.app.focusToken(),true);
 auto press=s3hid::Event{1,++f.now,42,0,true};repeat.event(press);f.app.key(press);s3hid::Event emitted;CHECK(repeat.poll(f.now+350,1,emitted));f.app.key(emitted);CHECK(f.app.document.text()=="abcd");f.key(29,1);CHECK(f.app.document.text()=="abcde");
 f.key(74,1); // original full-document Home
 f.key(76);CHECK(f.app.document.text()=="bcde");
 repeat.clear();repeat.context(f.app.focusToken(),true);repeat.event({1,f.now,76,0,true});CHECK(repeat.poll(f.now+350,1,emitted));f.app.key(emitted);CHECK(f.app.document.text()=="cde");f.key(29,1);CHECK(f.app.document.text()=="bcde");
 f.key(59);f.key(21);auto previous=std::string(f.app.renameText());f.key(80);f.key(42);CHECK(std::string(f.app.renameText()).size()+1==previous.size());repeat.clear();repeat.context(f.app.focusToken(),true);repeat.event({1,++f.now,80,0,true});auto nameRevision=f.app.renameRevision();CHECK(repeat.poll(f.now+350,1,emitted));f.app.key(emitted);CHECK(f.app.renameRevision()!=nameRevision);f.app.disconnected();repeat.context(f.app.focusToken(),false);CHECK(!repeat.poll(f.now+1000,1,emitted));CHECK(!f.app.renameOpen()&&!*f.app.renameText());
}
void shortcutsAndNames(){
 Fixture f;f.start();for(uint8_t mods:{uint8_t(1),uint8_t(16),uint8_t(8),uint8_t(128)}){auto id=f.app.session.currentId();f.key(17,mods);f.settle();CHECK(f.app.session.currentId()!=id&&f.app.document.text().empty());}
 auto a=f.app.session.currentId();f.key(4);f.key(17,1);CHECK(f.app.commandBarrier());f.key(17,1);f.settle();auto b=f.app.session.currentId();CHECK(a!=b&&f.app.persistence.loadLatest(a).snapshot.text=="a");
 f.key(5);f.key(17,8);s3fs::faults.write_bytes=0;f.settle();s3fs::faults={};CHECK(f.app.session.currentId()==b&&f.app.document.text()=="b"&&f.app.document.dirty());
 f.key(9,1);auto q=f.app.queryStamp();CHECK(f.app.commitQuery("b",q));f.key(12,1);auto cs=f.app.find.view().case_sensitive;f.key(17,1);CHECK(f.app.session.currentId()==b);f.key(9,8);CHECK(!f.app.findOpen());f.key(9,1);CHECK(f.app.findOpen()&&f.app.find.view().query=="b"&&f.app.find.view().case_sensitive==cs);f.key(41);
 f.key(60);f.key(17,1);CHECK(f.app.session.currentId()==b);f.key(41);f.key(59);f.key(21);f.key(17,1);CHECK(f.app.renameOpen());f.key(41);f.key(7);f.key(17,1);CHECK(f.app.manager.state().pending==ds::PendingAction::TrashSelected);f.key(41);
 renameTo(f,"TF Test");CHECK(!f.app.renameOpen());f.key(17);f.settle();auto c=f.app.session.currentId();f.key(59);f.key(21);while(*f.app.renameText())f.key(42);CHECK(f.app.commitName("TF Test",f.app.nameStamp()));auto cursor=f.app.renameCursor();f.key(40);CHECK(f.app.lastOperation.status==ds::Status::NameExists&&f.app.renameCursor()==cursor&&std::string(f.app.renameText())=="TF Test");CHECK(metadata(f,c).name!="TF Test");f.key(41);
 CHECK(f.app.session.moveToTrash(b,++f.now));renameTo(f,"TF Test");CHECK(f.app.lastOperation.status==ds::Status::Ok&&metadata(f,c).name=="TF Test");
 // Legacy duplicate fixtures remain readable, and same-name no-op remains legal.
 CHECK(f.app.session.create("Legacy",++f.now));auto legacy1=f.app.session.currentId();CHECK(f.app.session.create("Legacy second",++f.now));
 auto legacy=metadata(f,f.app.session.currentId());legacy.name="Legacy";
 ds::RecordStore records(f.storage);CHECK(records.commitPair(ds::paths::metadata(legacy.id),ds::RecordKind::Metadata,legacy.metadata_generation+1,ds::encodeMetadata(legacy))==ds::Status::Ok);f.app.catalog.invalidateCache();
 CHECK(f.app.session.rename(f.app.session.currentId(),"Legacy")==ds::Status::Ok);CHECK(f.app.session.rename(legacy1,"Legacy")==ds::Status::Ok);
}
int main(){try{nameEditing();shortcutsAndNames();std::puts("PASS v0.5.1 editing: UTF8 cursor/delete/repeat/focus, shortcuts, dirty failure, exact duplicate policy, Find toggle");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
