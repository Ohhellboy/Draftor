#pragma once
#include "memory_storage.hpp"
#include "product_config.hpp"
#include "presentation_controller.hpp"
#include "hanime_document_editor_port.hpp"
#include "src/hanime_document/document_session.hpp"
#include "src/hanime_document/entropy_id_source.hpp"
#include "src/hanime_document/paths.hpp"

// Synthetic metrics only. The H/V presentation, navigator and viewport
// implementations are real R2. No glyph loading or display is exercised.
struct MetricFont final : s3host::FontPort {
  s3host::GlyphView glyph(std::uint32_t cp) const noexcept override {
    s3host::GlyphView g;g.advance=cp<128?14:28;return g;
  }
  std::uint16_t lineHeight() const noexcept override { return 34; }
  std::uint16_t defaultAdvance() const noexcept override { return 14; }
};
inline bool ok(hd::Status s) { return s==hd::Status::Ok||s==hd::Status::OkDegraded; }
inline std::string controlText() {
  std::string text;
  for(unsigned c=1;c<32;++c)text.push_back(static_cast<char>(c));
  text.push_back('\x7f');
  for(unsigned c=0x80;c<=0x9f;++c){text.push_back('\xc2');text.push_back(static_cast<char>(c));}
  return text;
}
inline std::string referenceExport(std::string_view input) {
  std::string output;
  for(std::size_t at=0;at<input.size();++at){
    if(input[at]!='\r')output.push_back(input[at]);
    else {output.push_back('\n');if(at+1<input.size()&&input[at+1]=='\n')++at;}
  }
  return output;
}
struct R2Fixture {
  MemoryStorage& storage;
  editor::Document document{draftor::product::document()};
  editor::body_edit::BodyEditor body{document,draftor::product::editor()};
  MetricFont font;
  s3app::HorizontalPresentation horizontal{document,font};
  s3app::VerticalPresentation vertical{{document,body},font};
  s3app::PresentationController presentation{{document,body},horizontal,vertical};
  unsigned installs=0, activations=0;
  static void installed(void* p) noexcept {
    auto& f=*static_cast<R2Fixture*>(p);++f.installs;f.body.resetForDocument(false);
  }
  static void activated(void* p) noexcept {
    auto& f=*static_cast<R2Fixture*>(p);++f.activations;f.body.resetForDocument(true);
  }
  hd::integration::HanimeDocumentEditorPort port{document,body,presentation,
      {nullptr,nullptr,installed,this,activated}};
  hd::EntropyIdSource ids{{1,2,3,4},0x20260925};
  hd::DocumentCatalog catalog{storage,ids};
  hd::PersistenceService persistence{storage,catalog,{},draftor::product::save()};
  hd::DocumentSession session{catalog,persistence,port};
  std::uint32_t clock=1;
  std::size_t save_steps=0;
  explicit R2Fixture(MemoryStorage& medium,bool create=true) : storage(medium) {
    CHECK(body.ready());CHECK(ok(catalog.initialize()));
    if(create){const auto r=session.create("fixture",clock);CHECK(ok(r.status));}
    else CHECK(ok(session.startupRestore().status));
    CHECK(session.hasCurrent());
  }
  void setBody(std::string_view text) {
    // Fixture initialization only, never called by the export adapter.
    CHECK(document.replaceAll(text,0,true));body.resetForDocument();
    presentation.followCaret(body.caretAffinity());session.onBodyMutation(++clock);
    session.onPositionChanged(clock);
  }
  void edit(std::string_view text) {
    CHECK(body.insertUtf8(text).textChanged());session.onBodyMutation(++clock);
    presentation.followCaret(body.caretAffinity());session.onPositionChanged(clock);
  }
  hd::OperationResult drainSave() {
    hd::OperationResult result;
    for(unsigned n=0;n<10000&&persistence.backgroundSaveActive();++n){
      result=persistence.tick(++clock,{true,false,false},port);++save_steps;
      if(persistence.backgroundSaveActive())CHECK(result.status==hd::Status::Busy);
    }
    CHECK(!persistence.backgroundSaveActive());CHECK(storage.handle.empty());return result;
  }
  hd::OperationResult saveAuto() {
    const auto start=persistence.startAutoSave(++clock,port);
    CHECK(start.status==hd::Status::Busy&&persistence.backgroundSaveActive());
    return drainSave();
  }
  std::string text() const { return std::string(document.text().data(),document.text().size()); }
  std::map<std::string,std::string> records(std::string_view component) const {
    std::map<std::string,std::string> result;
    for(const auto& entry:storage.files)if(entry.first.find(component)!=std::string::npos)result.insert(entry);
    return result;
  }
};

struct EditorEvidence {
  std::string text;
  hd::DocumentId id;
  std::uint64_t session=0, revision=0, body_revision=0;
  std::size_t cursor=0, undo=0, redo=0, history=0;
  editor::body_edit::SelectionView selection;
  editor::presentation::ViewportState viewport;
  s3app::PresentationFrame frame;
  unsigned installs=0,activations=0;
  std::map<std::string,std::string> current,manual;
  explicit EditorEvidence(R2Fixture& f) : text(f.text()),id(f.session.currentId()),
      session(f.session.sessionSerial()),revision(f.port.revision()),body_revision(f.port.bodyRevision()),
      cursor(f.document.cursorByte()),undo(f.body.undoCount()),redo(f.body.redoCount()),
      history(f.body.historyBytesUsed()),selection(f.body.selection()),frame(f.presentation.frame()),
      installs(f.installs),activations(f.activations),current(f.records("/CURRENT")),manual(f.records("/MANUAL")) {
    CHECK(f.presentation.captureViewport(viewport));
  }
  void unchanged(R2Fixture& f) const {
    CHECK(bytesEqual(text,f.text()));CHECK(f.session.currentId()==id&&f.session.sessionSerial()==session);
    CHECK(f.port.revision()==revision&&f.port.bodyRevision()==body_revision&&f.document.cursorByte()==cursor);
    CHECK(f.body.undoCount()==undo&&f.body.redoCount()==redo&&f.body.historyBytesUsed()==history);
    const auto s=f.body.selection();
    CHECK(s.begin==selection.begin&&s.end==selection.end&&s.anchor==selection.anchor&&s.active==selection.active);
    CHECK(s.version==selection.version&&s.has_selection==selection.has_selection&&s.active_affinity==selection.active_affinity);
    editor::presentation::ViewportState v;CHECK(f.presentation.captureViewport(v));
    CHECK(v.byte_offset==viewport.byte_offset&&v.affinity==viewport.affinity&&v.layout_revision==viewport.layout_revision);
    CHECK(f.presentation.current(frame)&&f.installs==installs&&f.activations==activations);
    CHECK(f.records("/CURRENT")==current&&f.records("/MANUAL")==manual);
  }
};
