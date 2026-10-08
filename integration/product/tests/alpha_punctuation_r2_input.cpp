// Physical UK HID events through the existing product route. Expected text is
// explicit and independent of the new CN-only semantic resolver.
#include <filesystem>
#include <string>
static const std::filesystem::path r2_candidate_root=
 std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
static const std::string r2_resource_root=(r2_candidate_root/"resources"/"staged").string();
#undef RESOURCE_ROOT
#undef INDEX_ROOT
#define RESOURCE_ROOT r2_resource_root.c_str()
#define INDEX_ROOT r2_resource_root.c_str()
#define main r2_unused_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"

namespace {
struct R2Rig:Rig {
 s3app::ProductInput product;
 void tap(uint8_t usage,uint8_t mods=0){
  for(bool down:{true,false}){
   s3hid::Event e{1,++now,usage,mods,down,0,frame,ime.attemptSerial()};
   e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();
   if(product.layout(app,e))continue;
   const bool chinese=ime.chinese();
   if(!product.ime(app,&ime,e))app.key(e,chinese);
  }
 }
 void type(std::string_view s){for(char c:s)tap(uint8_t(c-'a'+4));}
 void cn(unsigned scheme=0){tap(57);settle();CHECK(ime.chinese());for(unsigned i=0;i<scheme;++i){tap(44,1);settle();}
  using R=hanime_extension::SchemeRules;CHECK(resources.rules()==(scheme==0?R::R1Xiaohe:scheme==1?R::FullPinyin:R::DirectExact));}
 void layout(bool vertical){if(app.horizontalActive()==vertical)tap(61);CHECK(app.horizontalActive()!=vertical);}
 void begin(unsigned scheme,bool vertical){cn(scheme);layout(vertical);type(scheme==0?"ui":scheme==1?"shi":"a");settle();}
 void save(){tap(22,1);for(unsigned i=0;i<10000;++i){app.service(++now,false);if(!app.commandBarrier()&&!app.persistence.backgroundSaveActive())break;}CHECK(!app.document.dirty());}
 void move(size_t byte,bool select=false){const auto r=app.moveToByte(byte,select);CHECK(r.code==editor::body_edit::ResultCode::Applied||r.code==editor::body_edit::ResultCode::NoAction);CHECK(app.document.cursorByte()==byte);}
};
struct RawCase{uint8_t usage;bool shift;const char* cn;const char* en;};
constexpr RawCase primary[]={
 {54,false,"，",","},{55,false,"。","."},{51,false,"；",";"},{51,true,"：",":"},
 {30,true,"！","!"},{56,false,"/","/"},{56,true,"？","?"},
 {38,true,"（","("},{39,true,"）",")"},{47,false,"【","["},{48,false,"】","]"},
 {54,true,"《","<"},{55,true,"》",">"},{47,true,"「","{"},{48,true,"」","}"},
 {49,false,"、","#"},{50,false,"、","#"},{100,false,"、","\\"},
 {49,true,"~","~"},{50,true,"~","~"},{100,true,"|","|"},
 {35,true,"……","^"},{45,true,"——","_"},
 {52,false,"‘","'"},{52,false,"’","'"},{52,true,"“","@"},{52,true,"”","@"},
 {31,true,"\"","\""}
};

void rawMatrix(){
 for(bool vertical:{false,true})for(unsigned scheme:{0u,1u,2u}){
  R2Rig r;r.cn(scheme);r.layout(vertical);std::string expected;
  for(uint8_t shift:{uint8_t(2),uint8_t(32)})for(const auto& c:primary){
   const auto revision=r.app.document.textRevision();r.tap(c.usage,c.shift?shift:0);expected+=c.cn;
   CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==expected.size());
   CHECK(r.app.document.textRevision()==revision+1&&r.ime.raw().empty());
  }
  const auto revision=r.app.document.textRevision();r.tap(57);CHECK(!r.ime.chinese());
  CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.textRevision()==revision);
  for(uint8_t shift:{uint8_t(2),uint8_t(32)})for(const auto& c:primary){r.tap(c.usage,c.shift?shift:0);expected+=c.en;CHECK(r.app.document.text()==std::string_view(expected));}
  std::cout<<"PASS R2 raw HID CN/EN table, left/right Shift, H/V scheme="<<scheme<<" vertical="<<vertical<<'\n';
 }
}

