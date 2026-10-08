#pragma once
#include "body_editor.hpp"
#include "candidate_visual.hpp"
#include "name_input.hpp"
#include "find_controller.hpp"
#include "document_stats_adapter.hpp"
#include "presentation_controller.hpp"
#include "font_port.hpp"
#include "hid_keyboard.hpp"
#include "hanime_document_editor_port.hpp"
#include "src/hanime_document/entropy_id_source.hpp"
#include "src/hanime_document/file_manager_model.hpp"
namespace s3app {
namespace ds=hanime::documents;
struct EditStamp {uint64_t focus=0;uint32_t revision=0,selection=0;};
enum class Command:uint8_t{None,Save,Files,New,OpenSelected,PreviousPage,NextPage,ToggleTrash,Trash,RestoreManual,RestoreLatest,RestoreProtection,Confirm};
// Retained explicit construction seam for the Phase 2B geometry fixtures.
struct PresentationConstruction {
 editor::presentation::PresentationPort& (*construct)(void*,editor::SharedEditorView)=nullptr;
 void* context=nullptr;
};
class App {
 public:
 explicit App(ds::StoragePort& storage,std::array<uint8_t,16> uid,uint64_t entropy,const s3host::FontPort& font,PresentationConstruction construction={});
 ds::OperationResult startup(uint32_t now);
 // Per-operation mode captured before IME dispatch; never persisted in text.
 void key(const s3hid::Event& e,bool chinese_punctuation=false);
 void service(uint32_t now,bool input_pending);
 // Busy/manual_save_accepted means ownership passed to this App. Input epoch
 // invalidation cannot revoke it; media/document failures are reported visibly.
 ds::OperationResult requestSave(uint32_t now);
 bool manualSavePending()const noexcept{return manualSave_.pending;}

 void disconnected() noexcept;
 void allocationFailed();
 bool commandBarrier()const noexcept{return queuedFile_!=Command::None||pending_!=Command::None||session.creating();}
 bool ready()const noexcept{return ready_;}
 bool transferActive()const noexcept{return transfer_active_;}
 bool enterTransfer()noexcept;
 void leaveTransfer()noexcept;
 void transferBoundary()noexcept{++focusEpoch_;dirtyDisplay=true;}
 void transferSaveResult(ds::OperationResult);
 ds::OperationResult openImported(const ds::DocumentId&,uint32_t now);
 void setProgressHook(void(*hook)(void*),void* context)noexcept{progressHook_=hook;progressContext_=context;}
 bool progressDrawing()const noexcept{return progressDrawing_;}
 // Production enables this after startup. Host consumers without an LCD keep
 // their synchronous contract; the deferred path is separately exercised.
 void deferFileCommands(bool enabled)noexcept{deferFiles_=enabled;}
 struct FileTiming {
  uint32_t serial=0,key_ms=0;Command command=Command::None;
  uint64_t request_us=0,feedback_us=0,start_us=0,install_us=0,complete_us=0,body_lcd_us=0;
  bool cancelled=false;
 } fileTiming;
 void fileFramePresented(uint64_t us,bool busy)noexcept;
 bool fileRequestPending()const noexcept{return queuedFile_!=Command::None;}

 mutable CandidateVisualContinuity candidateVisual;
 bool dirtyDisplay=true;
 const char* notice="Starting";
 draftor::memory::String currentName;
 std::array<char,128> restoredNotice{}; // actual restored name + status label, no body
 ds::OperationResult lastOperation{};
 const char* restoredName()const noexcept{return notice==restoredNotice.data()?restoredNotice.data()+restoredPrefix_:nullptr;}
 uint32_t operationSerial=0;
 struct CreateTiming {uint64_t us[10]{},max_us[10]{};uint32_t steps[10]{};} createTiming;

