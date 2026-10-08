// Product-level menu tests on real S3 filesystem adapter and document codec.
#include "src/hanime_document/paths.hpp"
static void renameTo(Fixture& f,std::string_view name){
 f.key(21);CHECK(f.app.renameOpen());while(*f.app.renameText())f.key(42);
 CHECK(f.app.commitName(name,f.app.focusToken()));f.key(40);
}
static ds::DocumentMetadata metadata(Fixture& f,ds::DocumentId id){ds::DocumentMetadata m;auto s=f.app.catalog.get(id,&m);CHECK(s==ds::Status::Ok||s==ds::Status::OkDegraded);return m;}
void docNames(){
 Fixture f;f.start();auto id=f.app.session.currentId();f.key(4);f.key(5);auto text=f.app.document.text();auto cursor=f.app.document.cursorByte();
 f.key(59);renameTo(f,"Same title");CHECK(!f.app.renameOpen()&&f.app.currentName=="Same title");CHECK(f.app.document.text()==text&&f.app.document.cursorByte()==cursor&&f.app.document.dirty());
 f.key(21);auto old=f.app.focusToken();f.key(6);f.key(41);CHECK(metadata(f,id).name=="Same title");CHECK(!f.app.commitName("stale",old));
 f.key(21);while(*f.app.renameText())f.key(42);f.key(40);CHECK(f.app.renameOpen()&&f.app.lastOperation.status==ds::Status::InvalidName);
 CHECK(f.app.commitName("bad/name",f.app.focusToken()));f.key(40);CHECK(f.app.renameOpen());f.key(41);
 f.key(21);while(*f.app.renameText())f.key(42);CHECK(!f.app.commitName(draftor::memory::String(97,'a'),f.app.focusToken()));CHECK(!f.app.commitName("\xff",f.app.focusToken()));CHECK(f.app.commitName(draftor::memory::String(96,'a'),f.app.focusToken()));f.key(40);CHECK(metadata(f,id).name.size()==96);
 renameTo(f,"Same title");f.key(41);f.key(29,1);CHECK(f.app.document.text()=="a"); // original undo history
 f.save();CHECK(f.app.session.create("Other",++f.now));auto other=f.app.session.currentId();f.key(59);renameTo(f,"Same title");CHECK(f.app.renameOpen()&&f.app.lastOperation.status==ds::Status::NameExists&&metadata(f,other).name=="Other");f.key(41);renameTo(f,"Other renamed");CHECK(metadata(f,id).name!=metadata(f,other).name&&id.shortHex()!=other.shortHex());
 Fixture reboot(f.path);reboot.start();CHECK(reboot.app.currentName=="Other renamed");CHECK(reboot.app.session.currentId()==other);
}
void docTrash(){
 Fixture f;f.start();auto a=f.app.session.currentId();f.key(4);f.save();CHECK(f.app.session.create("B",++f.now));auto b=f.app.session.currentId();f.key(5);auto text=f.app.document.text();auto cursor=f.app.document.cursorByte();
 f.key(9,1);query(f,"b");f.key(41);f.key(60);f.key(41);
 f.key(59);selectId(f,a);f.key(7);CHECK(f.app.manager.state().trash_id==a);f.key(41);CHECK(metadata(f,a).area==ds::DocumentArea::Normal);
 f.key(7);f.key(40);CHECK(metadata(f,a).area==ds::DocumentArea::Trash);CHECK(f.app.session.currentId()==b&&f.app.document.text()==text&&f.app.document.cursorByte()==cursor&&f.app.document.dirty());
 f.key(23);selectId(f,a);f.key(40);CHECK(f.app.filesOpen()&&f.app.session.currentId()==b&&f.app.document.text()==text&&f.app.document.dirty());CHECK(metadata(f,a).area==ds::DocumentArea::Normal);
 f.key(23);selectId(f,b);f.key(7);f.key(40);CHECK(f.app.commandBarrier());CHECK(!f.app.commitText("late",f.app.editStamp()));f.settle();CHECK(!f.app.session.hasCurrent()&&f.app.filesOpen());CHECK(f.app.document.text().empty());CHECK(f.app.find.view().query.empty()&&!f.app.editor.selection().has_selection&&!f.app.statsOpen());
 f.key(41);f.key(4);CHECK(f.app.document.text().empty());f.key(22,1);CHECK(f.app.lastOperation.status==ds::Status::NoCurrentDocument);
 f.key(59);f.key(23);selectId(f,b);f.key(40);CHECK(!f.app.session.hasCurrent());f.key(23);selectId(f,b);f.key(40);CHECK(f.app.session.currentId()==b&&f.app.document.text()==text&&f.app.document.cursorByte()==cursor);
 CHECK(f.app.persistence.loadLane(b,ds::SaveLane::Auto).snapshot.text==text);
}
void docFailures(){
 Fixture f;f.start();auto id=f.app.session.currentId();f.key(4);f.key(59);f.key(7);f.key(40);s3fs::faults.write_bytes=0;f.settle();s3fs::faults={};CHECK(f.app.session.currentId()==id&&f.app.document.text()=="a"&&f.app.document.dirty());CHECK(metadata(f,id).area==ds::DocumentArea::Normal);
 f.app.manager.cancelConfirmation();f.key(41);f.save();f.key(59);
 s3fs::faults.write_bytes=0;renameTo(f,"Failure");s3fs::faults={};CHECK(f.app.renameOpen()&&metadata(f,id).name!="Failure");Fixture degraded(f.path);degraded.start();CHECK(degraded.app.currentName==metadata(f,id).name);f.key(41);
 f.key(7);s3fs::faults.write_bytes=0;f.key(40);s3fs::faults={};CHECK(f.app.session.hasCurrent()&&metadata(f,id).area==ds::DocumentArea::Normal);
 // Full metadata bytes are written, but CURRENT cannot be written: Partial.
 auto m=metadata(f,id);m.area=ds::DocumentArea::Trash;
 s3fs::faults.write_bytes=ds::RecordStore::encode(ds::RecordKind::Metadata,m.metadata_generation+1,ds::encodeMetadata(m)).size();
 f.key(7);f.key(40);s3fs::faults={};CHECK(f.app.lastOperation.status==ds::Status::Partial);CHECK(!f.app.session.hasCurrent()&&f.app.document.text().empty());CHECK(metadata(f,id).area==ds::DocumentArea::Trash);
 Fixture reboot(f.path);reboot.start();CHECK(!reboot.app.session.hasCurrent()&&reboot.app.filesOpen());
 // Flush failure after visible metadata commit is also reconciled.
 f.key(23);selectId(f,id);s3fs::faults.flush=true;f.key(40);s3fs::faults={};CHECK(f.app.lastOperation.status==ds::Status::Partial&&metadata(f,id).area==ds::DocumentArea::Normal);
 f.key(23);selectId(f,id);f.key(40);f.key(59);s3fs::faults.flush=true;renameTo(f,"Visible");s3fs::faults={};CHECK(!f.app.renameOpen()&&f.app.lastOperation.status==ds::Status::Partial&&f.app.currentName=="Visible");
 f.app.catalog.invalidateCache();s3fs::faults.directory=true;CHECK(f.app.manager.refresh()==ds::Status::IoError);s3fs::faults={};CHECK(f.app.manager.state().page.items.empty());
 s3fs::faults.read=true;Fixture bad(f.path);CHECK(!bad.app.startup(1));s3fs::faults={};CHECK(metadata(f,id).name=="Visible");
}
void docTokensPages(){
 Fixture f;f.start();auto id=f.app.session.currentId();f.key(59);f.key(7);CHECK(f.app.manager.refresh()==ds::Status::Ok);f.key(40);CHECK(metadata(f,id).area==ds::DocumentArea::Normal);
 f.key(59);f.key(7);f.app.disconnected();CHECK(f.app.manager.state().pending==ds::PendingAction::None);f.key(40);CHECK(metadata(f,id).area==ds::DocumentArea::Normal);
 f.key(59);f.key(21);f.app.disconnected();CHECK(!f.app.renameOpen()&&!*f.app.renameText());
 f.key(7);CHECK(f.app.session.create("Other",++f.now));CHECK(f.app.manager.confirm(++f.now).status==ds::Status::StaleToken);CHECK(metadata(f,id).area==ds::DocumentArea::Normal);
 f.app.manager.enter(ds::FileManagerView::Normal);f.key(7);f.storage.noteUnmounted();f.storage.noteMounted();CHECK(f.app.manager.confirm(++f.now).status==ds::Status::StaleToken);
 Fixture pages;pages.start();for(unsigned i=0;i<16;++i)CHECK(pages.app.session.create({},++pages.now));pages.key(59);pages.key(79);CHECK(pages.app.manager.state().page.offset==16&&pages.app.manager.state().page.items.size()==1);pages.key(7);pages.key(40);CHECK(pages.app.manager.state().page.offset==0&&pages.app.manager.state().page.items.size()==16);
 Fixture empty;empty.start();empty.key(59);empty.key(7);empty.key(40);CHECK(empty.app.manager.state().page.items.empty());empty.key(41);CHECK(empty.app.manager.state().view==ds::FileManagerView::TrashedReturn);empty.key(41);empty.key(17);empty.settle();CHECK(empty.app.session.hasCurrent());
}
void docLong(){
 Fixture f;f.start();auto id=f.app.session.currentId();CHECK(s3app::AppTestAccess::document(f.app).replaceAll(draftor::memory::String(65536,'q'),0,true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);f.save();auto body=f.app.document.text().data();auto snapshot=f.app.persistence.snapshotAllocation();
 f.key(59);s3profile::begin("metadata_only_64k");
 draftor::memory::allocation_fail_after=0;
 for(unsigned i=0;i<100;++i){f.key(21);f.key(41);CHECK(!*f.app.renameText());}draftor::memory::allocation_fail_after=-1;renameTo(f,"Long renamed");
 auto profile=s3profile::finish();CHECK(profile.read_bytes<8192&&profile.write_bytes<1024);CHECK(f.app.document.text().data()==body&&f.app.persistence.snapshotAllocation()==snapshot);CHECK(f.app.document.text().size()==65536);
 f.key(7);f.key(40);f.key(23);selectId(f,id);f.key(40);f.key(23);selectId(f,id);f.key(40);CHECK(f.app.document.text()==draftor::memory::String(65536,'q'));
 std::printf("DOCMANAGER_64K sizeof_App=%zu metadata_read_bytes=%llu name_capacity=97 cycles=100 body_pointer_stable=1 snapshot_pointer_stable=1\n",sizeof(s3app::App),(unsigned long long)profile.read_bytes);
}
#include "docmanager_display.hpp"
void renderDocManager(const std::string& path){
 Fixture f;f.start();f.key(59);renameTo(f,draftor::memory::String(96,'W'));
 constexpr int width=2400,height=400;std::vector<unsigned char> pixels(width*height*3,255);int panel=0;
 auto pixel=[&](int x,int y,bool white){CHECK(x>=0&&x<300&&y>=0&&y<400);size_t at=(y*width+panel*300+x)*3;for(int c=0;c<3;++c)pixels[at+c]=white?255:0;};
 auto label=[&](const char* text,int x,int y,bool white){std::string_view value(text);for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){auto g=f.smallFont.glyph(s3app::display::scalar(value,at));if(x+g.advance>294)break;for(unsigned r=0;r<g.height;++r)for(unsigned c=0;c<g.width;++c)if(g.bits[r*g.stride+c/8]&(0x80>>(c%8)))pixel(x+g.bearing+c,y+r,white);x+=g.advance;}};
 auto rect=[&](int x,int y,int w,int h){for(int r=0;r<h;++r)for(int c=0;c<w;++c)pixel(x+c,y+r,false);};
 for(panel=0;panel<8;++panel){
  if(panel==1){f.key(21);f.key(4,1);}
  if(panel==2){f.key(41);f.key(7);}
  if(panel==3){f.key(40);f.key(23);}
  if(panel==4)f.key(41);
  if(panel==5)f.key(41);
  if(panel==6){f.key(41);f.key(23);f.key(40);}
  if(panel==7){f.key(41);f.key(41);}
  label("S3 English Writing v0.5.4 DocManager",8,5,false);
  CHECK(s3app::display::documentMenu(f.app,f.smallFont,label,rect));
  label("BLE ready",8,383,false);
 }
 std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<" "<<height<<"\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());CHECK(out.good());
 std::puts("PASS actual-font menu raster: normal / 96-byte select-all / ID confirmation / trash / empty / trashed return / restored full name / no active; labels bounded");
}
