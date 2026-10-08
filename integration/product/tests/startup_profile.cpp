#include "tf_resources.hpp"
#include "chinese_font.hpp"
#include "candidate_window.hpp"
#include <iostream>
#include <array>
#include <stdexcept>
#include <string>
using namespace ime_device;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
namespace {
struct Cost {
 IoCounters io{};uint64_t steps=0,work=0,us=0,wall=0;
 std::array<uint64_t,1025> sizes{};
};
void print(const char* mode,const char* stage,const Cost& c){
 std::cout<<"COST mode="<<mode<<" stage="<<stage<<" steps="<<c.steps<<" work="<<c.work<<" exec_us="<<c.us<<" wall_us="<<c.wall
 <<" logical_calls="<<c.io.requests<<" logical_bytes="<<c.io.requested_bytes<<" physical_calls="<<c.io.reads<<" physical_bytes="<<c.io.bytes
 <<" hits="<<c.io.hits<<" hit_bytes="<<c.io.hit_bytes<<" seeks="<<c.io.seeks<<" seek_us="<<c.io.seek_us<<" read_us="<<c.io.read_us<<" sizes=";
 bool first=true;for(size_t i=0;i<c.sizes.size();++i)if(c.sizes[i]){if(!first)std::cout<<',';first=false;std::cout<<i<<':'<<c.sizes[i];}std::cout<<'\n';
}
// Host-only diagnostic: classify observed logical I/O from this frozen file's
// section offsets and the inspected validation read order. Not a device API,
// not a replacement validator. CPU ticks can span subphases, so only aggregate
// shared CPU time is reported. Every logical byte is accounted for exactly once.
class TraceReader final:public hanime::ResourceReader {
 mutable TfReader file_;
 mutable unsigned stage_=0;
 mutable uint64_t hash_next_=40,pages_next_=0;
 hanime::PagedSharedInfo info_{};bool shared_=false;
 public:
 mutable std::array<Cost,8> stages{};
 mutable Cost total;
 TraceReader(std::shared_ptr<Mount> m,std::string path,size_t lines):file_(std::move(m),path,lines){}
 size_t size()const override{return file_.size();}
 void sharedLayout(const hanime::PagedSharedInfo& i){info_=i;shared_=true;pages_next_=i.sections[0].offset;stage_=1;}
 void finishValidation(){shared_=false;}
 bool read(size_t off,void* dst,size_t n)const override{
  if(shared_){
   if(stage_==1&&hash_next_==info_.file_bytes)stage_=2;
   if(stage_==2&&pages_next_==info_.payload_offset)stage_=3;
   if(stage_==1){CHECK(off==hash_next_);hash_next_+=n;}
   else if(stage_==2){CHECK(off==pages_next_);pages_next_+=n;}
   else if(stage_!=7){
    if(off>=info_.sections[4].offset&&off<info_.payload_offset)stage_=7;
    else if(off>=info_.sections[3].offset)stage_=6;
    else if(off>=info_.sections[2].offset)stage_=5;
    else if(off>=info_.sections[1].offset)stage_=4;
   }
  }
  const auto before=file_.io();const bool ok=file_.read(off,dst,n);const auto delta=file_.io()-before;
  auto& c=stages[stage_];c.io.add(delta);++c.sizes.at(n);total.io.add(delta);++total.sizes.at(n);return ok;
 }
};
void run(const char* name,size_t lines,size_t pages,size_t work){
 auto mount=std::make_shared<Mount>();auto reader=std::make_shared<TraceReader>(mount,std::string(RESOURCE_ROOT)+"/shared.hns",lines);
 hanime::PagedSharedResource shared;std::string error;
 CHECK(shared.open(reader,Resources::Namespace,0,1,{pages,1024},&error));reader->sharedLayout(shared.info());
 const auto budget=hanime::WorkBudget{1,1024,work};auto cursor=shared.validation_cursor();Cost validation;
 const auto io=metrics.io();auto wall=clockUs();
 for(size_t i=0;i<10000000;++i){auto start=clockUs();auto r=shared.advance_validation(cursor,budget);validation.us+=clockUs()-start;++validation.steps;validation.work+=r.cost.work_items;
  CHECK(r.status==hanime::ValidationStepStatus::InProgress||r.status==hanime::ValidationStepStatus::Ready);if(r.status==hanime::ValidationStepStatus::Ready)break;
 }
 CHECK(shared.ready());validation.wall=clockUs()-wall;validation.io=metrics.io()-io;
 const char* names[]={"shared_header","shared_hash","shared_pages","shared_lexemes","shared_relations","shared_selectors","shared_strings","shared_unique"};
 IoCounters sum{};for(size_t i=1;i<8;++i){sum.add(reader->stages[i].io);for(size_t n=0;n<validation.sizes.size();++n){validation.sizes[n]+=reader->stages[i].sizes[n];}}
 CHECK(sum.requests==validation.io.requests&&sum.requested_bytes==validation.io.requested_bytes);
 CHECK(reader->stages[7].io.requests==shared.info().uniqueness_cost.read_calls&&reader->stages[7].io.requested_bytes==shared.info().uniqueness_cost.read_bytes);
 for(size_t i=0;i<8;++i){print(name,names[i],reader->stages[i]);}print(name,"shared_total",validation);reader->finishValidation();
 hanime::TextAccessCheck admission;CHECK(admission.begin(shared,64,128)==hanime::TextAccessStatus::Pending);
 Cost text;const auto sizesBefore=reader->total.sizes;auto before=metrics.io();wall=clockUs();
 for(size_t i=0;i<10000000;++i){auto start=clockUs();auto r=admission.advance(budget);text.us+=clockUs()-start;++text.steps;text.work+=r.cost.work_items;
  CHECK(r.status==hanime::TextAccessStatus::Pending||r.status==hanime::TextAccessStatus::Ready);if(r.status==hanime::TextAccessStatus::Ready)break;
 }
 CHECK(admission.certificate().current(shared,64,128));text.wall=clockUs()-wall;text.io=metrics.io()-before;for(size_t i=0;i<text.sizes.size();++i){text.sizes[i]=reader->total.sizes[i]-sizesBefore[i];}print(name,"admission",text);
 for(auto rule:{hanime_extension::SchemeRules::R1Xiaohe,hanime_extension::SchemeRules::FullPinyin}){
  auto scheme_reader=std::make_shared<TraceReader>(mount,std::string(RESOURCE_ROOT)+(rule==hanime_extension::SchemeRules::R1Xiaohe?"/xiaohe.hnm":"/full_pinyin.hnm"),lines);
  hanime::PagedSchemeIndex scheme;CHECK(scheme.open(scheme_reader,Resources::Namespace,shared.info().shared_bundle_id,shared.info().sections[0].count,{1,1024},&error));
  Cost sc;const auto schemeSizesBefore=scheme_reader->total.sizes;before=metrics.io();wall=clockUs();for(size_t i=0;i<1000000;++i){auto start=clockUs();auto r=scheme.advance_validation(budget);sc.us+=clockUs()-start;++sc.steps;sc.work+=r.cost.work_items;
   CHECK(r.status==hanime::ValidationStepStatus::InProgress||r.status==hanime::ValidationStepStatus::Ready);if(r.status==hanime::ValidationStepStatus::Ready)break;
  }
  CHECK(scheme.ready());sc.wall=clockUs()-wall;sc.io=metrics.io()-before;for(size_t i=0;i<sc.sizes.size();++i){sc.sizes[i]=scheme_reader->total.sizes[i]-schemeSizesBefore[i];}print(name,rule==hanime_extension::SchemeRules::R1Xiaohe?"scheme_xh":"scheme_py",sc);
  hanime_extension::Composition comp(scheme,shared,rule);s3host::BuiltinLatinFont font;CandidateWindow window(comp,font);
  CHECK(comp.edit_raw(0,0,rule==hanime_extension::SchemeRules::R1Xiaohe?"wjdjle":"wandanle")==hanime_extension::Status::Ready);
  before=metrics.io();wall=clockUs();const auto q0=metrics.phases[unsigned(Phase::Query)],v0=metrics.phases[unsigned(Phase::VisibleText)];window.query();
  for(size_t i=0;i<10000000&&window.busy();++i){window.service();}CHECK(window.state()==WindowState::Ready&&window.view().count);
  Cost first;first.wall=clockUs()-wall;first.io=metrics.io()-before;
  const auto q=metrics.phases[unsigned(Phase::Query)],v=metrics.phases[unsigned(Phase::VisibleText)];first.us=q.us-q0.us+v.us-v0.us;first.steps=q.steps-q0.steps+v.steps-v0.steps;
  print(name,rule==hanime_extension::SchemeRules::R1Xiaohe?"first_window_xh":"first_window_py",first);
  Cost query;query.steps=q.steps-q0.steps;query.us=q.us-q0.us;query.io=q.io-q0.io;print(name,"query_only",query);
  Cost visible;visible.steps=v.steps-v0.steps;visible.us=v.us-v0.us;visible.io=v.io-v0.io;print(name,"visible_only",visible);
 }
}
}
int main(){try{
 std::cout<<"HOST_ONLY fresh owners; OS file cache not flushed; microseconds are host CPU/elapsed, not device throughput. No firmware frequency inference.\n";
 ChineseFont font;auto before=metrics.io();const auto start=clockUs();CHECK(font.begin());Cost fc;fc.io=metrics.io()-before;fc.wall=clockUs()-start;fc.us=metrics.phases[unsigned(Phase::FontLoad)].us;fc.steps=metrics.phases[unsigned(Phase::FontLoad)].steps;print("common","font_read_whole_wall",fc);
 const auto& cm=metrics.phases[unsigned(Phase::FontContent)];Cost content;content.us=cm.us;content.steps=cm.steps;content.work=cm.work;content.wall=cm.last_us-cm.first_us;print("common","font_crc",content);
 const auto& fm=metrics.phases[unsigned(Phase::FontIndex)];Cost fi;fi.io=fm.io;fi.us=fm.us;fi.steps=fm.steps;fi.work=fm.work;fi.wall=fm.last_us-fm.first_us;print("common","font_index",fi);
 run("uncached_2p_128w",0,2,128);run("cache16_2p_128w",16,2,128);run("cache16_2p_1024w",16,2,1024);run("cache16_4p_1024w",16,4,1024);
 std::cout<<"PASS full frozen production validation/admission, both schemes, first visible window, read-order accounting, unique-cost cross-check\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
