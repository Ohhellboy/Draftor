#pragma once
#include "tf_resources.hpp"
#include "font_port.hpp"
#include <array>
namespace ime_device {
struct VisibleItem {
 hanime::StableLexemeKey key;hanime::RuntimeLexemeRef ref=0;
 hanime_extension::Selection selection;
 uint64_t input=0,revision=0,scheme=0,page_generation=0;
 hanime::SharedActivationHandle activation;
 size_t page=0,slot=0,begin=0,end=0,ordinal=0;
 std::array<char,65> text{};uint16_t bytes=0,width=0;
};
struct CandidateRow { std::array<VisibleItem,6> items{};size_t index=0,count=0;uint16_t width=0;bool vertical=false; };
struct CandidateView {
 std::array<CandidateRow,3> rows{};size_t count=0,row=0,col=0;
 uint32_t generation=0,action=0;bool expanded=false,at_end=false;
};
enum class WindowState { Empty, Loading, Ready, NoResults, Boundary, Error, Stale, Selected, Unresolved, Limited, Capacity, Busy, OutputError };
enum class ChoiceResult {Captured,Waiting,Busy,NoCandidate,Boundary,ExpiredFrame,ExpiredInput};
enum class SelectionCompletion { None, Choice, Recheck };
// Three visible rows plus one prepared row; bounded work/presented snapshots.
// Adjacent public ranges and fixed row anchors, never a complete candidate list.
struct WindowCosts {uint64_t materialized_items=0,reused_rows=0,built_rows=0,prefetch_starts=0,prefetch_hits=0,prefetch_misses=0,prefetch_cancelled=0,foreground_waits=0,prefetch_steps=0,range_moves=0,anchor_fallbacks=0;};
class CandidateWindow {
 friend struct CandidateBuildDiagnostics; // Read-only candidate memory receipt.
 using Composition=hanime_extension::Composition;using Status=hanime_extension::Status;
 Composition& c_;const s3host::FontPort& font_;
 enum class Task { None, Query, Page, Seek, Text, CachedText, Auth, Select, Recheck } task_=Task::None;
 hanime_extension::Operation op_;
 CandidateView view_{},building_{},frame_{},previous_{};
 CandidateRow row_{},prefetched_{};VisibleItem reading_{},choice_{};
 size_t page_=0,slot_=0,scan_row_=0,want_row_=0,want_col_=0;
 bool expanded_=false,exact_=false,choosing_=false;
 std::uint8_t occupied_columns_=1; // last complete, current V window; never building_.count
 bool find_profile_=false;
 bool vertical_=false,vertical_seen_=false,stage_expanded_=false,repacking_=false,locating_=false;
 size_t ordinal_=0,focus_ordinal_=0;
 static constexpr unsigned vertical_height=356,vertical_gap=2;
 uint16_t extent(const VisibleItem&)const;
 const VisibleItem* cachedItem(size_t ordinal)const;
 bool default_demand_=false;uint32_t default_action_=0;
 bool forward_more_=false,last_move_before_=false;
 bool reading_ready_=false,stream_valid_=false,end_known_=false,background_=false,prefetch_enabled_=false;
 size_t total_rows_=0,seek_page_=0,seek_slot_=0;
 struct Anchor {size_t row=0,page=0,slot=0;bool valid=false;uint8_t count=0;size_t ordinal=0;bool vertical=false;};
 std::array<Anchor,32> anchors_{};
 WindowCosts costs_{};
 bool probe_only_=false,probe_cache_=false;
 bool working()const{return task_!=Task::None;}
 const CandidateRow* cached(size_t)const;
 size_t reuseCachedRows();
 void continueScan();void seek();void resumeText();void prefetch();
 bool movePage(hanime::RangeDirection,Task);

