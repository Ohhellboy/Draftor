// Current product input route and explicit expectations for the punctuation
// migration; the accepted donor's original fixtures remain in its own tree.
// All three schemes use the candidate's frozen, staged production resources.
#include <filesystem>
#include <string>
static const std::filesystem::path alpha_candidate_root =
    std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
static const std::string alpha_resource_root =
    (alpha_candidate_root / "resources" / "staged").string();
#undef RESOURCE_ROOT
#undef INDEX_ROOT
#define RESOURCE_ROOT alpha_resource_root.c_str()
#define INDEX_ROOT alpha_resource_root.c_str()
#define main alpha_unused_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
#include "support/app_test_access.hpp"

namespace {
using Rules = hanime_extension::SchemeRules;
struct AlphaRig : Rig {
 s3app::ProductInput product;
 void tap(uint8_t usage, uint8_t mods=0) {
  for(bool down:{true,false}) {
   s3hid::Event e{1,++now,usage,mods,down,0,frame,ime.attemptSerial()};
   e.ime_context=ime.ingressContext();e.resource_owner=ime.resourceOwner();
   if(product.layout(app,e))continue;
   const bool chinese=ime.chinese(); // Same occurrence-time snapshot as main.
   if(!product.ime(app,&ime,e))app.key(e,chinese);
  }
 }
 void type(std::string_view s){for(char c:s)tap(uint8_t(c-'a'+4));}
 void cn(unsigned scheme=0) {
  tap(57);settle();CHECK(ime.chinese());
  for(unsigned i=0;i<scheme;++i){tap(44,1);settle();}
  CHECK(resources.rules()==(scheme==0?Rules::R1Xiaohe:scheme==1?Rules::FullPinyin:Rules::DirectExact));
 }
 void layout(bool vertical){if(app.horizontalActive()==vertical)tap(61);CHECK(app.horizontalActive()!=vertical);}
 void begin(unsigned scheme,std::string_view raw,bool vertical=false){cn(scheme);layout(vertical);type(raw);settle();}
 void pump(){for(unsigned i=0;i<10000;++i){app.service(++now,false);if(!app.commandBarrier()&&!app.persistence.backgroundSaveActive())return;}CHECK(false);}
 void save(){tap(22,1);pump();CHECK(!app.document.dirty());}
};

struct Mapping{uint8_t usage,mods;const char* chinese;const char* english;};
// UK HID inputs and independently written expected UTF-8 bytes.
constexpr Mapping mappings[]={
 {54,0,"，",","},{55,0,"。","."},{51,0,"；",";"},{51,2,"：",":"},
 {30,2,"！","!"},{56,2,"？","?"},{38,2,"（","("},{39,2,"）",")"},
 {100,0,"、","\\"},{47,0,"【","["},{48,0,"】","]"},
 {54,2,"《","<"},{55,2,"》",">"},{47,2,"「","{"},{48,2,"」","}"},
 {35,2,"……","^"},{45,2,"——","_"},
 {47,4,"“",""},{48,4,"”",""},{47,6,"‘",""},{48,6,"’",""},
 {54,6,"『",""},{55,6,"』",""}
};

void allMappings() {
 for(bool vertical:{false,true})for(unsigned scheme:{0u,1u,2u}) {
  AlphaRig r;r.cn(scheme);r.layout(vertical);std::string expected;
  for(const auto& m:mappings) {
   const auto revision=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
   r.tap(m.usage,m.mods);expected+=m.chinese;
   CHECK(r.app.document.text()==std::string_view(expected));
   CHECK(r.app.document.cursorByte()==expected.size());
   CHECK(r.app.document.textRevision()==revision+1&&r.app.editor.undoCount()==undo+1);
   CHECK(r.ime.raw().empty());
  }
  // R2 migration: raw apostrophe and Enter-near # positions now have CN
  // semantics. The earlier explicit aliases inserted two quotes per family,
  // so these primary quote keys see even prefixes and insert opening sides.
  // UK Shift+2 is intentionally still ASCII: this is no whole-layout rewrite.
  for(const auto& m:{Mapping{52,2,"“","@"},Mapping{50,0,"、","#"},
                     Mapping{52,0,"‘","'"},Mapping{31,2,"\"","\""},
                     Mapping{56,0,"/","/"},Mapping{30,0,"1","1"},
                     Mapping{44,0," "," "}}) {
   r.tap(m.usage,m.mods);expected+=m.chinese;
   CHECK(r.app.document.text()==std::string_view(expected)&&r.ime.raw().empty());
  }
  const auto revision=r.app.document.textRevision();const auto cursor=r.app.document.cursorByte();
  r.tap(57);CHECK(!r.ime.chinese());
  CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.textRevision()==revision&&r.app.document.cursorByte()==cursor);
  for(const auto& m:mappings){r.tap(m.usage,m.mods);expected+=m.english;CHECK(r.app.document.text()==std::string_view(expected));}
  r.tap(8,0x40);expected+="€"; // UK AltGr+E is not a new quote chord.
  CHECK(r.app.document.text()==std::string_view(expected));
  std::cout<<"PASS alpha mapping scheme="<<scheme<<" vertical="<<vertical<<" CN/EN complete table, one insertion, UK AltGr, unlisted ASCII\n";
 }
}

