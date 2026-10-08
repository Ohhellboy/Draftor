#define main lc1_previous_rows_main
#include "query_rows.cpp"
#undef main
#include "ui_trace.hpp"
void ready(CandidateWindow& w){unsigned n=0;while(w.busy()){w.service();CHECK(++n<2000000);}UiTraceRecord r;while(uiTrace.pop(r)){} }
void rejected(bool members){
 Rig r;r.begin(false,"wo",false);
 auto cfg=hanime_extension::Config{};cfg.members=members?1:4;cfg.text_bytes=members?512:3;
 hanime_extension::Composition c(r.resources.scheme,r.resources.shared,r.resources.rules(),cfg);
 CandidateWindow w(c,r.font);CHECK(c.edit_raw(0,0,"wowo")==hanime_extension::Status::Ready);
 w.query(2);ready(w);CHECK(w.state()==WindowState::Ready);CHECK(w.choose(0,w.framePresented())==ChoiceResult::Captured);ready(w);CHECK(w.state()==WindowState::Selected);
 w.query(4);ready(w);CHECK(w.state()==WindowState::Ready);const auto generation=w.framePresented();const auto view=digest(w.view());
 auto before=c.snapshot();const auto rev=before.revision;std::string text(before.selected_text);const auto member=before.members[0];
 CHECK(w.choose(0,generation)==ChoiceResult::Captured);ready(w);auto after=c.snapshot();
 CHECK(w.state()==WindowState::Capacity&&w.lastStatus()==hanime_extension::Status::CapacityExceeded);
 CHECK(after.raw=="wowo"&&after.revision==rev&&after.focus==2&&after.member_count==1&&after.selected_text==text);
 CHECK(after.members[0].key==member.key&&after.members[0].ref==member.ref&&after.members[0].begin==member.begin&&after.members[0].end==member.end);
 CHECK(w.completion()==SelectionCompletion::None&&!w.selecting()&&digest(w.view())==view&&r.app.document.text().empty());
 CHECK(c.edit_raw(0,4,"wo")==hanime_extension::Status::Ready);w.query(2);ready(w);CHECK(w.state()==WindowState::Ready);
 CHECK(w.choose(0,w.framePresented())==ChoiceResult::Captured);ready(w);CHECK(w.state()==WindowState::Selected&&c.snapshot().focus==2);
 std::cout<<"PASS window_"<<(members?"members":"total_text")<<" capacity_preserves_and_edit_recovers unknown_detail=1\n";
}
void boundaries(){
 Rig r;r.begin(false,"wo",false);auto& c=*r.ime.composition();auto& w=*r.ime.window();const auto frame=r.frame;
 CHECK(c.read_text(c.selection(0))==hanime_extension::Status::Working);const auto operation=c.operation();
 CHECK(w.choose(0,frame)==ChoiceResult::Captured);CHECK(w.state()==WindowState::Busy);CHECK(w.lastStatus()==hanime_extension::Status::Busy);
 CHECK(c.cancel(operation)==hanime_extension::Status::Cancelled);w.query(2);ready(w);const auto fresh=w.framePresented();
 CHECK(c.edit_raw(0,2,"ui")==hanime_extension::Status::Ready);CHECK(w.choose(0,fresh)==ChoiceResult::ExpiredInput);CHECK(w.state()==WindowState::Stale);
 w.query(2);ready(w);CHECK(w.state()==WindowState::Ready);
 CHECK(c.edit_raw(0,2,"qqqqqqqq")==hanime_extension::Status::Ready);w.query(8);ready(w);CHECK(w.state()==WindowState::NoResults);
 std::cout<<"PASS window_busy_stale_nomatch_distinct\n";
}
int main(){try{rejected(true);rejected(false);boundaries();std::cout<<"LC1_WINDOW cases=3\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
