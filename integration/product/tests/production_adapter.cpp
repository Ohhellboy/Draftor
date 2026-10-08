#include "candidate_window.hpp"
#include "chinese_font.hpp"
#include "hanime_extension/submission.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ime_device;
using namespace hanime_extension;
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace {
void mount(Resources& r){
 const auto start=clockUs();const auto before=metrics.io();size_t steps=0;
 for(;!r.ready()&&steps<128;++steps){r.service();CHECK(r.state()!=ResourceState::Error);}CHECK(r.ready());
 const auto io=metrics.io()-before;CHECK(io.bytes<=32768&&io.reads<=32);
 std::cout<<"HOST_ACTIVATION scheme="<<(r.rules()==SchemeRules::R1Xiaohe?"XH":"PY")<<" steps="<<steps<<" wall_us="<<clockUs()-start<<" logical="<<io.requests<<" logical_bytes="<<io.requested_bytes<<" reads="<<io.reads<<" bytes="<<io.bytes<<" seeks="<<io.seeks<<" read_us="<<io.read_us<<" seek_us="<<io.seek_us<<" auth_payload="<<r.cacheBytes()<<" handles="<<metrics.resource_handles<<'\n';
}
Status drain(Composition& c,Status s){auto op=c.operation();for(unsigned i=0;s==Status::Working&&i<5000000;++i)s=c.tick(op,Resources::Budget).status;return s;}
void select(Composition& c,size_t end,std::string_view wanted){
 CHECK(drain(c,c.choose_prefix(end))==Status::PageReady);
 for(unsigned pages=0;pages<1000;++pages){
  const auto p=c.page();
  for(size_t i=0;i<p.size;++i){auto choice=c.selection(i);CHECK(drain(c,c.read_text(choice))==Status::Ready);if(c.last_text()==wanted){CHECK(drain(c,c.select(choice))==Status::Ready);return;}}
  if(!p.has_more)break;
  CHECK(drain(c,c.request_page(hanime::RangeDirection::After))==Status::PageReady);
 }
 throw std::runtime_error("production output absent: "+std::string(wanted));
}
struct Sink:hanime::TextCommitSink{std::string body;unsigned calls=0;bool reject=true;bool commit(std::string_view text)override{++calls;if(reject)return false;body+=text;return true;}};
void submit(Resources& r,Composition& c,std::string_view expected){
 Sink sink;Submission s(c,r.shared,sink);auto result=s.prepare();auto first=result.transaction;
 for(unsigned i=0;result.core.status==hanime::CompositionStatus::Preparing&&i<100000;++i)result=s.advance(first,Resources::Budget).result;
 CHECK(result.core.status==hanime::CompositionStatus::Ready);result=s.confirm(first);CHECK(result.core.status==hanime::CompositionStatus::RetryableNoEffect&&sink.body.empty());
 auto retry=s.retry(first);auto second=retry.transaction;CHECK(second.attempt()!=first.attempt());
 // Simulates a queued device cancel/confirm containing its original attempt.
 CHECK(s.cancel(first).core.status==hanime::CompositionStatus::Stale);CHECK(s.confirm(first).core.status==hanime::CompositionStatus::Stale);CHECK(c.cancel_input(c.snapshot().input_id)==Status::Busy);
 for(unsigned i=0;retry.core.status==hanime::CompositionStatus::Preparing&&i<100000;++i)retry=s.advance(second,Resources::Budget).result;
 CHECK(retry.core.status==hanime::CompositionStatus::Ready);sink.reject=false;CHECK(s.confirm(second).core.status==hanime::CompositionStatus::Accepted);CHECK(sink.body==expected&&sink.calls==2);CHECK(s.confirm(second).core.status==hanime::CompositionStatus::Accepted&&sink.calls==2);
 std::cout<<"PASS production full text "<<expected<<" preparation_reads="<<s.costs().preparation.read_calls<<" bytes="<<s.costs().preparation.read_bytes<<'\n';
}
void scenario(Resources& r,SchemeRules rule){
 Composition c(r.scheme,r.shared,rule);const bool x=rule==SchemeRules::R1Xiaohe;
 for(unsigned n=0;n<4;++n){
  const std::string raw=n==0?(x?"wojtdeba":"wojuedeba"):n==1?(x?"xpwjle":"xiewanle"):n==2?"wode":(x?"wjdjle":"wandanle");
  CHECK(c.edit_raw(0,0,raw)==Status::Ready);
  if(n==0){select(c,2,"我");select(c,x?6:7,"觉得");select(c,x?8:9,"吧");submit(r,c,"我觉得吧");}
  if(n==1){select(c,x?2:3,"写");select(c,x?4:6,"完");select(c,x?6:8,"了");submit(r,c,"写完了");}
  if(n==2){select(c,2,"我");select(c,4,"的");submit(r,c,"我的");}
  if(n==3){select(c,x?4:6,"完蛋");select(c,x?6:8,"了");CHECK(c.reopen_segment(0)==Status::Ready);select(c,x?2:3,"玩");select(c,x?4:6,"蛋");CHECK(drain(c,c.revalidate())==Status::Ready);if(c.snapshot().state!=CompositionState::AllSelected)select(c,x?6:8,"了");submit(r,c,"玩蛋了");}
 }
 // Real bounded display with demand beyond the first six results.
 ChineseFont font;CHECK(font.begin());CandidateWindow window(c,font);
 CHECK(c.edit_raw(0,0,x?"wj":"xian")==Status::Ready);window.query();
 for(unsigned i=0;window.busy()&&i<5000000;++i)window.service();
 CHECK(window.state()==WindowState::Ready);window.framePresented();window.request(10,0,true);
 for(unsigned i=0;window.busy()&&i<10000000;++i)window.service();
 CHECK(window.state()==WindowState::Ready);CHECK(window.view().row==10&&window.view().count==3);
 std::cout<<"PASS production visible deep row 10 scheme="<<(x?"xiaohe":"full_pinyin")<<" resident_rows="<<window.view().count<<'\n';
}
}
int main(){try{
 Resources r(RESOURCE_ROOT,std::make_shared<Mount>(),productionPins(),INDEX_ROOT);r.start(SchemeRules::R1Xiaohe);mount(r);auto admitted=metrics.phases[unsigned(Phase::Admission)].steps;
 CHECK(metrics.phases[unsigned(Phase::Admission)].io.bytes==0&&admitted==1);
 {
  ChineseFont font;CHECK(font.begin());Composition c(r.scheme,r.shared,SchemeRules::R1Xiaohe);CandidateWindow w(c,font);
  const auto start=clockUs();const auto before=metrics.io();CHECK(c.edit_raw(0,0,"wjdjle")==Status::Ready);w.query();unsigned steps=0;
  for(;w.busy()&&steps<100000;++steps){w.service();}
  CHECK(w.state()==WindowState::Ready&&w.view().count<=3);
  const auto io=metrics.io()-before;
  std::cout<<"HOST_FIRST_WINDOW raw=wjdjle rows="<<w.view().count<<" steps="<<steps<<" wall_us="<<clockUs()-start<<" logical="<<io.requests<<" logical_bytes="<<io.requested_bytes<<" reads="<<io.reads<<" bytes="<<io.bytes<<" seeks="<<io.seeks<<" read_us="<<io.read_us<<" seek_us="<<io.seek_us<<" scope=memory_window_not_LCD\n";
 }
 scenario(r,SchemeRules::R1Xiaohe);
 r.switchScheme(SchemeRules::FullPinyin);mount(r);CHECK(metrics.phases[unsigned(Phase::Admission)].steps==admitted);scenario(r,SchemeRules::FullPinyin);
 for(unsigned i=0;i<unsigned(Phase::Count);++i){const auto& m=metrics.phases[i];std::cout<<"PHASE "<<i<<" steps="<<m.steps<<" us="<<m.us<<" max_us="<<m.max_us<<" reads="<<m.read_calls<<" bytes="<<m.read_bytes<<" work="<<m.work<<'\n';}
 std::cout<<"HOST_ONLY driver_max_read_us="<<metrics.max_read_us<<" seek_us="<<metrics.max_seek_us<<" open_us="<<metrics.max_open_us<<" handles_peak="<<metrics.handle_peak<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