void candidateOwnership() {
 for(bool vertical:{false,true})for(unsigned scheme:{0u,1u}) {
  AlphaRig r;r.begin(scheme,scheme?"shi":"ui",vertical);
  const std::string raw(r.ime.raw());const auto input=r.ime.composition()->raw_input().stamp;
  for(const auto& m:{Mapping{54,0,"",""},Mapping{51,2,"",""},Mapping{47,4,"",""},Mapping{52,0,"",""},Mapping{52,2,"",""}}) {
   r.tap(m.usage,m.mods);
   CHECK(r.app.document.text().empty()&&r.ime.raw()==raw);
   CHECK(r.ime.composition()->input_is_current(input));
  }
  // Ctrl+, is not punctuation; save is owned/blocked by the active IME.
  r.tap(54,1);r.tap(22,1);CHECK(r.app.document.text().empty()&&r.ime.raw()==raw);
  r.tap(46);r.settle();CHECK(r.app.document.text().empty()&&r.ime.raw()==raw);
  r.tap(45);r.settle();CHECK(r.app.document.text().empty()&&r.ime.raw()==raw);
  r.tap(41);r.settle();CHECK(r.ime.rawEditing()&&r.ime.raw()==raw);
  r.tap(41);r.settle();CHECK(r.ime.raw().empty()&&!r.ime.rawEditing());
  r.type(scheme?"shi":"ui");r.settle();
  const auto item=r.ime.view()->rows[0].items[0];const std::string selected(item.text.data(),item.bytes);
  r.tap(30);r.tap(54);r.settle();
  CHECK(r.app.document.text()==std::string_view(selected+"，")&&r.ime.raw().empty());
  std::cout<<"PASS alpha candidate ownership, blocked punctuation/chords, page keys, numbered selection and queued Chinese comma scheme="<<scheme<<" vertical="<<vertical<<'\n';
 }
 for(bool vertical:{false,true})for(unsigned scheme:{0u,1u}) {
  AlphaRig r;r.begin(scheme,scheme?"wandanle":"wjdjle",vertical);
  r.tap(30);r.tap(55);r.settle();
  CHECK(r.app.document.text().empty()&&r.ime.selected()=="完蛋");
  const std::string raw(r.ime.raw());r.tap(54);CHECK(r.app.document.text().empty()&&r.ime.raw()==raw);
  r.tap(44);r.tap(55);r.settle();
  CHECK(r.app.document.text()=="完蛋了。"&&r.ime.raw().empty());
 }
 // Raw-edit focus consumes punctuation without modifying raw, selected text,
 // cursor or body. The physical raw-entry key remains the product Left key.
 for(bool vertical:{false,true}) {
  AlphaRig r;r.begin(0,"wjdjle",false);r.tap(80);CHECK(r.ime.rawEditing());r.layout(vertical);
  const std::string raw(r.ime.raw()),selected(r.ime.selected());const auto cursor=r.ime.rawCursor();
  r.tap(54);r.tap(47,4);r.tap(52);r.tap(52,2);
  CHECK(r.ime.raw()==raw&&r.ime.selected()==selected&&r.ime.rawCursor()==cursor&&r.app.document.text().empty());
 }
 // R2 migration: UK @ remains available in EN; switching to CN does not
 // create a temporary-English state or reinterpret that existing body byte.
 {AlphaRig r;r.tap(52,2);r.cn();r.type("a");CHECK(r.app.document.text()=="@"&&r.ime.raw()=="a");}
 std::cout<<"PASS alpha partial composition, raw editing and existing @ boundary retain input\n";
}