void englishText(R2Rig& r,std::string_view text){
 // This is the known UK physical sequence, not the product's UK translator.
 for(char c:text){
  if(c>='a'&&c<='z'){r.tap(uint8_t(c-'a'+4));continue;}
  if(c>='A'&&c<='Z'){r.tap(uint8_t(c-'A'+4),2);continue;}
  if(c>='1'&&c<='9'){r.tap(uint8_t(c-'1'+30));continue;}
  switch(c){case '0':r.tap(39);break;case ' ':r.tap(44);break;
   case ',':r.tap(54);break;case '.':r.tap(55);break;case '!':r.tap(30,2);break;
   case '\'':r.tap(52);break;case '@':r.tap(52,2);break;default:CHECK(false);}
 }
}
void englishAndModifiers(){
 for(bool vertical:{false,true}){
  R2Rig r;r.layout(vertical);const std::string reference="Hello, world! Don't change v1.2, test@example.com or 3.14.";
  englishText(r,reference);CHECK(r.app.document.text()==std::string_view(reference));std::string expected=reference;
  for(uint8_t shift:{uint8_t(2),uint8_t(32)}){
   for(uint8_t u=30;u<=39;++u)r.tap(u,shift);expected+="!\"£$%^&*()";
   CHECK(r.app.document.text()==std::string_view(expected));
  }
  r.tap(49);r.tap(50);r.tap(100);r.tap(8,64);expected+="##\\€";CHECK(r.app.document.text()==std::string_view(expected));
  // Ctrl/GUI chords, AltGr and mixed modifiers never become CN marks.
  r.cn();
  for(uint8_t mods:{uint8_t(1),uint8_t(16),uint8_t(8),uint8_t(128),uint8_t(4),uint8_t(64),uint8_t(5),uint8_t(12),uint8_t(9),uint8_t(66)})
   for(uint8_t usage:{uint8_t(49),uint8_t(50),uint8_t(52),uint8_t(100)}){
    r.tap(usage,mods);CHECK(r.app.document.text()==std::string_view(expected));
   }
  r.tap(8,64);expected+="€";CHECK(r.app.document.text()==std::string_view(expected));
  // Retained explicit aliases coexist with parity without adding hidden state.
  r.tap(47,4);r.tap(48,4);r.tap(47,6);r.tap(48,6);r.tap(54,6);r.tap(55,6);expected+="“”‘’『』";
  CHECK(r.app.document.text()==std::string_view(expected));
  r.tap(52,2);r.tap(52);expected+="“‘";CHECK(r.app.document.text()==std::string_view(expected));
 }
 std::cout<<"PASS R2 exact English reference, UK shifted numbers/#/ISO/AltGr, shortcut isolation and secondary aliases\n";
}

void parity(){
 for(bool vertical:{false,true})for(bool double_quote:{false,true}){
  R2Rig r;r.cn();r.layout(vertical);const uint8_t mods=double_quote?2:0;
  const std::string left=double_quote?"“":"‘",right=double_quote?"”":"’";
  std::string expected;
  for(unsigned i=0;i<4;++i){r.tap(52,mods);expected+=(i%2?right:left);CHECK(r.app.document.text()==std::string_view(expected));}
  r.tap(42);expected.resize(expected.size()-3);CHECK(r.app.document.text()==std::string_view(expected));
  r.tap(52,mods);expected+=right;CHECK(r.app.document.text()==std::string_view(expected));
  r.tap(29,1);expected.resize(expected.size()-3);CHECK(r.app.document.text()==std::string_view(expected));
  r.tap(52,mods);expected+=right;CHECK(r.app.document.text()==std::string_view(expected));
  r.move(3);r.tap(52,mods);expected.insert(3,right);CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==6);
  r.tap(76);expected.erase(6,3);CHECK(r.app.document.text()==std::string_view(expected));
  r.move(0);r.tap(52,mods);expected.insert(0,left);CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==3);
  r.save();const auto id=r.app.session.currentId();const auto caret=r.app.document.cursorByte();
  CHECK(r.app.session.create({},++r.now));CHECK(r.app.session.open(id,++r.now));
  CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==caret);
  r.tap(52,mods);expected.insert(caret,right);CHECK(r.app.document.text()==std::string_view(expected));
 }
 // Count both opening and closing forms, separately for each quote family.
 {R2Rig r;r.cn();CHECK(r.app.commitText("”’",r.app.editStamp()));r.tap(52,2);r.tap(52);CHECK(r.app.document.text()=="”’”’");r.tap(52,2);r.tap(52);CHECK(r.app.document.text()=="”’”’“‘");}
 std::cout<<"PASS R2 quote parity from actual caret prefix, delete/forward-delete/Undo/move/save-reopen, independent families\n";
}

void selectionCaret(){
 for(bool vertical:{false,true})for(bool double_quote:{false,true})for(bool reverse:{false,true}){
  R2Rig r;r.cn();r.layout(vertical);const uint8_t mods=double_quote?2:0;
  const std::string left=double_quote?"“":"‘",right=double_quote?"”":"’";
  CHECK(r.app.commitText(left+"x"+right,r.app.editStamp()));
  r.move(reverse?4:0);r.move(reverse?0:4,true);
  const auto selection=r.app.editor.selection();CHECK(selection.has_selection&&selection.begin==0&&selection.end==4);
  CHECK(r.app.document.cursorByte()==(reverse?0u:4u));
  r.tap(52,mods);const std::string expected=(reverse?left:right)+right;
  CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==3&&!r.app.editor.selection().has_selection);
 }
 std::cout<<"PASS R2 selection replacement counts actual caret prefix, not normalized range begin, H/V both families\n";
}

