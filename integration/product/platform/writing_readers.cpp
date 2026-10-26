#include "writing_app.hpp"
#include "operation_profile.hpp"
#include "reader_display.hpp"
#include <algorithm>

namespace s3app {
namespace hf=hanime::find;
namespace fi=hanime::find::input;
namespace ts=hanime::text_stats;

void App::scheduleFind(){
 findScheduled_=findOpen()&&!find.view().query.empty();findDue_=now_+80;dirtyDisplay=true;
}
void App::observeStats() {
 const auto before=stats.report();
 ts::observe_document(stats,document,session.sessionSerial());
 const auto after=stats.report();
 if(before.context_id!=after.context_id||before.text_revision!=after.text_revision){
  if(before.state==ts::State::Counting)++scans.stats_cancelled;
  dirtyDisplay=true;
 }
}
void App::syncReaders() {
 const auto mount=catalog.boundSession().mount_epoch;
 if(!findDocument.bindingMatches(session.currentId(),session.sessionSerial(),mount)){
  closeReaders();
  ++focusEpoch_;
  find=hf::FindSession{}; // queries belong to one installed document only
  findDocument.rebind(session.currentId(),session.sessionSerial(),mount);
  stats.reset();
 } else if(findOpen()&&document.textRevision()!=findTextRevision_){
  // Mutations include undo/redo and future externally submitted body edits.
  if(find.view().status==hf::Status::Searching)++scans.find_cancelled;
  find.documentSwitched(findDocument); // rebase at current caret; no old offsets survive
  scheduleFind();
  dirtyDisplay=true;
 }
 findTextRevision_=document.textRevision();
 if(session.hasCurrent())observeStats();else stats.reset();
}
void App::closeReaders() noexcept {
 findScheduled_=false;
 if(find.view().status==hf::Status::Searching)++scans.find_cancelled;
 const auto cursor=document.cursorByte();
 findController.closeForExternalFocus(false);
 findController.invalidateQueuedInput();
 findDocument.clearMatchHighlight();
 presentation_.findSurface(false);
 if(cursor!=document.cursorByte())session.onPositionChanged(now_);
 statsOpen_=false;
 dirtyDisplay=true;
}
bool App::commitQuery(std::string_view text,QueryStamp stamp) {
 syncReaders();
 if(!ready_||!findOpen()||statsOpen_||filesOpen()||commandBarrier()||
    stamp.focus!=focusToken()||stamp.session!=find.sessionId()||
    stamp.revision!=find.view().query_version||stamp.cursor!=find.view().query_cursor||stamp.selected!=find.view().query_selected||stamp.selection!=find.view().selection_version)return false;
 const bool scanning=find.view().status==hf::Status::Searching;
 auto result=find.insertQueryUtf8(text);
 if(result!=hf::QueryEditResult::NoChange)dirtyDisplay=true;
 if(result==hf::QueryEditResult::Applied){
  if(scanning)++scans.find_cancelled;
  scheduleFind();
  return true;
 }
 return result==hf::QueryEditResult::NoChange;
}
bool App::findKey(const s3hid::Event& e) {
 const auto cp=s3hid::uk(e.usage,e.mods);
 bool ctrl=e.mods&0x11,gui=e.mods&0x88,alt=e.mods&0x44,shift=e.mods&0x22;
 if(e.usage==9&&(ctrl!=gui)&&!alt&&!shift&&findOpen()){
  closeReaders();++focusEpoch_;editor_.discardPreparedTextInput();dirtyDisplay=true;return true;
 }
 fi::Modifier mods=fi::Modifier::None;
 if(ctrl)mods=mods|fi::Modifier::Ctrl;
 if(gui)mods=mods|fi::Modifier::Gui;
 if(alt)mods=mods|fi::Modifier::Alt;
 if(shift)mods=mods|fi::Modifier::Shift;
 fi::KeyKind kind=fi::KeyKind::Character;
 switch(e.usage){
 case 40:kind=fi::KeyKind::Enter;break;
 case 41:kind=fi::KeyKind::Escape;break;
 case 42:kind=fi::KeyKind::Backspace;break;
 case 44:kind=fi::KeyKind::Space;break;
 case 76:kind=fi::KeyKind::DeleteForward;break;
 case 80:kind=fi::KeyKind::Left;break;
 case 79:kind=fi::KeyKind::Right;break;
 case 82:kind=fi::KeyKind::Up;break;
 case 81:kind=fi::KeyKind::Down;break;
 case 74:kind=fi::KeyKind::Home;break;
 case 77:kind=fi::KeyKind::End;break;
 default:break;
 }
 if(findOpen()&&!horizontalActive()){
  if(kind==fi::KeyKind::Up)kind=fi::KeyKind::Left;
  else if(kind==fi::KeyKind::Down)kind=fi::KeyKind::Right;
  else if(kind==fi::KeyKind::Left||kind==fi::KeyKind::Right)return true;
 }
 if(findOpen()&&cp>127&&!ctrl&&!gui&&(!alt||((e.mods&0xdd)==0x40&&e.usage==8))){
  char bytes[3];size_t n;
  if(cp<2048){bytes[0]=0xc0|(cp>>6);bytes[1]=0x80|(cp&63);n=2;}
  else{bytes[0]=0xe0|(cp>>12);bytes[1]=0x80|((cp>>6)&63);bytes[2]=0x80|(cp&63);n=3;}
  commitQuery({bytes,n},queryStamp());return true;
 }
 // The platform has already applied Shift to printable characters. Semantic
 // router expects direct printable input without Shift (Enter keeps Shift).
 if(kind==fi::KeyKind::Character&&cp>=32&&cp<127&&!ctrl&&!gui&&!alt)mods=fi::Modifier::None;
 const auto before=find.view();const auto cursor=document.cursorByte();
 auto result=findController.dispatch(findController.stamp(kind,cp<128?char(cp):0,mods),false,true);
 if(result==fi::DispatchResult::PassToExistingImeOrBody)return findOpen();
 if(!before.open&&findOpen()){(void)find.setQuery({});find.moveQueryCursorHome();}
 const auto after=find.view();
 if(before.query_version!=after.query_version)scheduleFind();
 if(kind==fi::KeyKind::Enter||!after.open)findScheduled_=false;
 if(before.open!=after.open){
  presentation_.findSurface(after.open);
  ++focusEpoch_;editor_.discardPreparedTextInput();
 }
 if(before.status==hf::Status::Searching&&(before.query_version!=after.query_version||!after.open))++scans.find_cancelled;
 if(cursor!=document.cursorByte())session.onPositionChanged(e.ms);
 dirtyDisplay=true;
 return true;
}
void App::serviceReaders() {
 if(findScheduled_&&int32_t(now_-findDue_)>=0){findScheduled_=false;find.start(hf::Direction::Forward);}
 if(find.view().match.valid&&find.view().wrapped&&int32_t(now_-findWrapUntil_)>=0){find.clearWrapNotice();dirtyDisplay=true;}
 if(find.hasWork()){
  const auto started=s3profile::clockUs();
  auto result=find.step(findDocument,256);
  const auto elapsed=s3profile::clockUs()-started;
  scans.find_us+=elapsed;
  scans.max_find_step_us=std::max(scans.max_find_step_us,elapsed);
  scans.max_find_work=std::max(scans.max_find_work,find.lastStepWork());
  if(result!=hf::StepResult::MoreWork){
   if(result==hf::StepResult::Stale)++scans.find_cancelled;
   else if(result!=hf::StepResult::Idle)++scans.find_completed;
   if(result==hf::StepResult::Found){session.onPositionChanged(now_);findWrapUntil_=now_+1200;}
   dirtyDisplay=true;
  }
 }
 const auto old=stats.report();
 if(old.state==ts::State::Counting){
  const auto started=s3profile::clockUs();
  const auto work=ts::service_document(stats,document,session.sessionSerial(),256);
  const auto elapsed=s3profile::clockUs()-started;
  scans.stats_us+=elapsed;
  scans.max_stats_step_us=std::max(scans.max_stats_step_us,elapsed);
  scans.max_stats_bytes=std::max(scans.max_stats_bytes,work);
  if(stats.report().complete())++scans.stats_completed;
  if(statsOpen_&&(stats.report().state!=old.state||old.scanned_bytes/4096!=stats.report().scanned_bytes/4096))dirtyDisplay=true;
 }
}
}