void shortTextCapacity() {
 for(const auto& m:{Mapping{35,2,"……",""},Mapping{45,2,"——",""}})for(size_t room:{size_t(0),size_t(3),size_t(5),size_t(6)}) {
  AlphaRig r;r.cn();const auto maximum=r.app.document.maxBytes();
  while(r.app.document.text().size()<maximum-room){
   const size_t amount=std::min(size_t(4096),maximum-room-r.app.document.text().size());
   CHECK(r.app.commitText(std::string(amount,'x'),r.app.editStamp()));
  }
  const std::string before(r.app.document.text());const auto revision=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
  r.tap(m.usage,m.mods);
  if(room<6){
   CHECK(r.app.document.text()==std::string_view(before)&&r.app.document.cursorByte()==before.size());
   CHECK(r.app.document.textRevision()==revision&&r.app.editor.undoCount()==undo);
  }else{
   CHECK(r.app.document.text()==std::string_view(before+m.chinese)&&r.app.document.cursorByte()==maximum);
   CHECK(r.app.document.textRevision()==revision+1&&r.app.editor.undoCount()==undo+1);
   r.tap(42);CHECK(r.app.document.text().size()==maximum-3); // Existing codepoint deletion, not paired deletion.
   r.tap(42);CHECK(r.app.document.text()==std::string_view(before));
  }
 }
 // Existing candidate/mark transaction boundary: body capacity failure must
 // retain the unfinished selected candidate, with no queued punctuation leak.
 {AlphaRig r;r.cn();const auto maximum=r.app.document.maxBytes();
  while(r.app.document.text().size()<maximum-1){const auto amount=std::min(size_t(4096),maximum-1-r.app.document.text().size());CHECK(r.app.commitText(std::string(amount,'x'),r.app.editStamp()));}
  r.type("ui");r.settle();const std::string raw(r.ime.raw());r.tap(44);r.tap(54);r.settle();
  CHECK(r.ime.receipt().status==hanime::CompositionStatus::RetryableNoEffect);
  CHECK(r.ime.raw()==raw&&r.ime.selected()=="是"&&r.app.document.text().size()==maximum-1&&r.ime.queuedInput()==0);
 }
 std::cout<<"PASS alpha short UTF-8 0/3/5/6-byte capacity, atomic revision/undo, per-codepoint deletion, candidate failure retention\n";
}

void fieldsAndPersistence() {
 AlphaRig r;r.cn();for(const auto& m:mappings)r.tap(m.usage,m.mods);
 const std::string expected(r.app.document.text());r.save();
 const auto revision=r.app.document.textRevision();const auto cursor=r.app.document.cursorByte();
 for(unsigned i=0;i<4;++i){r.tap(61);CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.textRevision()==revision&&r.app.document.cursorByte()==cursor&&!r.app.document.dirty());}
 r.tap(9,1);CHECK(r.app.findOpen());r.tap(54);r.tap(55);r.tap(47);r.tap(48);
 CHECK(r.app.find.view().query==",.[]");CHECK(r.app.document.text()==std::string_view(expected));
 r.tap(41);CHECK(!r.app.findOpen());
 r.tap(59);CHECK(r.app.filesOpen());r.tap(21);CHECK(r.app.renameOpen());
 r.tap(4,1);r.tap(54);r.tap(55);r.tap(47);r.tap(48);
 CHECK(std::string_view(r.app.renameText())==",.[]"&&r.app.document.text()==std::string_view(expected));
 r.tap(41);CHECK(!r.app.renameOpen());r.tap(41);CHECK(!r.app.filesOpen());
 // Unassigned modifier combinations do not escape as translated characters.
 for(uint8_t mods:{uint8_t(1),uint8_t(8),uint8_t(5),uint8_t(12),uint8_t(64)})r.tap(54,mods);
 CHECK(r.app.document.text()==std::string_view(expected));
 const auto saved=r.app.session.currentId();CHECK(r.app.session.create({},++r.now));
 CHECK(r.app.document.text().empty());CHECK(r.app.session.open(saved,++r.now));
 CHECK(r.app.document.text()==std::string_view(expected)&&!r.app.document.dirty());
 // Unicode editor movement/deletion uses byte boundaries after full marks.
 CHECK(r.app.moveToByte(3).code==editor::body_edit::ResultCode::Applied);
 r.tap(42);CHECK(r.app.document.text()==std::string_view(expected).substr(3)&&r.app.document.cursorByte()==0);
 r.tap(54);CHECK(r.app.document.text()==std::string_view(expected)&&r.app.document.cursorByte()==3);
 std::cout<<"PASS alpha layout clean-body invariance, Find/rename isolation, modifier ownership, save/reopen ordinary UTF-8 and cursor/delete/reinsert\n";
}
} // namespace

int main(){try{
 // Rig's existing relative scratch directories stay wholly in this candidate.
 const auto scratch_parent=alpha_candidate_root/"TestBuilds";
 fs::create_directories(scratch_parent);
 auto scratch=(scratch_parent/"alpha_input_runtime-XXXXXX").string();
 CHECK(mkdtemp(scratch.data()));fs::current_path(scratch);
 allMappings();candidateOwnership();shortTextCapacity();fieldsAndPersistence();
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);
 std::cout<<"PASS alpha_punctuation_input complete; temporary-English and apostrophe separator remain donor-unimplemented boundaries\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
