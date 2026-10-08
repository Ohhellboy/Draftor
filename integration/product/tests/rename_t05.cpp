#include <filesystem>
#include <string>
static const std::filesystem::path t05_root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
static const std::string t05_resources=(t05_root/"resources/staged").string();
#undef RESOURCE_ROOT
#undef INDEX_ROOT
#define RESOURCE_ROOT t05_resources.c_str()
#define INDEX_ROOT t05_resources.c_str()
#define main t05_unused_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "presentation_frame.hpp"
#include "docmanager_display.hpp"
#include "metadata_font.hpp"
#include <map>
#include <fstream>
#include "support/rename_usb_platform.hpp"
struct RenameRig:Rig {
 s3app::ProductInput product;Platform platform;
 s3app::UsbDocumentImport importer{storage,app.catalog,app.persistence,app.session,app.editorPort};
 s3app::UsbTransfer usb{app,platform,importer};
 unsigned usb_consumed=0,cold_entries=0;
 ~RenameRig(){s3fs::faults={};}
 void display(){
  CHECK(ime_device::prepareImePresentation(&ime,app));
  if(app.renameOpen())CHECK(app.horizontalImeSurface());
  const auto published=ime_device::publishActivePresentation(app,&ime,app.presentationFrame());
  CHECK(published.accepted);frame=published.candidates;
  if(ime_device::candidateSurfaceVisible(&ime,&app)&&ime.view())CHECK(frame==ime.view()->generation);
 }
 void settle(){unsigned n=0;do{step();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());display();}
 void clearName(){tap(4,1);tap(42);CHECK(std::string_view(app.renameText()).empty());}
 void tap(uint8_t usage,uint8_t mods=0,bool release=true){
  for(bool down:{true,false}){
   if(!down&&!release)continue;
   s3hid::Event e{1,++now,usage,mods,down,0,frame,ime.attemptSerial()};
   e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();
   if(s3app::usbEntryKey(app,e))++cold_entries;
   if(usb.key(e,{ime.active(),ime.hasWork(),false})){++usb_consumed;continue;}
   if(product.layout(app,e))continue;
   const bool chinese=ime.chinese();if(!product.ime(app,&ime,e))app.key(e,chinese);
  }
 }
 void text(std::string_view t){for(char c:t){if(c>='a'&&c<='z')tap(c-'a'+4);else if(c>='A'&&c<='Z')tap(c-'A'+4,2);else if(c==' ')tap(44);else if(c=='0')tap(39);else if(c>='1'&&c<='9')tap(c-'1'+30);else CHECK(false);}}
 void rename(bool vertical){if(vertical)tap(61);CHECK(app.horizontalActive()!=vertical);tap(59);CHECK(app.filesOpen());tap(21);CHECK(app.renameOpen());tap(4,1);tap(42);CHECK(std::string_view(app.renameText()).empty());}
};

