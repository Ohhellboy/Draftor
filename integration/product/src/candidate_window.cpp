#include "cp_observe.hpp"
#include "behavior_bench.hpp"
#include "candidate_window.hpp"
#include "ui16_font.hpp"
#include "behavior_observer.hpp"
#include "utf8.hpp"
#include "ui_text.hpp"
#include "ui_trace.hpp"
#include <algorithm>
#include <cstring>
namespace ime_device {
using Status=hanime_extension::Status;
namespace {
uint32_t presentationSequence=0;
uint32_t nextPresentation(){return presentationSequence<UINT32_MAX?++presentationSequence:0;}
void clearRow(CandidateRow& row){for(auto& item:row.items)item=VisibleItem{};row.index=0;row.count=0;row.width=0;row.vertical=false;}
void clearView(CandidateView& view){for(auto& row:view.rows)clearRow(row);view.count=view.row=view.col=0;view.generation=view.action=0;view.expanded=view.at_end=false;}
}
void CandidateWindow::failStatus(Status status,hanime::OutputStatus output){
 last_status_=status;last_output_=output;
 const auto state=status==Status::CapacityExceeded?WindowState::Capacity:
  status==Status::Busy?WindowState::Busy:status==Status::Stale?WindowState::Stale:
  status==Status::OutputRestricted?WindowState::OutputError:
  status==Status::NoMappingForPrefix||status==Status::NoPrefixMatch?WindowState::NoResults:WindowState::Error;
 traceUi(UiTraceKind::SelectionResult,&c_,this,nullptr,choosing_?&choice_:nullptr,uint8_t(status));
 fail(state);
}
void CandidateWindow::fail(WindowState state){if(state==WindowState::Stale)last_status_=Status::Stale;task_=Task::None;background_=false;state_=state;choosing_=default_demand_=false;pending_={};completion_=SelectionCompletion::None;changed_=true;BT_OBSERVE(selectionResult(c_,state==WindowState::Stale?2:3));}
bool CandidateWindow::current(const VisibleItem& i)const{
 const auto s=c_.snapshot();return i.input==s.input_id&&i.revision==s.revision&&i.scheme==s.scheme_epoch&&i.activation.mount_epoch()==s.activation.mount_epoch()&&i.activation.shared_bundle_id()==s.activation.shared_bundle_id();
}
const VisibleItem* CandidateWindow::focusedCurrent()const{
 if(!view_.count||view_.row<view_.rows[0].index||
    view_.row-view_.rows[0].index>=view_.count)return nullptr;
 const auto& row=view_.rows[view_.row-view_.rows[0].index];
 if(view_.col>=row.count)return nullptr;
 const auto& item=row.items[view_.col];
 const auto page=c_.page();
 if(!current(item)||item.begin!=page.begin||item.end!=page.end)return nullptr;
 return &item;
}
bool CandidateWindow::focusedNeedsWholeText()const{
 const auto* item=focusedCurrent();if(!find_profile_||!item)return false;
 const s3app::ui16::Font face;unsigned pixels=0;const std::string_view text(item->text.data(),item->bytes);
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))pixels+=face.glyph(s3app::display::scalar(text,at)).advance;
 return pixels>(vertical_?196U:272U);
}
void CandidateWindow::cancel(){
 CPATTR(cpattr::cancel());
 if(layout_serial_<UINT32_MAX)++layout_serial_;
 last_status_=Status::Ready;last_output_=hanime::OutputStatus::Cancelled;
 if(background_||prefetched_.count)++costs_.prefetch_cancelled;
 if(working()&&task_!=Task::CachedText)c_.cancel(op_);
 task_=Task::None;background_=false;pending_={};choosing_=default_demand_=false;reading_ready_=stream_valid_=end_known_=false;
 completion_=SelectionCompletion::None;
 probe_only_=probe_cache_=repacking_=locating_=false;
 clearView(view_);clearView(building_);clearRow(row_);clearRow(prefetched_);anchors_={};
 state_=WindowState::Empty;changed_=true;
 BT_OBSERVE(selectionResult(c_,5));
}
uint16_t CandidateWindow::width(std::string_view text)const{
 const s3app::ui16::Font ui;const auto& face=find_profile_?static_cast<const s3host::FontPort&>(ui):font_;
 size_t pixels=find_profile_?24:22;for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))pixels+=face.glyph(s3app::display::scalar(text,at)).advance;
 return uint16_t(std::min(pixels,size_t(find_profile_?296:284)));
}

