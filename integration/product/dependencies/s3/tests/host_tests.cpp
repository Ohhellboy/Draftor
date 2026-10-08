#include "writing_app.hpp"
#include "operation_profile.hpp"
#include "input_route.hpp"
#include "latin_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include "optional_resources.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
namespace ds=hanime::documents;
#define CHECK(v) do{if(!(v))throw std::runtime_error(std::string(__func__)+":"+std::to_string(__LINE__)+" " #v);}while(0)
static const uint8_t bootMap[]={0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x75,0x01,0x95,0x08,0x81,0x02,0x75,0x08,0x95,0x01,0x81,0x01,0x19,0x00,0x29,0xff,0x75,0x08,0x95,0x06,0x81,0x00,0xc0};
void hid(){
 s3hid::Map map;CHECK(map.parse(bootMap,sizeof(bootMap)));auto r=map.report(0);CHECK(r&&r->bits==64);
 uint8_t b[]={0x22,0,4,5,0,0,0,0};s3hid::State state;CHECK(map.decode(*r,b,8,state));CHECK(state.mods==0x22&&state.has(4)&&state.has(5));CHECK(!map.decode(*r,b,7,state));
 s3hid::Tracker t;std::vector<s3hid::Event> e;auto emit=[&](auto event){e.push_back(event);};t.update(0,state,3,99,emit);CHECK(e.size()==4);t.update(0,state,3,100,emit);CHECK(e.size()==4);
 b[0]=0x20;b[3]=0;CHECK(map.decode(*r,b,8,state));t.update(0,state,3,102,emit);CHECK(e.size()==6&&e.back().usage==5&&!e.back().down&&e.back().mods==0x20);
 t.clear();CHECK(t.allReleased());s3hid::State heldOne,heldTwo;heldOne.set(4);heldTwo.mods=2;t.update(0,heldOne,4,102,[](auto){});t.update(1,heldTwo,4,102,[](auto){});CHECK(!t.allReleased());t.update(0,{},4,103,[](auto){});CHECK(!t.allReleased());t.update(1,{},4,104,[](auto){});CHECK(t.allReleased());
 t.clear();e.clear();state={};t.update(0,state,4,103,emit);CHECK(e.empty());
 b[2]=1;CHECK(!map.decode(*r,b,8,state)); // rollover never interpreted as letters
 std::vector<uint8_t> idmap{0x85,7};idmap.insert(idmap.end(),std::begin(bootMap),std::end(bootMap));CHECK(map.parse(idmap.data(),idmap.size()));CHECK(map.report(7)&&!map.report(0));
 const uint8_t nkro[]={0x05,7,0x19,4,0x29,11,0x75,1,0x95,8,0x81,2};CHECK(map.parse(nkro,sizeof(nkro)));uint8_t n=0x81;CHECK(map.decode(*map.report(0),&n,1,state));CHECK(state.has(4)&&state.has(11));
 CHECK(!map.parse(nkro,sizeof(nkro)-1));
 const uint8_t consumer[]={0x05,0x0c,0x09,1,0x75,8,0x95,1,0x81,0};CHECK(!map.parse(consumer,sizeof(consumer)));
 CHECK(s3hid::uk(31,2)=='"'&&s3hid::uk(32,0x20)==0xa3&&s3hid::uk(52,2)=='@'&&s3hid::uk(100,0)=='\\'&&s3hid::uk(100,0x20)=='|'&&s3hid::uk(8,0x40)==0x20ac);
 s3hid::Repeat repeat;s3hid::Event out;repeat.event({1,100,80,2,true});CHECK(!repeat.poll(449,1,out));CHECK(repeat.poll(450,1,out));CHECK(out.mods==2);CHECK(repeat.poll(1000,1,out));CHECK(!repeat.poll(1000,1,out));repeat.clear();CHECK(!repeat.poll(5000,2,out));repeat.event({2,5000,22,1,true});CHECK(!repeat.poll(6000,2,out));
 repeat.event({2,7000,80,0,true});repeat.context(0x100000001ULL,true);CHECK(!repeat.poll(8000,2,out));repeat.event({2,9000,80,0,true});repeat.context(0x100000001ULL,false);CHECK(!repeat.poll(10000,2,out));repeat.context(2,true);
 repeat.event({2,11000,80,0,true});CHECK(repeat.poll(11350,2,out));repeat.actionResult(false);CHECK(!repeat.poll(12000,2,out));repeat.clear();
 repeat.event({2,0xfffffff0u,80,0,true});CHECK(repeat.poll(334,2,out));repeat.event({2,385,80,0,false});CHECK(!repeat.poll(1000,2,out));
}
struct Fixture {
 std::string path; s3fs::Fs fs;ds::ArduinoSdStoragePort storage;s3host::BuiltinLatinFont font;s3host::BuiltinSmallFont smallFont;s3app::App app;uint32_t now=100;
 static std::string temp(){char p[]="/tmp/hanime-s3-port-tests-XXXXXX";auto s=mkdtemp(p);if(!s)throw std::runtime_error("mkdtemp");return s;}
 explicit Fixture(std::string p=temp()):path(std::move(p)),fs(path),storage(fs),app(storage,{},1234,font){storage.noteMounted();}
 void start(){CHECK(app.startup(now));}
 void key(uint8_t usage,uint8_t mods=0){app.key({1,++now,usage,mods,true});}
 void settle(){unsigned steps=0;while(app.persistence.backgroundSaveActive()||app.commandBarrier()){app.service(++now,false);CHECK(++steps<12000);}CHECK(!app.persistence.backgroundSaveActive());}
 void save(){key(22,1);CHECK(app.persistence.backgroundSaveActive());settle();CHECK(app.lastOperation);}
};
void files(){Fixture f;auto& s=f.storage;CHECK(s.mkdirs("/HANIME_S3_TEST/v01")==ds::Status::Ok);
 const char* p="/HANIME_S3_TEST/v01/api.bin";CHECK(s.write(p,"long old text")==ds::Status::Ok);CHECK(s.write(p,"x")==ds::Status::Ok);CHECK(s.read(p,99).bytes=="x");
 auto a=f.fs.open(p,s3fs::Append);CHECK(a.write((const uint8_t*)"z",1)==1);CHECK(a.flush()&&a.close());CHECK(s.read(p,99).bytes=="xz");
 CHECK(s.write("/DOCS/existing.txt","no")==ds::Status::InvalidName);CHECK(s.write("/HANIME_S3_TEST/v01/../escape","no")==ds::Status::InvalidName);CHECK(s.read("/DOCS/DRAFT.TXT",999).status==ds::Status::InvalidName);
 s3fs::faults.write_bytes=1;CHECK(s.write(p,"three")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.flush=true;CHECK(s.write(p,"flush fail")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.close=true;CHECK(s.write(p,"close fail")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.rename=true;CHECK(s.rename(p,"/HANIME_S3_TEST/v01/other.bin")==ds::Status::IoError);s3fs::faults={};CHECK(s.rename(p,"/HANIME_S3_TEST/v01/other.bin")==ds::Status::Ok);
}
void accessPaths(){
 Fixture f;const char* path="/HANIME_S3_TEST/v01/nested/deep/slot.bin";
 CHECK(f.storage.write(path,"initial")==ds::Status::Ok); // missing parents fallback
 for(size_t length:{size_t(6000),size_t(1),size_t(0)}){
  draftor::memory::String bytes(length,'q');s3profile::begin("overwrite_inactive_slot");
  CHECK(f.storage.write(path,bytes)==ds::Status::Ok);auto io=s3profile::finish();
  CHECK(io.calls[s3profile::Stat].count==0&&io.calls[s3profile::Unlink].count==0);
  CHECK(io.calls[s3profile::Open].count==1&&io.calls[s3profile::Close].count==1&&io.calls[s3profile::Fsync].count==1);
  CHECK(f.storage.read(path,7000).bytes==bytes);
  const std::string_view part(bytes);CHECK(f.storage.verifyParts(path,&part,1)==ds::Status::Ok);
 }
 s3profile::begin("direct_read");auto read=f.storage.read(path,7000);auto io=s3profile::finish();
 CHECK(read.status==ds::Status::Ok&&read.bytes.empty());
 CHECK(io.calls[s3profile::Stat].count==0&&io.calls[s3profile::Open].count==1);
 const char* dir="/HANIME_S3_TEST/v01/nested/deep";
 CHECK(!f.fs.openFile(dir));CHECK(f.storage.read(dir,7000).status==ds::Status::IoError);
 CHECK(f.storage.write(dir,"must fail")==ds::Status::IoError);
 CHECK(f.storage.write("/HANIME_S3_TEST/v01/blocker","keep")==ds::Status::Ok);
 CHECK(f.storage.write("/HANIME_S3_TEST/v01/blocker/child","no")==ds::Status::IoError);
 CHECK(f.storage.read("/HANIME_S3_TEST/v01/blocker",99).bytes=="keep");
 CHECK(f.storage.read("/HANIME_S3_TEST/v01/missing",99).status==ds::Status::NotFound);
 // New direct opens must not consume file descriptors across errors or ordinary closes.
 for(unsigned i=0;i<100;++i){auto ordinary=f.fs.openFile(path);CHECK(ordinary);CHECK(!f.fs.openFile(dir));}
 auto folder=f.fs.openDirectory(dir);CHECK(folder&&folder.isDirectory());CHECK(folder.close());
 // Exercise the record-level truncate capability, including reuse of a larger slot.
 ds::RecordStore records(f.storage);const char* base="/HANIME_S3_TEST/v01/pair";
 CHECK(records.commitPair(base,ds::RecordKind::Position,1,draftor::memory::String(6000,'p'))==ds::Status::Ok);
 CHECK(records.commitPair(base,ds::RecordKind::Position,2,"older")==ds::Status::Ok);
 s3profile::begin("record_reuses_larger_slot");
 CHECK(records.commitPair(base,ds::RecordKind::Position,3,"")==ds::Status::Ok);io=s3profile::finish();
 CHECK(io.calls[s3profile::Unlink].count==0&&io.calls[s3profile::Fsync].count==1);
 auto loaded=records.loadPair(base,ds::RecordKind::Position,7000);
 CHECK(loaded.record.generation==3&&loaded.record.payload.empty());
 s3fs::faults.flush=true;CHECK(records.commitPair(base,ds::RecordKind::Position,4,"failed")==ds::Status::IoError);s3fs::faults={};
 // A failed acknowledgement is not proof the medium did not accept the bytes.
 // Explicitly damage that failed candidate and prove the older verified empty record survives.
 CHECK(f.storage.write("/HANIME_S3_TEST/v01/pair.B","broken")==ds::Status::Ok);
 loaded=records.loadPair(base,ds::RecordKind::Position,7000);
 CHECK(loaded.status==ds::Status::OkDegraded&&loaded.record.generation==3&&loaded.record.payload.empty());
}

void editing(){Fixture f;f.start();f.key(4);f.key(5,0x20);f.key(31,2);CHECK(f.app.document.text()=="aB\"");
 f.key(80,2);CHECK(f.app.editor.selection().has_selection);f.key(6,1);f.key(77);f.key(25,1);CHECK(f.app.document.text()=="aB\"\"");f.key(29,1);CHECK(f.app.document.text()=="aB\"");f.key(29,3);CHECK(f.app.document.text()=="aB\"\"");
 f.key(40);f.key(6);f.key(42);f.key(80);f.key(76);CHECK(f.app.document.text()=="aB\"\"");
 auto before=f.app.document.text();f.key(43,2);f.key(4,3);f.key(28,8);CHECK(f.app.document.text()==before&&!f.app.editor.selection().has_selection); // donor ignores Shift+Tab, Ctrl+Shift+A, Cmd+Y
 f.key(29,1);auto undone=f.app.document.text();f.key(28,8);CHECK(f.app.document.text()==undone);f.key(28,1);CHECK(f.app.document.text()==before); // Ctrl+Y only
 auto oldFocus=f.app.editStamp();f.key(18,1);f.key(41);CHECK(!f.app.commitText("late focus",oldFocus));
 auto stamp=f.app.editStamp();f.key(4);CHECK(!f.app.commitText("stale",stamp));auto current=f.app.editStamp();CHECK(f.app.commitText(" future",current));CHECK(f.app.document.text().find("future")!=draftor::memory::String::npos);
 s3host::NoOptionalResources resources;s3host::RandomAccessSource* source=nullptr;CHECK(resources.open(s3host::ResourceRole::Lexicon,"opaque",source)==s3host::ResourceStatus::NotConfigured);CHECK(!source);f.save();
}
void saves(){Fixture f;f.start();f.key(4);f.key(5);f.key(80);auto id=f.app.session.currentId();auto cursor=f.app.document.cursorByte();f.save();CHECK(!f.app.document.dirty());
 f.key(6);auto captured=f.app.document.text();f.key(22,1);CHECK(f.app.persistence.backgroundSaveActive());
 for(unsigned i=0;i<8;++i){f.app.service(++f.now,false);f.key(7);}
 f.settle();CHECK(f.app.document.dirty());auto saved=f.app.persistence.loadLane(id,ds::SaveLane::Manual);CHECK(saved.snapshot.text==captured);f.save();
 auto expected=f.app.document.text();auto expectedCursor=f.app.document.cursorByte();CHECK(f.app.session.create("B",++f.now));f.key(27);f.save();CHECK(f.app.session.open(id,++f.now));CHECK(f.app.document.text()==expected&&f.app.document.cursorByte()==expectedCursor);
 // Another host object (and separately process probe below) restores on-disk CURRENT.
 ds::ArduinoSdStoragePort otherStorage(f.fs);otherStorage.noteMounted();s3app::App other(otherStorage,{},5678,f.font);CHECK(other.startup(++f.now));CHECK(other.document.text()==expected&&other.document.cursorByte()==expectedCursor);
 f.key(80);f.save();CHECK(!f.app.persistence.positionDirty());
 for(unsigned fault=0;fault<3;++fault){f.key(4);f.key(22,1);if(fault==0)s3fs::faults.write_bytes=0;if(fault==1)s3fs::faults.flush=true;if(fault==2)s3fs::faults.close=true;f.settle();CHECK(!f.app.lastOperation);CHECK(f.app.document.dirty());s3fs::faults={};f.save();}
 (void)cursor;
}
void selectionGeometry(){Fixture f;f.start();CHECK(f.app.commitText("A word that wraps near the right edge and continues.\nSecond line",f.app.editStamp()));
 f.key(74,1);f.key(81,2);CHECK(f.app.editor.selection().has_selection);auto v=f.app.navigator.sync();CHECK(v.cursor_byte==f.app.document.cursorByte());
 f.key(6,1);auto clipboard=f.app.editor.clipboard().size();CHECK(clipboard>0);f.key(77,1);f.key(25,1);CHECK(f.app.document.dirty());f.key(29,1);CHECK(f.app.editor.clipboard().size()==clipboard);
 // PSRAM decoder failure must propagate, not terminate inside donor noexcept.
 ds::DocumentMetadata meta;meta.id.bytes[0]=1;meta.name=draftor::memory::String(60,'m');auto metaBytes=ds::encodeMetadata(meta);
 ds::Snapshot snapshot;snapshot.text=draftor::memory::String(200,'s');auto snapshotBytes=ds::encodeSnapshot(snapshot);
 ds::CreateIntent intent;intent.active=true;intent.metadata=meta;auto intentBytes=ds::encodeCreateIntent(intent);
 auto recordBytes=ds::RecordStore::encode(ds::RecordKind::Snapshot,1,snapshotBytes);
 auto mustFail=[&](auto action){bool caught=false;draftor::memory::allocation_fail_after=0;try{action();}catch(const std::bad_alloc&){caught=true;}draftor::memory::allocation_fail_after=-1;CHECK(caught);};
 mustFail([&]{ds::DocumentMetadata out;ds::decodeMetadata(metaBytes,&out);});
 mustFail([&]{ds::Snapshot out;ds::decodeSnapshot(snapshotBytes,&out);});
 mustFail([&]{ds::CreateIntent out;ds::decodeCreateIntent(intentBytes,&out);});
 mustFail([&]{ds::Record out;ds::RecordStore::decode(recordBytes,ds::RecordKind::Snapshot,65536,&out);});
 auto saved=f.app.document.text();auto cursor=f.app.document.cursorByte();auto changed=s3app::AppTestAccess::document(f.app).prepareReplaceRange(0,0,draftor::memory::String(6000,'x'),6000);CHECK(changed==editor::EditResult::Changed);
 // Force growth after a valid stable document; failure must be atomic.
 std::string huge(60000,'y');draftor::memory::allocation_fail_after=0;auto r=s3app::AppTestAccess::document(f.app).prepareReplaceRange(0,0,huge,huge.size());draftor::memory::allocation_fail_after=-1;CHECK(r==editor::EditResult::ResourceExhausted);CHECK(f.app.document.text()==saved&&f.app.document.cursorByte()==cursor);
}
void commandBarrier(){Fixture f;f.start();f.key(4);f.key(22,1);f.key(18,1);CHECK(f.app.commandBarrier());f.settle();CHECK(f.app.filesOpen());f.key(17);CHECK(f.app.commandBarrier());f.settle();CHECK(!f.app.filesOpen());f.key(5);CHECK(f.app.document.text()=="b");
 f.key(22,1);f.key(18,1);CHECK(f.app.commandBarrier());f.app.disconnected();CHECK(!f.app.commandBarrier());f.settle();CHECK(!f.app.filesOpen());
}
draftor::memory::String longText(){draftor::memory::String text;for(unsigned i=0;i<420;++i){char line[180];std::snprintf(line,sizeof(line),"Line %04u: The quick brown fox jumps over the lazy dog. 0123456789 UK symbols: ! \" # $ %% & ' ( ) + , - . / : ; < = > ? @ [ ] _ ~\n",i+1);text+=line;}CHECK(text.size()<65536&&text.size()>48000);return text;}
void longRun(){Fixture f;f.start();auto text=longText();CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,text.size(),true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);f.save();auto id=f.app.session.currentId();
 for(unsigned i=0;i<30;++i) { f.key(82); }
 f.key(22,1);for(unsigned i=0;i<12;++i){f.key(4);for(unsigned k=0;k<10;++k)f.app.service(++f.now,false);}f.settle();CHECK(f.app.document.dirty());f.save();auto final=f.app.document.text();auto cursor=f.app.document.cursorByte();CHECK(f.app.session.create("Long switch B",++f.now));CHECK(f.app.session.open(id,++f.now));CHECK(f.app.document.text()==final&&f.app.document.cursorByte()==cursor);
 std::printf("long document bytes=%zu cursor=%zu sizeof(App)=%zu clipboard=%zu history=%zu\n",final.size(),cursor,sizeof(s3app::App),f.app.editor.memoryBudget().clipboard_bytes,f.app.editor.memoryBudget().history_pool_bytes);
}

void profileFrame(Fixture& f) {
 if(f.app.filesOpen()) return;
 auto view=f.app.navigator.sync();auto anchor=f.app.navigator.firstVisibleAnchor();
 hanime::renderer::Placement placements[80];
 s3profile::Scope timing(s3profile::Layout);
 for(unsigned row=0;row<f.app.layout.visibleTextLineCount()&&anchor.valid;++row){
  auto line=f.app.layout.layoutVisualLine(f.app.document.text(),anchor,view.first_visible_line,placements,80);
  if(line.document_end) { break; }
  anchor=line.next;
 }
}
void profileResult(Fixture& f){profileFrame(f);auto r=s3profile::finish();s3profile::print(r,ds::statusMessage(f.app.lastOperation.status),f.app.document.text().size(),f.app.document.cursorByte());}
void selectId(Fixture& f,const ds::DocumentId& id){
 for(size_t i=0;i<f.app.manager.state().page.items.size();++i){
  if(f.app.manager.state().page.items[i].id==id){f.app.manager.moveSelection(int(i)-int(f.app.manager.state().selected));return;}
 }throw std::runtime_error("ID missing from fixture page");
}
void profileSwitches(){
 Fixture f;f.start();auto a=f.app.session.currentId();CHECK(f.app.commitText("Short A saved text\n",f.app.editStamp()));f.save();
 CHECK(f.app.session.create("B",++f.now));CHECK(f.app.commitText("Short B saved text\n",f.app.editStamp()));f.save();auto b=f.app.session.currentId();
 CHECK(f.app.session.create("Long",++f.now));auto text=longText();CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,0,true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);f.save();auto longId=f.app.session.currentId();
 CHECK(f.app.session.open(a,++f.now));
 for(unsigned round=0;round<2;++round){
  const ds::DocumentId ids[]={longId,b,a,longId};
  const char* names[]={"open_long","switch_short_B","switch_short_A","reopen_long"};
  for(unsigned i=0;i<4;++i){
   s3profile::begin("files");f.key(18,1);profileResult(f);CHECK(f.app.filesOpen());selectId(f,ids[i]);
   s3profile::begin(names[i]);f.key(40);profileResult(f);CHECK(f.app.lastOperation);CHECK(!f.app.filesOpen());
   if(ids[i]==longId){CHECK(f.app.document.text()==text);CHECK(f.app.document.cursorByte()==(round?text.size():0));}
   else {CHECK(f.app.document.text()==(ids[i]==a?"Short A saved text\n":"Short B saved text\n"));CHECK(f.app.document.cursorByte()==19);}
  }
  if(round==0){f.key(77,1);f.save();CHECK(f.app.session.open(a,++f.now));}
 }
 // Distinguish the dirty AUTO path from the clean menu/open measurements above.
 f.key(77,1);f.key(27);const auto dirty=f.app.document.text();const auto cursor=f.app.document.cursorByte();
 s3profile::begin("dirty_files");f.key(18,1);f.settle();profileResult(f);CHECK(f.app.filesOpen());
 selectId(f,a);s3profile::begin("dirty_auto_switch_short");f.key(40);CHECK(f.app.commandBarrier());f.settle();profileResult(f);CHECK(f.app.document.text()=="Short A saved text\n");
 f.key(18,1);f.settle();profileResult(f);selectId(f,longId);
 s3profile::begin("reopen_auto_long");f.key(40);f.settle();profileResult(f);
 CHECK(f.app.document.text()==dirty&&f.app.document.cursorByte()==cursor);

}


void openRegression(){
 // Font lookup changes implementation only; every stored glyph keeps identity/metrics.
 for(const auto& glyph:s3font::glyphs)CHECK(s3font::get(glyph.cp)==&glyph);
 CHECK(!s3font::get(31)&&!s3font::get(127)&&!s3font::get(0x4e00));
 hanime::renderer::LayoutConfig config;config.width_px=44;
 config.codepoint_advance=[](const void*,uint32_t cp)->uint16_t{return cp=='^'?0:cp=='i'?3:11;};
 hanime::renderer::LayoutEngine variable(config);const char* variableText="^^ii words wrap\n^second";
 for(size_t i=0;i<=std::strlen(variableText);++i){
  hanime::renderer::VisualLineAnchor anchor;
  auto caret=variable.visualCaretAtFromAnchor(variableText,{0,0,true},i,hanime::renderer::CaretAffinity::Downstream,nullptr,&anchor);
  auto reference=variable.visualLineAnchorAt(variableText,caret.line);
  CHECK(anchor.valid&&anchor.byte_offset==reference.byte_offset&&anchor.line==reference.line);
 }

 Fixture f;f.start();auto a=f.app.session.currentId();
 CHECK(f.app.commitText("Short A stays here\n",f.app.editStamp()));f.key(80);f.save();
 auto shortText=f.app.document.text();auto shortCursor=f.app.document.cursorByte();
 CHECK(f.app.session.create("Long",++f.now));auto longId=f.app.session.currentId();auto text=longText();
 CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,text.size()-137,true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);f.save();
 auto longCursor=f.app.document.cursorByte();auto longAnchor=f.app.navigator.firstVisibleAnchor();auto longView=f.app.navigator.sync();
 for(unsigned i=0;i<3;++i){
  CHECK(f.app.session.open(a,++f.now));CHECK(f.app.document.text()==shortText&&f.app.document.cursorByte()==shortCursor);
  CHECK(f.app.session.open(longId,++f.now));CHECK(f.app.document.text()==text&&f.app.document.cursorByte()==longCursor);
  auto view=f.app.navigator.sync();CHECK(view.visual_line==longView.visual_line&&view.cursor_x==longView.cursor_x&&view.affinity==longView.affinity);
  CHECK(f.app.navigator.firstVisibleAnchor().byte_offset==longAnchor.byte_offset);
 }
 // Compare optimized viewport lookup to the original from-start geometry. Include
 // soft-wrap affinity, non-anchor persisted bytes, stale viewports and UTF-8.
 const draftor::memory::String samples[]={"Short words, longer words; a line wraps here.\nNext £ € "+draftor::memory::String(160,'W'),text};
 for(const auto& sample:samples){
  CHECK(s3app::AppTestAccess::document(f.app).replaceAll(sample,0,false));
  std::vector<size_t> positions{0,sample.size()/2,sample.size()};
  if(sample.size()<1000){positions.clear();for(size_t i=0;i<=sample.size();++i)if(ds::isUtf8Boundary(sample,i))positions.push_back(i);}
  for(size_t cursor:positions)for(auto affinity:{hanime::renderer::CaretAffinity::Upstream,hanime::renderer::CaretAffinity::Downstream}){
   if(!ds::isUtf8Boundary(sample,cursor))continue;
   const auto caret=f.app.layout.visualCaretAt(sample,cursor,affinity);CHECK(caret.valid);
   auto near=f.app.layout.visualLineAnchorAt(sample,caret.line>4?caret.line-4:0);
   const size_t viewports[]={size_t(0),near.byte_offset,cursor,sample.size()};
   for(size_t viewport:viewports){
    CHECK(s3app::AppTestAccess::document(f.app).setCursorByte(cursor)!=editor::EditResult::InvalidPosition);
    auto viewportCaret=f.app.layout.visualCaretAt(sample,viewport);
    auto expectedLine=viewportCaret.line;auto visible=f.app.layout.visibleTextLineCount();
    if(caret.line<expectedLine)expectedLine=caret.line;
    if(caret.line>=expectedLine+visible)expectedLine=caret.line-visible+1;
    auto expectedAnchor=f.app.layout.visualLineAnchorAt(sample,expectedLine);
    CHECK(f.app.navigator.restoreViewportAnchor(viewport,affinity));auto actual=f.app.navigator.sync();
    CHECK(actual.visual_line==caret.line&&actual.cursor_x==caret.x&&actual.affinity==caret.affinity);
    CHECK(actual.first_visible_line==expectedLine&&f.app.navigator.firstVisibleAnchor().byte_offset==expectedAnchor.byte_offset);
   }
  }
 }
 CHECK(f.app.session.open(longId,++f.now));
 CHECK(f.app.commitText("dirty",f.app.editStamp()));auto dirtyText=f.app.document.text();auto dirtyCursor=f.app.document.cursorByte();
 f.key(18,1);selectId(f,a);f.key(40);CHECK(f.app.commandBarrier());
 CHECK(f.app.session.currentId()==longId);f.settle();CHECK(f.app.session.currentId()==a);
 CHECK(f.app.session.open(longId,++f.now));CHECK(f.app.document.text()==dirtyText&&f.app.document.cursorByte()==dirtyCursor);
 CHECK(f.app.persistence.loadLane(longId,ds::SaveLane::Auto).snapshot.text==dirtyText);
 CHECK(f.app.commitText("failed save",f.app.editStamp()));auto unsaved=f.app.document.text();
 f.key(18,1);selectId(f,a);f.key(40);CHECK(f.app.commandBarrier());s3fs::faults.flush=true;f.settle();s3fs::faults={};
 CHECK(!f.app.lastOperation&&f.app.session.currentId()==longId&&f.app.document.text()==unsaved&&f.app.document.dirty());
 // Batch read handles short POSIX returns; errors remain errors.
 const char* raw="/HANIME_S3_TEST/v01/batch.bin";CHECK(f.storage.write(raw,text)==ds::Status::Ok);
 s3fs::faults.max_read_bytes=113;CHECK(f.storage.read(raw,65536).bytes==text);s3fs::faults={};
 s3fs::faults.read=true;CHECK(f.storage.read(raw,65536).status==ds::Status::IoError);s3fs::faults={};
 // Enumerating directory entries must preserve lexical pagination and expose errors.
 std::vector<draftor::memory::String> expected;
 for(unsigned i=0;i<37;++i){auto path=draftor::memory::String("/HANIME_S3_TEST/v01/list/")+std::to_string(100+i).c_str();CHECK(f.storage.write(path,"x")==ds::Status::Ok);expected.push_back(path);}
 std::vector<draftor::memory::String> found; draftor::memory::String after;
 for(;;){auto page=f.storage.list("/HANIME_S3_TEST/v01/list/",after,5);CHECK(page.status==ds::Status::Ok);for(auto& path:page.paths){found.push_back(path);after=path;}if(!page.has_more)break;CHECK(!page.paths.empty());}
 CHECK(found==expected);
 s3fs::faults.directory=true;CHECK(f.storage.list("/HANIME_S3_TEST/v01/list/",{},5).status==ds::Status::IoError);s3fs::faults={};
 // CRC fallback still uses the older valid record; snapshot body CRC is still checked.
 ds::RecordStore records(f.storage);const char* base="/HANIME_S3_TEST/v01/CRC_TEST";
 CHECK(records.commitPair(base,ds::RecordKind::Snapshot,1,"old")==ds::Status::Ok);
 CHECK(records.commitPair(base,ds::RecordKind::Snapshot,2,"new")==ds::Status::Ok);
 auto broken=f.storage.read(draftor::memory::String(base)+".B",99).bytes;broken.back()^=1;CHECK(f.storage.write(draftor::memory::String(base)+".B",broken)==ds::Status::Ok);
 auto recovered=records.loadPair(base,ds::RecordKind::Snapshot,99);CHECK(recovered.status==ds::Status::OkDegraded&&recovered.record.payload=="old"&&recovered.used_older_slot);
 ds::Snapshot snapshot;snapshot.text="body";auto encoded=ds::encodeSnapshot(snapshot);encoded.back()^=1;ds::Snapshot out;CHECK(ds::decodeSnapshotOwned(std::move(encoded),&out)==ds::Status::Corrupt);
}
void transitionInput(){
 Fixture f;f.start();auto a=f.app.session.currentId();CHECK(f.app.commitText("A",f.app.editStamp()));f.save();
 CHECK(f.app.session.create("B",++f.now));CHECK(f.app.commitText("B",f.app.editStamp()));f.save();auto b=f.app.session.currentId();CHECK(f.app.session.open(a,++f.now));
 s3hid::InputRoute route;s3hid::Repeat repeat;
 auto present=[&]{if(route.needsFrame(f.app.focusToken())){repeat.clear();profileFrame(f);route.framePresented(f.app.focusToken());}};
 present();
 auto event=[&](uint8_t usage,uint8_t mods=0){return s3hid::Event{1,++f.now,usage,mods,true,route.epoch()};};
 auto deliver=[&](s3hid::Event e){if(route.accept(e,++f.now)){repeat.event(e);f.app.key(e);present();}};
 auto menu=event(18,1),lateInA=event(4);deliver(menu);CHECK(f.app.filesOpen());deliver(lateInA);CHECK(route.discarded==1);
 selectId(f,b);auto open=event(40),duringOpen=event(27);deliver(open);deliver(duringOpen);CHECK(f.app.document.text()=="B"&&route.discarded==2);
 deliver(event(6));CHECK(f.app.document.text()=="Bc");deliver(event(29,1));CHECK(f.app.document.text()=="B");
 // Saving remains the same document: new input is accepted until a focus command.
 deliver(event(22,1));deliver(event(7));CHECK(f.app.document.text()=="Bd");
 deliver(event(18,1));CHECK(f.app.commandBarrier());auto duringWait=event(28);
 f.settle();present();CHECK(f.app.filesOpen());deliver(duringWait);CHECK(route.discarded==3);
 selectId(f,a);deliver(event(40));f.settle();present();CHECK(f.app.document.text()=="A"&&f.app.document.cursorByte()==1);
 s3hid::Event repeated;CHECK(!repeat.poll(f.now+1000,1,repeated));
 // No deferred write from the waiting or old document contexts reaches A.
 deliver(event(5));CHECK(f.app.document.text()=="Ab");CHECK(route.max_discarded_age_ms>0);
}