using Files=std::map<std::string,std::string>;
Files disk(const RenameRig& r,bool include_meta=false){
 Files out;for(const auto& entry:fs::recursive_directory_iterator(r.path))if(entry.is_regular_file()){
  const auto name=entry.path().filename().string();if(!include_meta&&name.rfind("META.",0)==0)continue;
  std::ifstream f(entry.path(),std::ios::binary);out[entry.path().lexically_relative(r.path).string()]={std::istreambuf_iterator<char>(f),{}};
 }return out;
}
std::string savedName(RenameRig& r,s3app::ds::DocumentId id){s3app::ds::DocumentMetadata m;const auto status=r.app.catalog.get(id,&m);CHECK(status==s3app::ds::Status::Ok||status==s3app::ds::Status::OkDegraded);return std::string(m.name);}
struct Protected {
 Files files;std::string text;s3app::ds::DocumentId id;uint64_t session;uint32_t revision,text_revision,selection;size_t cursor,undo,redo;
 explicit Protected(RenameRig& r):files(disk(r)),text(r.app.document.text()),id(r.app.session.currentId()),session(r.app.session.sessionSerial()),revision(r.app.document.revision()),text_revision(r.app.document.textRevision()),selection(r.app.editor.selectionVersion()),cursor(r.app.document.cursorByte()),undo(r.app.editor.undoCount()),redo(r.app.editor.redoCount()){
  bool current=false;for(const auto& f:files)current|=f.first.find("CURRENT.")!=std::string::npos;CHECK(current);
 }
 void check(RenameRig& r)const{
  CHECK(std::string_view(r.app.document.text())==std::string_view(text)&&r.app.document.revision()==revision&&r.app.document.textRevision()==text_revision);
  CHECK(r.app.session.currentId()==id&&r.app.session.sessionSerial()==session);
  CHECK(r.app.document.cursorByte()==cursor&&r.app.editor.selectionVersion()==selection&&r.app.editor.undoCount()==undo&&r.app.editor.redoCount()==redo);
  CHECK(disk(r)==files);
 }
};
void seed(RenameRig& r){CHECK(r.app.commitText("Protected body 正文 iuIU",r.app.editStamp()));CHECK(r.app.session.manualSave(++r.now));}
void alphabet(){for(bool vertical:{false,true}){
 RenameRig r;seed(r);r.rename(vertical);const Protected before(r);
 r.text("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZiuIU");
 CHECK(std::string_view(r.app.renameText())=="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZiuIU");
 CHECK(!r.usb_consumed&&!r.cold_entries&&!r.usb.active());
 r.tap(66);CHECK(r.app.renameOpen()&&!r.usb_consumed&&!r.cold_entries);
 r.tap(42);r.tap(42);r.text(" 0123456789");CHECK(std::string_view(r.app.renameText()).substr(52)=="iu 0123456789");
 before.check(r);r.tap(40);CHECK(!r.app.renameOpen()&&r.app.lastOperation.status==s3app::ds::Status::Ok);before.check(r);
 r.tap(41);r.tap(59);CHECK(r.app.filesOpen());CHECK(r.app.manager.state().page.items[0].name=="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZiu 0123456789");before.check(r);
 std::cout<<"PASS alphabet upper/lower iuIU digits space Backspace final-Enter reopen CURRENT/body/files/id H/V="<<vertical<<'\n';
}}
void chinese(){for(bool vertical:{false,true})for(uint8_t choose:{uint8_t(30),uint8_t(44),uint8_t(40)}){
 RenameRig r;seed(r);r.rename(vertical);r.text("iuIU ");const Protected before(r);const auto old=savedName(r,before.id);
 r.tap(57);r.settle();CHECK(r.ime.chinese());r.text("uix");r.tap(42);r.settle();CHECK(r.ime.raw()=="ui"&&r.ime.view()->count&&r.frame);
 const auto choice=r.ime.view()->rows[0].items[0];const std::string word(choice.text.data(),choice.bytes);CHECK(!word.empty());
 // The production UI uses existing horizontal candidate publication in H and V.
 CHECK(ime_device::candidateSurfaceVisible(&r.ime,&r.app));
 r.tap(choose,0,choose!=40);r.settle();
 if(choose==40){r.tap(40);CHECK(r.app.renameOpen());}
 if(!r.ime.raw().empty()){r.tap(40);r.settle();}
 CHECK(r.ime.raw().empty()&&!r.ime.active()&&r.app.renameOpen());
 CHECK(std::string_view(r.app.renameText())=="iuIU "+word&&savedName(r,before.id)==old);
 before.check(r);CHECK(!r.ime.acceptedDocumentCurrent());
 // Idle digits/Space belong to the temporary name; Backspace is UTF-8 aware.
 r.tap(42);CHECK(std::string_view(r.app.renameText())=="iuIU ");r.text("ui");r.settle();r.tap(44);r.settle();
 r.tap(44);r.tap(31);r.tap(42);r.tap(42);CHECK(std::string_view(r.app.renameText())=="iuIU "+word);
 r.tap(57);CHECK(!r.ime.chinese());r.text(" IU");r.tap(57);CHECK(r.ime.chinese());r.tap(57);CHECK(!r.ime.chinese());
 const std::string expected(r.app.renameText());r.tap(40);CHECK(!r.app.renameOpen());before.check(r);CHECK(savedName(r,before.id)==expected);
 r.tap(41);r.tap(59);bool found=false;for(const auto& m:r.app.manager.state().page.items)found|=m.id==before.id&&std::string_view(m.name)==std::string_view(expected);CHECK(found);before.check(r);
 s3app::MetadataFont font(r.font);const auto glyph=font.glyph(s3app::display::scalar(word,0));CHECK(!glyph.missing&&glyph.width<=14&&glyph.height==14);bool ink=false;for(unsigned i=0;i<28;++i)ink|=glyph.bits[i]!=0;CHECK(ink);
 bool label=false;CHECK(s3app::display::documentMenu(r.app,font,[&](const char* t,int,int,bool){label|=std::string_view(t)==expected;},[](int,int,int,int){}));CHECK(label);
 std::cout<<"PASS CN candidate key="<<unsigned(choose)<<" H/V="<<vertical<<" name="<<expected<<" IME_Enter_separate=1 EN/CN=1 CJK_glyph=1 invariants=1\n";
}}
void cancel(){for(bool vertical:{false,true})for(unsigned phase=0;phase<3;++phase){
 RenameRig r;seed(r);r.rename(vertical);const Protected before(r);const auto old=savedName(r,before.id);r.text("cancel iuIU ");r.tap(57);r.settle();r.text("ui");r.settle();
 if(phase==1)r.tap(44); // pending selection, cancelled before cooperative work
 if(phase==2){r.tap(44);r.settle();r.text("ui");r.settle();}
 r.tap(41);CHECK(!r.app.renameOpen()&&r.ime.raw().empty()&&!r.ime.active()&&!r.ime.queuedInput()&&!r.ime.queuedArrows());
 for(unsigned i=0;i<16;++i)r.step();CHECK(!r.ime.view()->count&&!ime_device::candidateSurfaceVisible(&r.ime,&r.app));before.check(r);CHECK(savedName(r,before.id)==old);
 r.tap(21);CHECK(r.app.renameOpen()&&std::string_view(r.app.renameText())==old);r.tap(41);before.check(r);
 std::cout<<"PASS Esc cleanup original name retained H/V="<<vertical<<" phase="<<phase<<'\n';
}}
void validation(){for(bool vertical:{false,true}){
 RenameRig r;seed(r);const auto original=r.app.session.currentId();CHECK(r.app.session.create("occupied",++r.now));CHECK(r.app.session.open(original,++r.now));r.rename(vertical);const Protected before(r);const auto old=savedName(r,original);
 r.text("occupied");r.tap(40);CHECK(r.app.renameOpen()&&r.app.lastOperation.status==s3app::ds::Status::NameExists);CHECK(savedName(r,original)==old);before.check(r);
 for(const auto& bad:{std::string(""),std::string("bad/name"),std::string("bad\\name"),std::string("bad\x01name")}){
  r.clearName();CHECK(r.app.commitName(bad,r.app.nameStamp()));r.tap(40);CHECK(r.app.renameOpen()&&r.app.lastOperation.status==s3app::ds::Status::InvalidName);before.check(r);
 }
 r.clearName();r.text(std::string(96,'a'));const auto revision=r.app.renameRevision();r.text("i");CHECK(std::string_view(r.app.renameText()).size()==96&&r.app.renameRevision()==revision);
 r.clearName();CHECK(!r.app.commitName(std::string(97,'a'),r.app.nameStamp()));CHECK(std::string_view(r.app.renameText()).empty());
 CHECK(!r.app.commitName(std::string("\xc0\xaf",2),r.app.nameStamp()));
 r.text("storage failed iuIU");s3fs::faults.write_bytes=0;r.tap(40);s3fs::faults={};CHECK(r.app.renameOpen()&&r.app.lastOperation.status!=s3app::ds::Status::Ok&&r.app.lastOperation.status!=s3app::ds::Status::Partial);CHECK(savedName(r,original)==old);before.check(r);r.tap(41);
 std::cout<<"PASS duplicate UTF8 length/invalid path/control/UTF8 storage write refusal H/V="<<vertical<<" invariants=1\n";
}}
void imeCapacity(){for(bool vertical:{false,true}){
 RenameRig r;seed(r);r.rename(vertical);const Protected before(r);const auto old=savedName(r,before.id);
 std::string utf8;for(unsigned i=0;i<32;++i)utf8+="是";CHECK(utf8.size()==96);CHECK(r.app.commitName(utf8,r.app.nameStamp()));CHECK(!r.app.commitName("中",r.app.nameStamp()));CHECK(std::string_view(r.app.renameText())==utf8);
 r.clearName();r.text(std::string(95,'a'));r.tap(57);r.settle();r.text("ui");r.settle();r.tap(44);r.settle();
 CHECK(r.app.renameOpen()&&std::string_view(r.app.renameText())==std::string(95,'a'));
 CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect&&!r.ime.raw().empty());before.check(r);
 r.tap(41);CHECK(!r.app.renameOpen()&&!r.ime.active()&&r.ime.raw().empty());CHECK(savedName(r,before.id)==old);before.check(r);
 std::cout<<"PASS UTF8 96-byte boundary, IME overflow rejected without effect, Esc after refusal H/V="<<vertical<<'\n';
}}
void otherDocument(){for(bool vertical:{false,true}){
 RenameRig r;seed(r);const auto current=r.app.session.currentId();CHECK(r.app.session.create("other target",++r.now));const auto target=r.app.session.currentId();CHECK(r.app.session.open(current,++r.now));
 if(vertical)r.tap(61);r.tap(59);for(unsigned n=0;r.app.manager.state().page.items[r.app.manager.state().selected].id!=target&&n<16;++n)r.tap(81);
 CHECK(r.app.manager.state().page.items[r.app.manager.state().selected].id==target);r.tap(21);r.clearName();const Protected before(r);const auto currentName=savedName(r,current);
 r.text("iuIU ");r.tap(57);r.settle();r.text("ui");r.settle();r.tap(44);r.settle();CHECK(std::string_view(r.app.renameText())=="iuIU 是"&&r.app.renameOpen());r.tap(40);CHECK(!r.app.renameOpen());
 CHECK(savedName(r,target)=="iuIU 是"&&savedName(r,current)==currentName);before.check(r);
 std::cout<<"PASS non-current rename target Chinese commit current ID/name/body/content retained H/V="<<vertical<<'\n';
}}
void targetFences(){for(bool vertical:{false,true}){
 RenameRig r;seed(r);r.rename(vertical);const Protected before(r);EditorSink oldBody(r.app);CHECK(oldBody.bind(1,1));r.tap(41);CHECK(!oldBody.commit("leak"));r.tap(21);CHECK(!oldBody.commit("leak"));r.clearName();
 EditorSink stale(r.app);CHECK(stale.bind(2,2));r.text("i");CHECK(!stale.commit("leak"));
 EditorSink selection(r.app);CHECK(selection.bind(3,3));r.tap(4,1);CHECK(!selection.commit("leak"));
 r.tap(41);r.tap(41);EditorSink body(r.app);CHECK(body.bind(4,4));r.tap(59);r.tap(21);CHECK(!body.commit("leak"));r.tap(41);before.check(r);
 std::cout<<"PASS temporary target focus/revision/selection/body isolation H/V="<<vertical<<'\n';
}}
int main(int argc,char**argv){try{
 fs::create_directories(t05_root/"TestBuilds");auto scratch=(t05_root/"TestBuilds/rename-runtime-XXXXXX").string();CHECK(mkdtemp(scratch.data()));fs::current_path(scratch);
 const std::string mode=argc>1?argv[1]:"suite";
 if(mode=="suite"){alphabet();chinese();cancel();validation();imeCapacity();otherDocument();targetFences();std::cout<<"PASS T05 scoped rename suite\n";return 0;}
 for(bool vertical:{false,true}){
  RenameRig r;r.rename(vertical);
  if(mode=="iu"||mode=="all"){
   r.text("iuIU");std::cout<<"OBSERVED layout="<<vertical<<" expected=iuIU actual="<<r.app.renameText()<<" usb_consumed="<<r.usb_consumed<<std::endl;
   CHECK(std::string_view(r.app.renameText())=="iuIU"&&r.usb_consumed==0);
  }
  if(mode=="cn"||mode=="all"){
   r.tap(57);std::cout<<"OBSERVED layout="<<vertical<<" expected_CN=1 actual_CN="<<r.ime.chinese()<<std::endl;
   CHECK(r.ime.chinese());r.settle();r.text("ui");r.settle();CHECK(r.ime.view()->count);CHECK(r.app.document.text().empty());
  }
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
