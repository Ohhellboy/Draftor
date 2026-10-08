// Real P23 App/resources/IME fixture; no alternative editor or presentation.
#define LC1_REPLAY_LIBRARY
#include "../../fixtures/lc1_replay.cpp"
#include "utf8.hpp"
#include <type_traits>

namespace be=editor::body_edit;
using Affinity=hanime::renderer::CaretAffinity;
bool applied(const be::Outcome& x){return x.code==be::ResultCode::Applied||x.code==be::ResultCode::NoAction;}
void drainApp(Rig& r){for(unsigned n=0;r.app.persistence.backgroundSaveActive()||r.app.commandBarrier();++n){CHECK(n<20000);r.app.service(++r.now,false);}}
void saveApp(Rig& r){r.app.key({1,++r.now,22,1,true});drainApp(r);CHECK(r.app.lastOperation);}
void visible(Rig& r){const auto v=r.app.navigator.sync();CHECK(v.cursor_byte==r.app.document.cursorByte());CHECK(v.visual_line>=v.first_visible_line);CHECK(v.visual_line<v.first_visible_line+r.app.layout.visibleTextLineCount());}

void T1(){
 Rig r;const auto a=r.app.sharedState();const auto b=r.app.sharedState();
 static_assert(std::is_const_v<std::remove_reference_t<decltype(a.document)>>);
 static_assert(std::is_const_v<std::remove_reference_t<decltype(a.editor)>>);
 static_assert(!std::is_copy_constructible_v<s3app::App>);
 CHECK(&a.document==&b.document&&&a.document==&r.app.document);
 CHECK(&a.editor==&b.editor&&&a.editor==&r.app.editor);
 CHECK(r.app.commitText("A中文",r.app.editStamp()));
 const auto* body=a.document.text().data();const auto undo=a.editor.undoCount();
 editor::presentation::ViewportState viewport;
 CHECK(r.app.presentation().captureViewport(viewport));r.app.presentation().restoreViewport(viewport);
 CHECK(a.document.text().data()==body&&b.document.text()=="A中文");
 CHECK(a.editor.undoCount()==undo&&b.editor.historyAllocation()==a.editor.historyAllocation());
 r.app.key({1,++r.now,29,1,true});CHECK(a.document.text().empty());
 r.app.key({1,++r.now,28,1,true});CHECK(b.document.text()=="A中文");
 std::cout<<"SIZES App="<<sizeof(s3app::App)<<" BodyEditor="<<sizeof(be::BodyEditor)
          <<" Document="<<sizeof(editor::Document)<<" SharedView="<<sizeof(editor::SharedEditorView)
          <<" Horizontal="<<sizeof(s3app::HorizontalPresentation)<<'\n';
}
void T2(){
 Rig r;const std::string body="A中B文\n第三行";
 CHECK(r.app.commitText(body,r.app.editStamp()));const auto revision=r.app.document.textRevision();
 const auto history=r.app.editor.undoCount();
 for(size_t byte:{0u,1u,4u,5u,8u,9u,12u,15u,18u}){
  CHECK(applied(r.app.moveToByte(byte)));visible(r);
  CHECK(r.app.document.cursorByte()==byte&&r.app.document.text()==body.c_str());
 }
 const auto cursor=r.app.document.cursorByte();const auto stamp=r.app.editStamp();
 CHECK(r.app.moveToByte(2).code==be::ResultCode::InvalidPosition);
 CHECK(r.app.document.cursorByte()==cursor&&r.app.editStamp().revision==stamp.revision);
 CHECK(r.app.editor.undoCount()==history&&r.app.document.textRevision()==revision);
 draftor::memory::allocation_fail_after=0;
 const auto moved=r.app.moveToByte(4);
 draftor::memory::allocation_fail_after=-1;
 CHECK(applied(moved));visible(r);
}
void T3(){
 Rig r;std::string body="abc中文 defghijklmnopqrstuvwxyz long-word 中文中文中文中文 end\n";for(unsigned i=0;i<55;++i)body+="abc中文 defghijk\n";const auto hit=body.size();body+="命中target";
 CHECK(r.app.commitText(body,r.app.editStamp()));const auto revision=r.app.document.textRevision();const auto history=r.app.editor.undoCount();
 CHECK(applied(r.app.moveToByte(1)));CHECK(applied(r.app.moveToByte(6,true)));
 CHECK(r.app.editor.selection().has_selection);
 r.app.key({1,++r.now,9,1,true});CHECK(r.app.findOpen());
 CHECK(r.app.find.setQuery("命中")==hanime::find::QueryEditResult::Applied);
 r.app.key({1,++r.now,40,0,true});
 for(unsigned n=0;r.app.find.view().status==hanime::find::Status::Searching;++n){CHECK(n<150000);r.app.service(++r.now,false);}
 CHECK(r.app.find.view().match.begin==hit&&r.app.document.cursorByte()==hit);
 CHECK(!r.app.editor.selection().has_selection&&r.app.editor.caretAffinity()==Affinity::Downstream);visible(r);
 CHECK(r.app.navigator.firstVisibleLine()>0&&r.app.document.text()==body.c_str());
 r.app.key({1,++r.now,41,0,true});CHECK(r.app.document.cursorByte()==hit+6);
 CHECK(r.app.editor.undoCount()==history&&r.app.document.textRevision()==revision);
 // Same-byte reveal must canonicalize both editor and visible affinity.
 const auto line=r.app.layout.layoutVisualLine(r.app.document.text(),{0,0,true},0,nullptr,0);
 CHECK(line.break_kind==hanime::renderer::VisualLineBreak::SoftWrap&&line.next.valid);
 const auto wrap=line.next.byte_offset;
 CHECK(applied(r.app.moveToByte(editor::previousUtf8Boundary(r.app.document.text(),wrap))));
 r.app.key({1,++r.now,79,0,true});
 CHECK(r.app.document.cursorByte()==wrap&&r.app.editor.caretAffinity()==Affinity::Upstream&&r.app.navigator.caretAffinity()==Affinity::Upstream);
 CHECK(r.app.findDocument.placeCaretAtByte(wrap));
 CHECK(r.app.editor.caretAffinity()==Affinity::Downstream&&r.app.navigator.caretAffinity()==Affinity::Downstream);
 CHECK(!r.app.findDocument.placeCaretAtByte(hit+1));CHECK(r.app.document.cursorByte()==wrap);
}
void T4(){
 Rig r;CHECK(r.app.commitText("A中B文",r.app.editStamp()));CHECK(applied(r.app.moveToByte(1)));
 auto version=r.app.editor.selectionVersion();
 CHECK(applied(r.app.moveToByte(4,true)));auto s=r.app.editor.selection();
 CHECK(s.anchor==1&&s.active==4&&s.begin==1&&s.end==4&&s.version==version+1);
 CHECK(applied(r.app.moveToByte(8,true)));s=r.app.editor.selection();CHECK(s.anchor==1&&s.active==8);
 CHECK(applied(r.app.moveToByte(0,true)));s=r.app.editor.selection();CHECK(s.anchor==1&&s.active==0&&s.begin==0&&s.end==1);
 r.app.key({1,++r.now,79,0,true});CHECK(!r.app.editor.selection().has_selection&&r.app.document.cursorByte()==1);
 r.app.key({1,++r.now,79,2,true});s=r.app.editor.selection();CHECK(s.begin==1&&s.end==4);
 r.app.key({1,++r.now,80,2,true});CHECK(!r.app.editor.selection().has_selection&&r.app.document.cursorByte()==1);
 CHECK(r.app.document.text()=="A中B文"&&r.app.editor.undoCount()==1);
}
void T5(){
 Rig r;CHECK(r.app.commitText("A中文B",r.app.editStamp()));CHECK(applied(r.app.moveToByte(1)));CHECK(applied(r.app.moveToByte(7,true)));
 CHECK(r.app.commitText("X",r.app.editStamp()));CHECK(r.app.document.text()=="AXB"&&r.app.editor.undoCount()==2);
 const auto* history=r.app.editor.historyAllocation();
 r.app.key({1,++r.now,29,1,true});CHECK(r.app.document.text()=="A中文B"&&r.app.editor.selection().begin==1&&r.app.editor.selection().end==7);
 r.app.key({1,++r.now,28,1,true});CHECK(r.app.document.text()=="AXB"&&!r.app.editor.selection().has_selection);
 r.app.key({1,++r.now,29,1,true});r.app.key({1,++r.now,29,1,true});CHECK(r.app.document.text().empty());
 r.app.key({1,++r.now,28,1,true});r.app.key({1,++r.now,28,1,true});CHECK(r.app.document.text()=="AXB");
 CHECK(r.app.editor.historyAllocation()==history&&r.app.editor.undoCount()==2);
}
void T6(){
 LongRig r("phase2a_commit");const auto state=r.app.sharedState();const auto before=state.document.textRevision();
 r.start(false,"ui");const auto item=r.ime.view()->rows[0].items[0];const std::string expected(item.text.data(),item.bytes);
 r.key(44);r.ready();if(!r.accepted){r.key(40);r.ready();}
 CHECK(r.accepted==1&&state.document.text()==expected.c_str());
 CHECK(state.document.textRevision()==before+1&&state.editor.undoCount()==1&&!r.ime.active());
 const auto stamp=r.app.editStamp();auto stale=stamp;++stale.focus;
 CHECK(!r.app.commitText("stale",stale));CHECK(state.document.textRevision()==before+1);
 r.app.key({1,++r.now,29,1,true});CHECK(state.document.text().empty());
 r.app.key({1,++r.now,28,1,true});CHECK(state.document.text()==expected.c_str()&&state.editor.undoCount()==1);
 CHECK(&state.document==&r.app.document&&&state.editor==&r.app.editor);
}
void T7(){
 Rig r;std::string body;for(unsigned i=0;i<90;++i)body+="中abcdefghijk 文12345\n";
 CHECK(r.app.commitText(body,r.app.editStamp()));CHECK(applied(r.app.moveToByte(0)));
 for(unsigned i=0;i<75;++i){r.app.key({1,++r.now,81,0,true});visible(r);}
 for(unsigned i=0;i<17;++i){r.app.key({1,++r.now,82,0,true});visible(r);}
 CHECK(r.app.navigator.firstVisibleLine()>0);const auto cursor=r.app.document.cursorByte();
 editor::presentation::ViewportState saved;CHECK(r.app.presentation().captureViewport(saved));
 const auto id=r.app.session.currentId();saveApp(r);
 CHECK(r.app.session.create("other",++r.now));CHECK(r.app.document.text().empty());
 CHECK(r.app.session.open(id,++r.now));CHECK(r.app.document.text()==body.c_str()&&r.app.document.cursorByte()==cursor);
 editor::presentation::ViewportState restored;CHECK(r.app.presentation().captureViewport(restored));
 CHECK(restored.byte_offset==saved.byte_offset&&restored.affinity==saved.affinity&&restored.layout_revision==1);
 CHECK(r.app.editor.caretAffinity()==saved.affinity&&r.app.editor.undoCount()==0);visible(r);
 const auto source=r.app.document.text().data();r.app.key({1,++r.now,61,0,true});
 CHECK(r.app.document.text().data()==source&&r.app.document.cursorByte()==cursor&&r.app.layout.heightPx()==306);
}
int main(int argc,char** argv){try{
 CHECK(argc==2);const std::string name=argv[1];
 if(name=="T1")T1();else if(name=="T2")T2();else if(name=="T3")T3();else if(name=="T4")T4();else if(name=="T5")T5();else if(name=="T6")T6();else if(name=="T7")T7();else CHECK(false);
 std::cout<<"PHASE2A "<<name<<" OK\n";return 0;
}catch(const std::exception& e){std::cerr<<"PHASE2A FAIL "<<e.what()<<'\n';return 1;}}