void seed(const std::string& path){std::filesystem::create_directories(path);Fixture f(path);f.start();auto a=f.app.session.currentId();CHECK(f.app.session.rename(a,"A - English test")==ds::Status::Ok);CHECK(f.app.commitText("Document A. Type your acceptance text here.\n",f.app.editStamp()));f.save();CHECK(f.app.session.create("B - Clipboard test",++f.now));CHECK(f.app.commitText("Document B. Paste from A here.\n",f.app.editStamp()));f.save();CHECK(f.app.session.create("Long - 53 KiB test",++f.now));auto text=longText();CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,0,true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);f.save();CHECK(f.app.session.open(a,++f.now));std::puts("seed generated from actual document codec");}
#include "reader_tests.hpp"
#include "docmanager_tests.hpp"
int main(int argc,char**argv){try{if(argc==3&&std::string(argv[1])=="--render-docmanager"){renderDocManager(argv[2]);return 0;}if(argc==2&&std::string(argv[1])=="--readers-long"){readersLongBudget();return 0;}if(argc==3&&std::string(argv[1])=="--render-readers"){renderReaders(argv[2]);return 0;}if(argc==2&&std::string(argv[1])=="--profile"){profileSwitches();return 0;}if(argc==3&&std::string(argv[1])=="--seed"){seed(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--restart-write"){std::filesystem::create_directories(argv[2]);Fixture f(argv[2]);f.start();CHECK(f.app.commitText("Across process restart",f.app.editStamp()));f.key(80);f.save();f.key(59);renameTo(f,"Across restart renamed");return 0;}
 if(argc==3&&std::string(argv[1])=="--restart-read"){Fixture f(argv[2]);f.start();CHECK(f.app.document.text()=="Across process restart");CHECK(f.app.document.cursorByte()==21);CHECK(f.app.currentName=="Across restart renamed");std::puts("cross-process body + cursor restored");return 0;}
 const std::pair<const char*,void(*)()> groups[]={{"HID map / reports / modifiers / UK / repeat",hid},{"VFS modes / namespace / write-sync-close failures",files},{"English edit / clipboard / history / future boundary",editing},{"cooperative manual / concurrent edits / switch / failures",saves},{"visual selection / allocator failure atomicity",selectionGeometry},{"command ordering / disconnect cancellation",commandBarrier},{"long document combined run",longRun},{"open restore / layout equivalence / batch I/O / integrity",openRegression},{"transition input ownership / save wait / history",transitionInput},{"direct file access / missing parents / exact truncation",accessPaths},
 {"Find cycles / case / UTF-8 / read-only history",findBehaviour},
 {"Find direct keys / Unicode commit / focus epochs",findInputBoundaries},
 {"Find edit invalidation / stale close",findInvalidation},
 {"Statistics rules / byte-split UTF-8 and words",statisticsRules},
 {"Statistics edits / paste / deletion / undo redo",statsEdits},
 {"Readers + AUTO / MANUAL / switch / disconnect",readersCombined},
 {"64 KiB adversarial scan + save / bounded work",readersLongBudget},
 {"Find highlight / bounded query / partial stats display",readersDisplay},
 {"Names / cancellation / validation / duplicates / reboot / undo",docNames},
 {"Trash / restore / dirty current / no active",docTrash},
 {"Save / metadata / partial / read failures",docFailures},
 {"Confirmation identity / disconnect / mount / pagination",docTokensPages},
 {"64 KiB metadata / repeated cancel memory",docLong}};
 for(auto& g:groups){g.second();std::printf("PASS %s\n",g.first);}std::puts("PASS 23 targeted groups");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