void failedQuote(){
 for(bool double_quote:{false,true}){
  R2Rig r;r.cn();const uint8_t mods=double_quote?2:0;
  const std::string left=double_quote?"“":"‘",right=double_quote?"”":"’";
  CHECK(r.app.document.maxBytes()==65536);r.tap(52,mods);CHECK(r.app.document.text()==std::string_view(left));
  while(r.app.document.text().size()<65535){const auto amount=std::min(size_t(4096),size_t(65535)-r.app.document.text().size());CHECK(r.app.commitText(std::string(amount,'x'),r.app.editStamp()));}
  const std::string before(r.app.document.text());const auto revision=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
  for(unsigned i=0;i<2;++i){r.tap(52,mods);CHECK(r.app.document.text()==std::string_view(before)&&r.app.document.cursorByte()==65535&&r.app.document.textRevision()==revision&&r.app.editor.undoCount()==undo);}
  r.tap(42);r.tap(42);r.tap(52,mods);
  CHECK(r.app.document.text()==std::string_view(before.substr(0,before.size()-2)+right)&&r.app.document.cursorByte()==65536);
 }
 std::cout<<"PASS R2 quote 65535-byte capacity rejection is atomic and retry parity has no hidden state\n";
}

void newKeyOwnership(){
 for(bool vertical:{false,true})for(unsigned scheme:{0u,1u,2u}){
  R2Rig r;r.begin(scheme,vertical);const std::string raw(r.ime.raw());const auto stamp=r.ime.composition()->raw_input().stamp;
  for(uint8_t usage:{uint8_t(49),uint8_t(50),uint8_t(100),uint8_t(52)})for(uint8_t mods:{uint8_t(0),uint8_t(2),uint8_t(32)}){
   r.tap(usage,mods);CHECK(r.ime.raw()==raw&&r.app.document.text().empty()&&r.ime.composition()->input_is_current(stamp));
  }
  r.tap(41);CHECK(r.ime.rawEditing()&&r.ime.raw()==raw);const auto cursor=r.ime.rawCursor();
  r.tap(52);r.tap(52,2);r.tap(49);r.tap(50);r.tap(100);
  CHECK(r.ime.raw()==raw&&r.ime.rawCursor()==cursor&&r.app.document.text().empty());
  r.tap(41);CHECK(!r.ime.rawEditing()&&r.ime.raw().empty());
  r.type(scheme==0?"ui":scheme==1?"shi":"a");r.settle();
  const auto item=r.ime.view()->rows[0].items[0];const std::string selected(item.text.data(),item.bytes);
  r.tap(44);r.tap(54);r.tap(55);r.settle();CHECK(r.app.document.text()==std::string_view(selected+"，。")&&r.ime.raw().empty());
 }
 for(unsigned scheme:{0u,1u}){
  R2Rig r;r.cn(scheme);r.type(scheme?"wandanle":"wjdjle");r.settle();r.tap(30);r.settle();
  const std::string raw(r.ime.raw()),selected(r.ime.selected());CHECK(!raw.empty()&&!selected.empty()&&r.app.document.text().empty());
  r.tap(52);r.tap(52,2);r.tap(49);r.tap(50);r.tap(100);
  CHECK(r.ime.raw()==raw&&r.ime.selected()==selected&&r.app.document.text().empty());
 }
 std::cout<<"PASS R2 new raw punctuation preserves candidate/raw/partial ownership and Esc ladder; existing Accepted-only comma/period successor\n";
}

void modalIsolation(){
 R2Rig r;r.cn();CHECK(r.app.commitText("正文",r.app.editStamp()));r.save();
 r.tap(9,1);CHECK(r.app.findOpen());r.tap(52);r.tap(52,2);r.tap(49);r.tap(50);r.tap(100);
 CHECK(r.app.find.view().query=="'@##\\"&&r.app.document.text()=="正文");r.tap(41);
 r.tap(59);r.tap(21);CHECK(r.app.renameOpen());r.tap(4,1);r.tap(52);r.tap(52,2);r.tap(49);r.tap(50);r.tap(100);
 CHECK(std::string_view(r.app.renameText())=="'@##\\"&&r.app.document.text()=="正文");r.tap(41);r.tap(41);
 CHECK(!r.app.renameOpen()&&!r.app.filesOpen()&&r.ime.raw().empty());
 std::cout<<"PASS R2 Find/Rename retain UK raw-key semantics without new CN interception\n";
}
} // namespace

int main(){try{
 const auto scratch_parent=r2_candidate_root/"TestBuilds";fs::create_directories(scratch_parent);
 auto scratch=(scratch_parent/"alpha_r2_input_runtime-XXXXXX").string();CHECK(mkdtemp(scratch.data()));fs::current_path(scratch);
 rawMatrix();englishAndModifiers();parity();selectionCaret();failedQuote();newKeyOwnership();modalIsolation();
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS alpha_punctuation_r2_input; host-only production events, no device claims\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