uint16_t CandidateWindow::extent(const VisibleItem& item)const{
 if(!vertical_)return item.width;
 const s3app::ui16::Font ui;
 unsigned height=find_profile_?20:14+vertical_gap;
 const std::string_view text(item.text.data(),item.bytes);
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))height+=find_profile_?ui.glyph(s3app::display::scalar(text,at)).advance:s3app::display::scalar(text,at)<128?14:28;
 return uint16_t(std::min(height,find_profile_?216:vertical_height+vertical_gap));
}
const VisibleItem* CandidateWindow::cachedItem(size_t ordinal)const{
 for(const auto* v:{&view_,&frame_,&previous_})for(size_t r=0;r<v->count;++r)
  for(size_t c=0;c<v->rows[r].count;++c){const auto& item=v->rows[r].items[c];
   if(item.ordinal==ordinal&&current(item)&&item.begin==c_.page().begin&&item.end==c_.page().end)return &item;
  }
 for(size_t i=0;i<prefetched_.count;++i)if(prefetched_.items[i].ordinal==ordinal&&current(prefetched_.items[i])&&prefetched_.items[i].begin==c_.page().begin&&prefetched_.items[i].end==c_.page().end)return &prefetched_.items[i];
 return nullptr;
}
bool CandidateWindow::setPacking(bool vertical,bool find){
 if(find_profile_!=find){
  if(working()||choosing_||pending_.valid)return false;
  cancel();retirePresentedChoices();find_profile_=find; // A different target cannot retain candidate authority.
 }
 if(vertical){vertical_seen_=true;if(view_.expanded)stage_expanded_=true;}
 if(vertical_==vertical)return !repacking_;
 if(task_==Task::Query&&!choosing_&&!view_.count&&!building_.count&&!row_.count){vertical_=vertical;return true;}
 if(working()||choosing_||pending_.valid)return false;
 vertical_=vertical;
 if(!view_.count)return true;
 const auto& active=view_.rows[view_.row-view_.rows[0].index];
 focus_ordinal_=active.items[view_.col].ordinal;
 expanded_=view_.expanded||stage_expanded_;exact_=false;
 Anchor anchor{0,0,0,true,0,0,vertical_};
 // Retained anchors belong to one physical profile, exact sequence positions.
 for(const auto& a:anchors_)if(a.valid&&a.vertical==vertical_&&a.ordinal<=focus_ordinal_&&a.ordinal>=anchor.ordinal)anchor=a;
 // Expanded requires an earlier column. Use its exact retained boundary.
 if(expanded_&&anchor.row){
  bool prior=false;for(const auto& a:anchors_)if(a.valid&&a.vertical==vertical_&&a.row+1==anchor.row){anchor=a;prior=true;break;}
  if(!prior)anchor={0,0,0,true,0,0,vertical_};
 }
 clearView(building_);clearRow(row_);clearRow(prefetched_);probe_cache_=false;
 if(reading_ready_){prefetched_.items[0]=reading_;prefetched_.count=1;prefetched_.index=SIZE_MAX;}
 scan_row_=anchor.row;ordinal_=anchor.ordinal;seek_page_=anchor.page;seek_slot_=anchor.slot;
 end_known_=false;reading_ready_=false;stream_valid_=true;
 locating_=repacking_=true;state_=WindowState::Loading;changed_=true;
 beginText();return false;
}

