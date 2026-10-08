#define main inherited_main
#include "host_tests.cpp"
#undef main
void nameSelect(){
 Fixture f;f.start();f.key(4);auto body=f.app.document.text();f.key(59);renameTo(f,"Alpha");auto id=f.app.session.currentId();
 for(uint8_t mod:{uint8_t(1),uint8_t(16),uint8_t(8),uint8_t(128)}){
  f.key(21);auto stale=f.app.nameStamp();f.key(4,mod);CHECK(f.app.renameSelected());CHECK(!f.app.commitName("old",stale));
  f.key(5);CHECK(std::string(f.app.renameText())=="b"&&!f.app.renameSelected());f.key(41);CHECK(metadata(f,id).name=="Alpha");
 }
 f.key(21);f.key(4,1);f.key(80);CHECK(f.app.renameCursor()==0&&!f.app.renameSelected());f.key(4,1);f.key(79);CHECK(f.app.renameCursor()==5);
 f.key(4,1);f.key(42);CHECK(!*f.app.renameText());CHECK(f.app.commitName("Long £ UTF-8",f.app.nameStamp()));f.key(4,1);f.key(76);CHECK(!*f.app.renameText());
 f.key(41);CHECK(f.app.document.text()==body);f.key(21);auto stale=f.app.nameStamp();f.app.disconnected();CHECK(!f.app.commitName("late",stale)&&!*f.app.renameText());
}
void returnsAndDelete(){
 Fixture f;f.start();f.key(4);f.key(5);f.key(4,1);auto body=f.app.document.text();auto cursor=f.app.document.cursorByte();auto selection=f.app.editor.selection();auto id=f.app.session.currentId();
 f.key(59);f.key(23);f.key(41);CHECK(f.app.manager.state().view==ds::FileManagerView::Normal);
 f.key(41);CHECK(!f.app.filesOpen()&&f.app.document.text()==body&&f.app.document.cursorByte()==cursor&&f.app.document.dirty());CHECK(f.app.editor.selection().begin==selection.begin&&f.app.editor.selection().end==selection.end);
 f.key(59);f.key(76);CHECK(f.app.manager.state().trash_id==id&&f.app.manager.state().pending==ds::PendingAction::TrashSelected);f.key(76);CHECK(metadata(f,id).area==ds::DocumentArea::Normal);f.app.disconnected();f.key(40);CHECK(metadata(f,id).area==ds::DocumentArea::Normal);
 f.key(59);f.key(76);f.key(40);f.settle();CHECK(!f.app.session.hasCurrent());f.key(41);CHECK(f.app.manager.state().view==ds::FileManagerView::TrashedReturn);f.key(4);CHECK(f.app.document.text().empty());f.key(41);CHECK(f.app.manager.state().view==ds::FileManagerView::Normal);
 f.key(23);f.key(76);CHECK(f.app.manager.state().pending==ds::PendingAction::None&&metadata(f,id).area==ds::DocumentArea::Trash);
 Fixture empty(f.path);empty.start();CHECK(!empty.app.session.hasCurrent());empty.key(41);CHECK(!empty.app.filesOpen());
}
void restoreNames(){
 Fixture f;f.start();auto a=f.app.session.currentId();CHECK(f.app.session.rename(a,"Work")==ds::Status::Ok);f.key(4);f.save();auto saved=f.app.persistence.loadLatest(a);
 CHECK(f.app.session.moveToTrash(a,++f.now));CHECK(f.app.session.create("Work",++f.now));auto b=f.app.session.currentId();
 CHECK(f.app.session.create("Work",++f.now).status==ds::Status::NameExists);
 CHECK(f.app.session.create("Work-01",++f.now));CHECK(f.app.session.create("Work-02",++f.now));CHECK(f.app.session.open(b,++f.now));f.key(5);auto body=f.app.document.text();auto cursor=f.app.document.cursorByte();
 CHECK(f.app.session.restoreFromTrash(a));CHECK(metadata(f,a).name=="Work-03");CHECK(f.app.session.currentId()==b&&f.app.document.text()==body&&f.app.document.cursorByte()==cursor&&f.app.document.dirty());
 CHECK(f.app.persistence.loadLatest(a).snapshot.text==saved.snapshot.text&&f.app.persistence.loadLatest(a).snapshot.generation==saved.snapshot.generation);
 CHECK(f.app.session.moveToTrash(a,++f.now));CHECK(f.app.session.create("Work-03",++f.now));CHECK(f.app.session.restoreFromTrash(a));CHECK(metadata(f,a).name=="Work-03-01");
 CHECK(f.app.session.moveToTrash(a,++f.now));CHECK(f.app.session.rename(a,"Failure")==ds::Status::Ok);CHECK(f.app.session.create("Failure",++f.now));
 s3fs::faults.write_bytes=0;CHECK(f.app.session.restoreFromTrash(a).status==ds::Status::IoError);s3fs::faults={};CHECK(metadata(f,a).area==ds::DocumentArea::Trash&&metadata(f,a).name=="Failure");
 s3fs::faults.flush=true;CHECK(f.app.session.restoreFromTrash(a).status==ds::Status::Partial);s3fs::faults={};CHECK(metadata(f,a).area==ds::DocumentArea::Normal&&metadata(f,a).name=="Failure-01");
 Fixture reboot(f.path);reboot.start();CHECK(metadata(reboot,a).area==ds::DocumentArea::Normal&&metadata(reboot,a).name=="Failure-01");
 draftor::memory::String longName;for(unsigned i=0;i<32;++i)longName+="字";
 CHECK(f.app.session.rename(a,longName)==ds::Status::Ok);CHECK(f.app.session.moveToTrash(a,++f.now));CHECK(f.app.session.create(longName,++f.now));CHECK(f.app.session.restoreFromTrash(a));auto actual=metadata(f,a).name;CHECK(actual.size()==96&&ds::isValidUtf8(actual)&&actual.substr(actual.size()-3)=="-01");
 Fixture hundred;hundred.start();auto h=hundred.app.session.currentId();CHECK(hundred.app.session.rename(h,"Base")==ds::Status::Ok);CHECK(hundred.app.session.moveToTrash(h,++hundred.now));CHECK(hundred.app.session.create("Base",++hundred.now));
 for(unsigned i=1;i<=100;++i){char name[20];std::snprintf(name,sizeof(name),"Base-%02u",i);CHECK(hundred.app.session.create(name,++hundred.now));}
 CHECK(hundred.app.session.restoreFromTrash(h));CHECK(metadata(hundred,h).name=="Base-101");
}
int main(){try{nameSelect();returnsAndDelete();restoreNames();std::puts("PASS v0.5.3 menus: four primary modifiers/select/collapse/delete/stale stamps, Esc states, stable Delete confirm/disconnect, normal-only uniqueness, restore suffix through 101, UTF8 truncation, repeat restore, failure/Partial/restart, body/version unchanged");return 0;}catch(const std::exception&e){s3fs::faults={};std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
