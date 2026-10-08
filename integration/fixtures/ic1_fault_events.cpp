#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
#include "composition_oracle.hpp"
namespace ime_device {extern bool ic1_throw_after_accept;
 }
int main(){try{
 for(bool full_pinyin:{false,true}){
 LongRig r("ic1_unknown");
 r.app.layoutKey({1,++r.now,61,0,true});
 r.app.layoutKey({1,++r.now,61,0,false});
 r.start(full_pinyin,full_pinyin?"shang":"wo");
 const auto peak=r.app.compositionWidth();
 CHECK(peak==compositionOraclePeak(full_pinyin?"shang":"wo"));
 r.ime.window()->request(0,0,true);
 r.ready();
 CHECK(r.ime.window()->browsingExpanded());
 ic1_throw_after_accept=true;
 r.key(44);
 r.ready();
 ic1_throw_after_accept=false;
 CHECK(r.ime.receipt().status==hanime::CompositionStatus::UnknownOutcome);
 const std::string body(r.app.document.text());
 CHECK(!body.empty());
 CHECK(r.app.compositionWidth()==peak&&r.ime.window()->browsingExpanded()&&r.app.selectedAppeared());
 const auto input=r.ime.composition()->raw_input().stamp;
 const auto raw=std::string(r.ime.raw());
 const auto selected=std::string(r.ime.selected());
 for(unsigned i=0;i<3;++i){r.key(0x29);
 for(unsigned j=0;j<8;++j)r.step();
 r.drain();
 CHECK(r.ime.receipt().status==hanime::CompositionStatus::UnknownOutcome&&std::string_view(r.app.document.text())==body&&r.ime.raw()==raw&&r.ime.selected()==selected&&r.ime.composition()->input_is_current(input));
 }
 CHECK(r.app.compositionWidth()==peak&&r.ime.window()->browsingExpanded()&&r.app.selectedAppeared());
 CHECK(!r.ime.chinese());
 r.key(0x39);
 CHECK(!r.ime.chinese());
 CHECK(r.ime.submission()->cancel(r.ime.submission()->result().transaction).core.status==hanime::CompositionStatus::UnknownOutcome);
 std::cout<<"PASS esc_unknown_quarantine actual_body_acceptance_then_throw history_retained no_replay peak_retained\n";
 }return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';
 return 1;
 }}