void CandidateWindow::beginScan(Status s){
 if(s==Status::Working){task_=Task::Query;op_=c_.operation();state_=WindowState::Loading;changed_=true;}
 else queryFailed(s);
}
bool CandidateWindow::planningLimited()const{
 const auto report=c_.planning_report();
 return report.limited&&c_.plan_is_current(report);
}
void CandidateWindow::queryFailed(Status status){
 if(status==Status::Unresolved){
  fail(planningLimited()?WindowState::Limited:WindowState::Unresolved);
 }else if(status==Status::NoPrefixMatch||status==Status::NoMappingForPrefix){
  fail(WindowState::NoResults);
 }else{
  failStatus(status);
 }
}
void CandidateWindow::query(size_t prefix,uint32_t action){
 if(query_serial_<UINT32_MAX)++query_serial_;
 cancel();const auto s=c_.snapshot();input_=s.input_id;revision_=s.revision;want_row_=want_col_=page_=slot_=scan_row_=ordinal_=0;expanded_=stage_expanded_;exact_=false;
 action_=action;range_traced_=false;
 // Normal raw/suffix input uses the public planner. A user-selected prefix
 // remains exact; a plan or its first metadata item never authorizes a choice.
 // This adapter has one fixed default output profile and disabled learning;
 // no mutable scoring/output context exists, so context revision is zero.
 beginScan(prefix?c_.choose_prefix(prefix):c_.query_next({},0));
 BT_OBSERVE(query(c_,*this,prefix));
 CPATTR(cpattr::begin(action_,query_serial_,s.input_id,s.revision,s.scheme_epoch,s.activation.mount_epoch(),s.activation.shared_bundle_id()));
 traceUi(UiTraceKind::Query,&c_,this);
}
Status CandidateWindow::tryPrefix(size_t prefix,uint32_t action){
 if(busy())return Status::Busy;
 // choose_prefix validates its endpoint before invalidating the old page.
 // Never reinterpret a rejected zero/odd endpoint as query_next().
 const auto result=c_.choose_prefix(prefix);
 if(result==Status::InvalidRequest||result==Status::Busy)return result;
 if(query_serial_<UINT32_MAX)++query_serial_;
 cancel();const auto snap=c_.snapshot();input_=snap.input_id;revision_=snap.revision;
 want_row_=want_col_=page_=slot_=scan_row_=ordinal_=0;expanded_=stage_expanded_;exact_=false;
 action_=action;range_traced_=false;beginScan(result);
 BT_OBSERVE(query(c_,*this,prefix));
 traceUi(UiTraceKind::Query,&c_,this);return result;
}
void CandidateWindow::recheck(){
 cancel();const auto status=c_.revalidate();
 if(status==Status::Working){task_=Task::Recheck;op_=c_.operation();state_=WindowState::Loading;changed_=true;}
 else if(status==Status::Ready){completion_=SelectionCompletion::Recheck;state_=WindowState::Selected;changed_=true;}
 else failStatus(status);
}
const CandidateRow* CandidateWindow::cached(size_t index)const{
 const auto page=c_.page();
 const auto matches=[&](const CandidateRow& row){
  if(!row.count||row.index!=index||row.vertical!=vertical_)return false;
  const auto& item=row.items[0];
  // choose_prefix can change the query range without editing the input. A
  // matching input revision alone does not make an old display row reusable.
  return current(item)&&item.begin==page.begin&&item.end==page.end;
 };
 if(matches(prefetched_))return &prefetched_;
 if(probe_cache_&&matches(building_.rows[2]))return &building_.rows[2];
 for(const auto* v:{&view_,&frame_,&previous_})for(size_t i=0;i<v->count;++i)
  if(matches(v->rows[i]))return &v->rows[i];
 return nullptr;
}
size_t CandidateWindow::reuseCachedRows(){
 const auto first=expanded_&&want_row_?want_row_-1:want_row_;
 const size_t count=expanded_?3:1;
 const auto before=building_.count;
 // A hole only stops this pass. The old visible/presented rows remain intact
 // until publication, and are checked again after each missing row is built.
 // Copied text keeps its full context; choosing still reauthorizes through the
 // public range/selection API rather than reviving the stored Selection.
 while(building_.count<count){
  const auto index=first+building_.count;
  if(end_known_&&index>=total_rows_)break;
  const auto* ready=cached(index);
  if(!ready)break;
  building_.rows[building_.count++]=*ready;
  ++costs_.reused_rows;
  if(ready==&prefetched_){++costs_.prefetch_hits;clearRow(prefetched_);}
 }
 return building_.count-before;
}
void CandidateWindow::request(size_t row,size_t col,bool expand,bool exact,uint32_t action){
 if(layout_serial_<UINT32_MAX)++layout_serial_;
 last_status_=Status::Ready;last_output_=hanime::OutputStatus::Cancelled;
 if(working()){
  if(!pending_.valid&&!choosing_){pending_={row,col,expand,exact,true,action};if(background_){++costs_.foreground_waits;}traceUi(UiTraceKind::RowRequest,&c_,this,nullptr,nullptr,0,action,0,row);}
  else ++metrics.blocked;
  return;
 }
 // No usable page exists in these terminal query states. Navigation cannot
 // manufacture a page or turn an unresolved/limited result into a read error.
 if(!view_.count&&(state_==WindowState::Unresolved||state_==WindowState::Limited||state_==WindowState::Error||state_==WindowState::Stale)){changed_=true;return;}
 probe_only_=false;action_=action;range_traced_=false;
 traceUi(UiTraceKind::RowRequest,&c_,this,nullptr,nullptr,0,action,0,row);
 if((!view_.count&&(state_==WindowState::NoResults||state_==WindowState::Boundary))||(end_known_&&row>=total_rows_)){
  state_=WindowState::Boundary;changed_=true;return;
 }
 want_row_=row;want_col_=col;expanded_=expand||view_.expanded||stage_expanded_;exact_=exact;
 if(probe_cache_){clearRow(building_.rows[0]);clearRow(building_.rows[1]);building_.count=0;}
 else clearView(building_);
 const auto first=expanded_&&row?row-1:row;
 const auto count=expanded_?3u:1u;
 reuseCachedRows();
 if(building_.count==count||(end_known_&&first+building_.count==total_rows_)){finishScan(end_known_&&first+building_.count==total_rows_);return;}
 ++costs_.prefetch_misses;state_=WindowState::Loading;changed_=true;continueScan();
}
bool CandidateWindow::rowCount(size_t row,size_t& count)const{
 if(const auto* r=cached(row)){count=r->count;return true;}
 // Anchors are owned by this query and cleared on edit/query/cancellation.
 for(const auto& a:anchors_)if(a.valid&&a.vertical==vertical_&&a.row==row&&a.count){count=a.count;return true;}
 return false;
}
void CandidateWindow::probeRow(size_t row,uint32_t action){
 if(working())return;
 probe_only_=true;action_=action;want_row_=row;want_col_=0;expanded_=false;exact_=false;
 if(prefetched_.count){building_.rows[2]=prefetched_;probe_cache_=true;}
 clearRow(building_.rows[0]);clearRow(building_.rows[1]);building_.count=0;
 state_=WindowState::Loading;continueScan();
}
void CandidateWindow::continueScan(){
 const auto first=expanded_&&want_row_?want_row_-1:want_row_;
 const auto missing=first+building_.count;
 if(stream_valid_&&scan_row_<=missing){resumeText();return;}
 // A bounded row-position ring gives exact line starts even for variable-width
 // text. On eviction, use the nearest earlier retained anchor (row zero is
 // implicit). This never infers a row from a six-item core range.
 Anchor anchor{0,0,0,true,0,0,vertical_};
 for(const auto& a:anchors_)if(a.valid&&a.vertical==vertical_&&a.row<=missing&&a.row>=anchor.row)anchor=a;
 if(missing&&anchor.row==0)++costs_.anchor_fallbacks;
 scan_row_=anchor.row;ordinal_=anchor.ordinal;seek_page_=anchor.page;seek_slot_=anchor.slot;clearRow(row_);reading_ready_=false;stream_valid_=true;seek();
}
bool CandidateWindow::movePage(hanime::RangeDirection direction,Task task){
 const auto s=c_.request_page(direction);
 if(s!=Status::Working){failStatus(s);return false;}
 if(direction==hanime::RangeDirection::After)++page_;else if(direction==hanime::RangeDirection::Before)--page_;else page_=0;
 last_move_before_=direction==hanime::RangeDirection::Before;
 ++costs_.range_moves;task_=task;op_=c_.operation();return true;
}
void CandidateWindow::seek(){
 if(page_!=seek_page_){movePage(page_<seek_page_?hanime::RangeDirection::After:hanime::RangeDirection::Before,Task::Seek);return;}
 slot_=seek_slot_;resumeText();
}
void CandidateWindow::resumeText(){if(reading_ready_)appendText();else beginText();}
void CandidateWindow::beginText(){
 if(repacking_){
  if(const auto* cached=cachedItem(ordinal_)){
   reading_=*cached;reading_ready_=true;task_=Task::CachedText;return;
  }
  if(page_!=seek_page_){seek();return;}
  slot_=seek_slot_;
 }
 const auto p=c_.page();
 if(slot_>=p.size){
  if(!forward_more_){
   end_known_=true;total_rows_=scan_row_+(row_.count?1:0);
   if(row_.count&&finishRow())return;
   if(background_){background_=false;task_=Task::None;return;}
   finishScan(true);return;
  }
  if(movePage(hanime::RangeDirection::After,Task::Page)){slot_=0;if(repacking_){seek_page_=page_;seek_slot_=0;}}
  return;
 }
 const auto snapshot=c_.snapshot();const auto item=p.items[slot_];reading_={};
 reading_.key=item.key;reading_.ref=item.ref;reading_.selection=c_.selection(slot_);
 reading_.input=snapshot.input_id;reading_.revision=snapshot.revision;reading_.scheme=snapshot.scheme_epoch;reading_.activation=snapshot.activation;
 reading_.ordinal=ordinal_;reading_.page=page_;reading_.slot=slot_;reading_.page_generation=p.generation;reading_.begin=p.begin;reading_.end=p.end;
 const auto s=c_.read_text(reading_.selection);if(s!=Status::Working){failStatus(s);return;}
 task_=Task::Text;op_=c_.operation();
}
bool CandidateWindow::finishRow(){
 if(!row_.count)return false;
 row_.index=scan_row_;row_.vertical=vertical_;
 anchors_[scan_row_%anchors_.size()]={scan_row_,row_.items[0].page,row_.items[0].slot,true,uint8_t(row_.count),row_.items[0].ordinal,vertical_};++costs_.built_rows;
 if(background_){
  prefetched_=row_;++scan_row_;clearRow(row_);background_=false;task_=Task::None;
  if(pending_.valid){const auto n=pending_;pending_={};request(n.row,n.col,n.expand,n.exact,n.action);}
  return true;
 }
 if(locating_){
  const auto first_ordinal=row_.items[0].ordinal;
  if(focus_ordinal_>=first_ordinal+row_.count){
   building_.rows[0]=row_;++scan_row_;clearRow(row_);return false;
  }
  want_row_=scan_row_;want_col_=focus_ordinal_-first_ordinal;
  building_.count=expanded_&&scan_row_?1:0;locating_=false;
 }
 const size_t first=expanded_&&want_row_?want_row_-1:want_row_;
 if(scan_row_>=first&&scan_row_>=first+building_.count&&building_.count<building_.rows.size())building_.rows[building_.count++]=row_;
 ++scan_row_;clearRow(row_);
 const auto reused=scan_row_==first+building_.count?reuseCachedRows():0;
 if(reused){
  // The reader still points at the beginning of the skipped rows, possibly
  // holding their first overflow item. It is not the stream after those rows.
  // Retain the ordinary overflow item when no row was skipped; otherwise the
  // next true hole resumes at an exact retained row anchor.
  stream_valid_=false;reading_ready_=false;
  scan_row_=first+building_.count;
 }
 const size_t count=expanded_?3:1;
 const bool end=end_known_&&first+building_.count==total_rows_;
 if(building_.count==count||end){finishScan(end);return true;}
 if(reused){continueScan();return true;}
 return false;
}
void CandidateWindow::appendText(){
 if(!reading_ready_){
  const auto text=c_.last_text();if(text.size()>64){fail(find_profile_?WindowState::OutputError:WindowState::Error);return;}
  if(find_profile_&&!s3app::ui16::canRender(text)){fail(WindowState::OutputError);return;}
  std::copy(text.begin(),text.end(),reading_.text.begin());reading_.bytes=uint16_t(text.size());reading_.width=width(text);reading_ready_=true;++costs_.materialized_items;
 }
 // The one overflow item is retained, so the next row never rereads its text.
 if(row_.count&&(row_.count==6||row_.width+extent(reading_)>(vertical_?(find_profile_?216:vertical_height+vertical_gap):(find_profile_?296:284))))if(finishRow())return;
 row_.items[row_.count++]=reading_;row_.width+=extent(reading_);++ordinal_;slot_=reading_.slot+1;
 if(repacking_){seek_page_=reading_.page;seek_slot_=reading_.slot+1;}
 reading_ready_=false;
 const auto p=c_.page();if((!repacking_||page_==seek_page_)&&slot_>=p.size&&!forward_more_){end_known_=true;total_rows_=scan_row_+1;}
 if(row_.count==6&&finishRow())return;
 beginText();
}
void CandidateWindow::finishScan(bool end){
 task_=Task::None;if(repacking_)stream_valid_=false;repacking_=false;
 if(probe_only_){
  probe_only_=false;
  if(building_.count){prefetched_=building_.rows[0];state_=WindowState::Ready;}
  else state_=end?WindowState::Boundary:WindowState::Error;
  // This is demanded navigation layout, never a published LCD generation.
  return;
 }
 if(!building_.count){fail(end?WindowState::Boundary:WindowState::Error);return;}
 if(want_row_<building_.rows[0].index||want_row_>=building_.rows[0].index+building_.count){fail(WindowState::Boundary);return;}
 const auto& r=building_.rows[want_row_-building_.rows[0].index];const bool boundary=exact_&&want_col_>=r.count;
 building_.row=want_row_;building_.col=boundary?0:std::min(want_col_,r.count-1);building_.expanded=expanded_;building_.at_end=end;building_.generation=nextPresentation();building_.action=action_;view_=building_;probe_cache_=false;state_=boundary?WindowState::Boundary:WindowState::Ready;changed_=true;
 // finishScan publishes either the requested full window or a proven end
 // at this window's own start. A deep tail is not a total sequence count.
 if(vertical_&&focusedCurrent()&&(!expanded_||view_.count==3||end))
  occupied_columns_=expanded_?view_.count:1;
 if(vertical_seen_&&expanded_)stage_expanded_=true;
 BT_MEASURE(ready(view_.action));
 CPATTR(cpattr::ready());
 traceUi(UiTraceKind::WindowReady,&c_,this);
 if(choosing_){startAuth();return;}
 if(pending_.valid){const auto n=pending_;pending_={};request(n.row,n.col,n.expand,n.exact,n.action);}
}
void CandidateWindow::prefetch(){
 if(!prefetch_enabled_||working()||!view_.expanded||view_.count!=3||view_.at_end||frame_.generation!=view_.generation||state_!=WindowState::Ready)return;
 const auto next=view_.rows[0].index+view_.count;
 if(prefetched_.count||!stream_valid_||scan_row_!=next||(end_known_&&next>=total_rows_))return;
 ++costs_.prefetch_starts;background_=true;resumeText();
}
const CandidateView* CandidateWindow::presented(uint32_t gen)const{
 if(gen&&gen==frame_.generation)return &frame_;
 if(gen&&gen==previous_.generation)return &previous_;
 return nullptr;
}
uint32_t CandidateWindow::framePresented(){if(frame_.generation!=view_.generation||frame_.count!=view_.count){previous_=frame_;frame_=view_;}return frame_.generation;}
ChoiceResult CandidateWindow::choose(size_t column,uint32_t gen,uint32_t action){
 if(choosing_){++metrics.blocked;return ChoiceResult::Busy;}
 if(!gen){++metrics.blocked;return working()?ChoiceResult::Waiting:ChoiceResult::NoCandidate;}
 const auto* v=presented(gen);
 if(!v||v->row<v->rows[0].index||v->row>=v->rows[0].index+v->count){++metrics.stale_choices;state_=WindowState::Stale;changed_=true;return ChoiceResult::ExpiredFrame;}
 const auto& r=v->rows[v->row-v->rows[0].index];
 if(column>=r.count){state_=WindowState::Boundary;changed_=true;return ChoiceResult::Boundary;}
 if(!current(r.items[column])){++metrics.stale_choices;state_=WindowState::Stale;changed_=true;return ChoiceResult::ExpiredInput;}
 choice_=r.items[column];choosing_=true;pending_={};completion_=SelectionCompletion::None;state_=WindowState::Loading;changed_=true;
 action_=action;
 BT_OBSERVE(selection(*this,choice_,gen,action));BT_MEASURE(choose(action));
 if(background_){++costs_.foreground_waits;background_=false;}
 if(!working())startAuth();
 return ChoiceResult::Captured;
}
ChoiceResult CandidateWindow::chooseHighlighted(uint32_t gen,uint32_t action){const auto* v=presented(gen);return choose(v?v->col:0,gen,action);}
bool CandidateWindow::captureDefault(){
 const auto p=c_.page();const auto s=c_.snapshot();
 if(!p.size||page_!=0||p.begin!=s.focus||s.input_id!=input_||s.revision!=revision_)return false;
 choice_={};choice_.key=p.items[0].key;choice_.ref=p.items[0].ref;
 choice_.selection=c_.selection(0);choice_.input=s.input_id;choice_.revision=s.revision;
 choice_.scheme=s.scheme_epoch;choice_.activation=s.activation;
 choice_.begin=p.begin;choice_.end=p.end;choice_.page_generation=p.generation;
 choosing_=true;default_demand_=false;completion_=SelectionCompletion::None;
 action_=default_action_;pending_={};state_=WindowState::Loading;changed_=true;
 BT_OBSERVE(selection(*this,choice_,0,action_));BT_MEASURE(choose(action_));
 return true;
}
ChoiceResult CandidateWindow::chooseDefault(uint32_t action){
 if(choosing_||default_demand_)return ChoiceResult::Busy;
 if(task_==Task::Query){default_demand_=true;default_action_=action;return ChoiceResult::Waiting;}
 // No scan range is guessed. A default request is confined to row zero of
 // this query; a displayed navigation choice uses choose(), not this path.
 if(page_!=0||(!working()&&state_!=WindowState::Ready))return ChoiceResult::NoCandidate;
 default_action_=action;
 if(!captureDefault())return ChoiceResult::ExpiredInput;
 if(!working())startAuth(); // Otherwise hand off at the current operation boundary.
 return ChoiceResult::Captured;
}
ChoiceResult CandidateWindow::chooseInitialNumber(size_t column,uint32_t action){
 // A fixed initial ROW ordinal, resolved only by the normal pixel layout.
 // It is neither a global rank nor a fabricated displayed frame/Selection.
 if(choosing_)return ChoiceResult::Busy;
 if(column>=6||want_row_!=0)return ChoiceResult::NoCandidate;
 if(working())return ChoiceResult::Waiting;
 if(state_!=WindowState::Ready||!view_.count||view_.row!=0||view_.rows[0].index!=0||column>=view_.rows[0].count)return ChoiceResult::NoCandidate;
 const auto& item=view_.rows[0].items[column];if(!current(item))return ChoiceResult::ExpiredInput;
 choice_=item;choosing_=true;pending_={};completion_=SelectionCompletion::None;
 action_=action;state_=WindowState::Loading;changed_=true;BT_OBSERVE(selection(*this,choice_,view_.generation,action));BT_MEASURE(choose(action));startAuth();return ChoiceResult::Captured;
}
void CandidateWindow::startAuth(){
 if(!current(choice_)){fail(WindowState::Stale);++metrics.stale_choices;return;}
 const auto p=c_.page();
 if(p.begin!=choice_.begin||p.end!=choice_.end){fail(WindowState::Stale);return;}
 // Exact full identity + range, using adjacent existing ranges. No longest-
 // prefix query, First replay, text lookup, or fabricated old Selection.
 if(page_==choice_.page){authorize();return;}
 ++metrics.reauthorizations;movePage(page_<choice_.page?hanime::RangeDirection::After:hanime::RangeDirection::Before,Task::Auth);
}
void CandidateWindow::authorize(){
 const auto p=c_.page();
 if(!current(choice_)||p.begin!=choice_.begin||p.end!=choice_.end||choice_.slot>=p.size||p.items[choice_.slot].key!=choice_.key||p.items[choice_.slot].ref!=choice_.ref){++metrics.stale_choices;fail(WindowState::Stale);return;}
 last_output_=hanime::OutputStatus::Cancelled;
 const auto s=c_.select(c_.selection(choice_.slot));last_status_=s;
 if(s!=Status::Working){failStatus(s);return;}
 traceUi(UiTraceKind::SelectionResult,&c_,this,nullptr,&choice_,uint8_t(s));
 task_=Task::Select;op_=c_.operation();
}
void CandidateWindow::tickDone(Status s){
 if(default_demand_&&task_==Task::Query&&s==Status::PageReady){
  if(!captureDefault()){fail(WindowState::Stale);return;}
  task_=Task::None;startAuth();return;
 }
 if(s==Status::PageReady){
  forward_more_=task_==Task::Query?c_.page().has_more:last_move_before_||c_.page().has_more;
  // Keep the first usable range timestamp for this action. Page counts stay
  // in WindowCosts; a deep scan must not fill the trace ring with every range.
  if(!range_traced_&&!background_){CPATTR(cpattr::range());traceUi(UiTraceKind::RangeReady,&c_,this);range_traced_=true;}
 }
 if(task_==Task::Text&&s==Status::Ready&&uiTrace.detailed())traceUi(UiTraceKind::ItemTextReady,&c_,this,nullptr,&reading_);
 if(choosing_&&(task_==Task::Query||task_==Task::Page||task_==Task::Seek||task_==Task::Text)&&(s==Status::PageReady||s==Status::Ready)){
  task_=Task::None;startAuth();return; // Stop hidden/demand text at its current public-operation boundary.
 }
 if(task_==Task::Query||task_==Task::Page||task_==Task::Seek){
  if(s==Status::PageReady){stream_valid_=true;if(task_==Task::Seek)seek();else beginText();return;}
  if(s==Status::RangeExhausted){end_known_=true;total_rows_=scan_row_+(row_.count?1:0);if(!finishRow())finishScan(true);return;}
  if(s==Status::NoPrefixMatch||s==Status::NoMappingForPrefix||s==Status::Unresolved){queryFailed(s);return;}
 }else if(task_==Task::Text&&s==Status::Ready){appendText();return;}
 else if(task_==Task::Auth&&s==Status::PageReady){startAuth();return;}
 else if((task_==Task::Select||task_==Task::Recheck)&&s==Status::Ready){
  if(task_==Task::Select){BT_OBSERVE(selectionResult(c_,1));}
  else {BT_OBSERVE(revalidated(c_,action_));}
  completion_=task_==Task::Select?SelectionCompletion::Choice:SelectionCompletion::Recheck;
  traceUi(UiTraceKind::SelectionDone,&c_,this,nullptr,choosing_?&choice_:nullptr);
  task_=Task::None;choosing_=false;clearView(view_);state_=WindowState::Selected;changed_=true;return;
 }
 failStatus(s,last_output_);
}
void CandidateWindow::service(bool allow_prefetch){
 CPATTR_TIMER(cp_window,Window);
 if(task_==Task::CachedText){appendText();return;}
 if(background_&&!allow_prefetch&&!choosing_&&!pending_.valid)return;
 if(!working()){if(allow_prefetch)prefetch();return;}
 const auto start=clockUs();const auto io=metrics.io();const auto task=task_;const bool hidden=background_;
 const auto r=c_.tick(op_,Resources::Budget);
 if(hidden)++costs_.prefetch_steps;
 metrics.step(hidden?Phase::RowPrefetch:task==Task::Text?Phase::VisibleText:Phase::Query,start,r.core_cost.read_calls,r.core_cost.read_bytes,r.core_cost.work_items,io);
 CPATTR(cpattr::interval(cpattr::Core,start,metrics.phases[unsigned(hidden?Phase::RowPrefetch:task==Task::Text?Phase::VisibleText:Phase::Query)].last_us));
 if(r.status!=Status::Working){last_output_=r.output;tickDone(r.status);}
}
}