 struct Navigation {size_t row=0,col=0;bool expand=false,exact=false,valid=false;uint32_t action=0;} pending_;
 uint32_t action_=0,query_serial_=0,layout_serial_=0,stage_serial_=1;
 bool range_traced_=false;
 WindowState state_=WindowState::Empty;
 SelectionCompletion completion_=SelectionCompletion::None;
 uint64_t input_=0,revision_=0;bool changed_=false;
 Status last_status_=Status::Ready;
 hanime::OutputStatus last_output_=hanime::OutputStatus::Cancelled;
 void fail(WindowState);void failStatus(Status,hanime::OutputStatus=hanime::OutputStatus::Cancelled);
 bool current(const VisibleItem&)const;
 void beginScan(Status);
 void queryFailed(Status);
 void beginText();void appendText();bool finishRow();void finishScan(bool end);
 void tickDone(Status);void startAuth();void authorize();
 bool captureDefault();
 uint16_t width(std::string_view)const;
 public:
 CandidateWindow(Composition& c,const s3host::FontPort& f):c_(c),font_(f){}
 hanime_extension::Status tryPrefix(size_t prefix,uint32_t action=0);
 void query(size_t prefix=0,uint32_t action=0);
 bool planningLimited()const;
 void recheck();
 // Retire choice authority on an explicit mode return without claiming a
 // new LCD presentation. Physical generation remains a diagnostic fact.
 void retirePresentedChoices(){frame_.count=0;previous_.count=0;}
 void cancel();void service(bool allow_prefetch=false);
 // Spatial regrouping over the existing query and bounded materialized text.
 // It does not call query_next/choose_prefix or alter candidate identity.
 bool setPacking(bool vertical,bool find);
 bool setVerticalPacking(bool vertical){return setPacking(vertical,find_profile_);}
 bool findProfile()const{return find_profile_;}
 bool verticalPacking()const{return vertical_;}
 bool packingPending()const{return repacking_;}
 std::uint32_t candidateStage()const{return stage_serial_;}
 void beginCandidateStage(){occupied_columns_=1;stage_expanded_=expanded_=false;if(stage_serial_<UINT32_MAX)++stage_serial_;}
 const VisibleItem* focusedCurrent()const;
 bool focusedNeedsWholeText()const;
 void endSession(){beginCandidateStage();vertical_seen_=false;}
 void setPrefetch(bool enabled){prefetch_enabled_=enabled;}
 bool prefetchPending()const{return background_;}
 size_t preparedRows()const{return prefetched_.count?1:0;}
 const WindowCosts& costs()const{return costs_;}
 void request(size_t row,size_t col,bool expand,bool exact=false,uint32_t action=0);
 bool rowCount(size_t row,size_t& count)const;
 bool rowBeyondEnd(size_t row)const{return end_known_&&row>=total_rows_;}
 void probeRow(size_t row,uint32_t action);
 ChoiceResult choose(size_t column,uint32_t presented_generation,uint32_t action=0);
 ChoiceResult chooseHighlighted(uint32_t presented_generation,uint32_t action=0);
 // A current-input default demand is not an LCD event or a Selection. Only
 // the formal query's first PageReady may resolve it, never planner metadata.
 ChoiceResult chooseDefault(uint32_t action);
 ChoiceResult chooseInitialNumber(size_t column,uint32_t action);
 const VisibleItem* capturedChoice()const{return choosing_?&choice_:nullptr;}
 bool selecting()const{return choosing_;}
 uint32_t querySerial()const{return query_serial_;}
 uint32_t layoutSerial()const{return layout_serial_;}
 uint32_t actionId()const{return action_;}
 size_t requestedRow()const{return want_row_;}
 size_t requestedCol()const{return want_col_;}
 // Read-only browse intent, independent of materialized/presented row count.
 bool browsingExpanded()const{return stage_expanded_||view_.expanded;}
 // Geometry retains this coherent value during async preparation/error. A
 // new semantic stage resets it; demand remains independently up to 3 columns.
 unsigned occupiedColumns()const{return occupied_columns_;}
 const CandidateView& view()const{return view_;}
 const CandidateView* presented(uint32_t)const;
 uint32_t framePresented();
 // A painted nonselectable surface acknowledges pixels without inventing choices.
 bool frameHidden(uint32_t generation){if(view_.generation!=generation)return false;retirePresentedChoices();frame_.generation=generation;return true;}
 uint32_t onScreenGeneration()const{return frame_.generation;}
 bool busy()const{return working()&&(!background_||pending_.valid);}
 WindowState state()const{return state_;}
 Status lastStatus()const{return last_status_;}
 hanime::OutputStatus lastOutput()const{return last_output_;}
 // Borrowed until the next window mutation. Completion describes this window's
 // own public select/revalidate result, never an arbitrary AllSelected snapshot.
 SelectionCompletion completion()const{return completion_;}
 const VisibleItem* completedChoice()const{return completion_==SelectionCompletion::Choice?&choice_:nullptr;}
 bool changed(){const auto r=changed_;changed_=false;return r;}
};
}