 bool findOpen()const noexcept{return find.view().open;}
 bool statsOpen()const noexcept{return statsOpen_;}
 // A future UTF-8 producer must retain this exact query/focus identity.
 struct QueryStamp {uint64_t focus;uint32_t session,revision;size_t cursor;};
 QueryStamp queryStamp()const noexcept{auto v=find.view();return {focusToken(),find.sessionId(),v.query_version,v.query_cursor};}
 bool commitQuery(std::string_view text,QueryStamp expected);
 struct ScanMetrics {
  uint32_t find_completed=0,find_cancelled=0,stats_completed=0,stats_cancelled=0;
  uint64_t find_us=0,stats_us=0,max_find_step_us=0,max_stats_step_us=0;
  size_t max_find_work=0,max_stats_bytes=0;
 } scans;
 hanime::text_stats::Report statsReport()const noexcept{return stats.report();}
 bool renameOpen()const noexcept{return renaming_;}
 ds::DocumentId renameId()const noexcept{return renameId_;}
 bool imeTextTargetAvailable()const noexcept {
  return ready_&&!transfer_active_&&!commandBarrier()&&!findOpen()&&!statsOpen()&&
    (renameOpen()||(session.hasCurrent()&&!filesOpen()));
 }
 // Rename is the same horizontal modal in either body layout.
 bool horizontalImeSurface()const noexcept{return horizontalActive()||renameOpen();}
 const char* renameText()const noexcept{return nameInput_.text();}
 bool renameSelected()const noexcept{return nameInput_.selected();}
 size_t renameCursor()const noexcept{return nameInput_.cursor();}
 uint32_t renameRevision()const noexcept{return nameInput_.revision();}
 struct NameStamp {uint64_t focus;uint32_t revision;size_t cursor;bool selected=false;};
 NameStamp nameStamp()const noexcept{return {focusToken(),renameRevision(),renameCursor(),renameSelected()};}
 bool commitName(std::string_view text,NameStamp expected);
 bool commitName(std::string_view text,uint64_t focus){auto stamp=nameStamp();stamp.focus=focus;return commitName(text,stamp);}
 bool filesOpen()const{return manager.state().view!=ds::FileManagerView::Closed;}
 // Matches the public core TextCommitSink::commit(string_view) semantics.
 // Integration will bind/validate a focus token before calling; no IME runtime here.
 bool commitText(std::string_view text,EditStamp expected);
 EditStamp editStamp()const{return {focusToken(),document.revision(),editor.selectionVersion()};}
 uint64_t focusToken()const{return focusEpoch_;}
 bool bodyNavigationPending()const noexcept{return presentation_.navigationPending();}
 editor::SharedEditorView sharedState()const noexcept{return {document_,editor_};}
 editor::presentation::PresentationPort& presentation()noexcept{return presentation_;}
 LayoutMode layoutMode()const noexcept{return presentation_.mode();}
 bool horizontalActive()const noexcept{return presentation_.horizontalActive();}
 PresentationFrame presentationFrame()const noexcept{return presentation_.frame();}
 bool currentPresentation(PresentationFrame f)const noexcept{return presentation_.current(f);}
 bool presentationPending()const noexcept{return presentation_.framePending();}
 bool presentationFramePresented(PresentationFrame f)noexcept{return presentation_.framePresented(f);}
 unsigned compositionWidth()const noexcept{return presentation_.compositionWidth();}
 void beginCompositionPresentation(bool full_pinyin)noexcept{presentation_.cancelNavigation();presentation_.beginComposition(full_pinyin);}
 VerticalImeGeometryState verticalImeGeometryState()const noexcept{return presentation_.verticalImeGeometryState();}
 void chinesePresentationContext(bool visible)noexcept{presentation_.chineseContext(visible);}
 VerticalPresentationState verticalPresentationState()const noexcept{return presentation_.verticalPresentationState();}
 void observeCompositionWidth(unsigned width)noexcept{presentation_.observeCompositionWidth(width);}
 void endCompositionPresentation()noexcept{presentation_.endComposition();}
 void candidateColumns(unsigned count)noexcept{presentation_.candidateColumns(count);}
 void compositionScheme(bool full_pinyin)noexcept{presentation_.compositionScheme(full_pinyin);}
 void observeComposition(bool full_pinyin,unsigned width,bool selected)noexcept{
  presentation_.observeComposition(full_pinyin,width,selected);
 }
 bool selectedAppeared()const noexcept{return presentation_.selectedAppeared();}
 draftor::vertical::AbGeometry imeGeometry()const noexcept{return presentation_.imeGeometry();}
 bool preparePresentation(bool incremental=false)noexcept{return presentation_.prepare(incremental);}
 VerticalPresentation& verticalPresentation()noexcept{return vertical_;}
 // Product command route: called before DeviceIme and every editor/text route.
 bool layoutKey(const s3hid::Event&)noexcept;
 editor::body_edit::Outcome moveToByte(size_t byte,bool extend=false);
 private:
 friend struct AppTestAccess; // Host fault/setup fixtures only; no runtime instance.
 editor::Document document_;
 editor::body_edit::BodyEditor editor_;
 HorizontalPresentation horizontal_;
 VerticalPresentation vertical_;
 PresentationController presentation_;
 uint32_t layout_key_epoch_=0;bool layout_key_held_=false;
 public:
 // Read-only compatibility views for the unchanged P23 IME/diagnostic consumers.
 const editor::Document& document;
 const editor::body_edit::BodyEditor& editor;
 // Explicitly Horizontal geometry aliases; never shared cursor/selection state.
 hanime::renderer::LayoutEngine& layout;
 editor::visual_nav::VisualCursorNavigator& navigator;
 ds::EntropyIdSource ids;
 ds::DocumentCatalog catalog;
 ds::PersistenceService persistence;
 ds::integration::HanimeDocumentEditorPort editorPort;
 ds::DocumentSession session;
 ds::FileManagerModel manager;
 hanime::find::integration::HanimeDocumentAdapter findDocument;
 hanime::find::FindSession find;
 hanime::find::input::FindController findController;
 hanime::text_stats::Counter stats;
 private:
 void progress(const char* text);
 void(*progressHook_)(void*)=nullptr;void* progressContext_=nullptr;bool progressDrawing_=false;
 void beginRename();
 void cancelRename() noexcept;
 void renameKey(const s3hid::Event&);
 NameInput nameInput_;
 ds::DocumentId renameId_{};
 uint64_t renameSession_=0;
 bool renaming_=false;
 uint8_t restoredPrefix_=0;
 void syncReaders();
 void observeStats();
 void closeReaders() noexcept;
 bool findKey(const s3hid::Event&);
 void serviceReaders();
 bool statsOpen_=false;
 uint32_t findTextRevision_=0;
 void mutation(const editor::body_edit::Outcome&,uint32_t,bool preserve_preferred=false);
 void issue(Command,uint32_t,bool resumed=false);
 void cancelFileRequest()noexcept;
 bool deferFiles_=false,fileExecuting_=false;
 Command queuedFile_=Command::None;
 ds::DocumentId queuedFileId_{};
 uint64_t queuedSession_=0,queuedFocus_=0;
 uint32_t queuedRevision_=0;

 void execute(Command,uint32_t);
 void result(ds::OperationResult);
 void cacheCurrentName();
 static void afterInstall(void*)noexcept;
 struct ManualSaveRequest {
  ds::DocumentId id{};
  uint64_t session=0,mount=0,revision_epoch=0;
  uint32_t requested_revision=0,started_revision=0;
  bool pending=false,running=false;
 } manualSave_;
 bool manualSaveIdentityValid()const noexcept;
 ds::OperationResult startRequestedSave(uint32_t now);
 void failRequestedSave(ds::OperationResult);
 bool ready_=false;
 bool transfer_active_=false;
 uint64_t focusEpoch_=1; // host focus generation, advanced on install and UI focus changes
 uint32_t now_=0;
 Command pending_=Command::None;
};
}
