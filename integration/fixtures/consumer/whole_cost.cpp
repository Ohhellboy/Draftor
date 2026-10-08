#ifdef C4_TRACE_ENABLED
#include "c4_trace.hpp"
#endif
#include "verified_runtime.hpp"
#include "hanime_extension/lc1_config.hpp"
#include "probe_hooks.hpp"
#include "allocation_probe.hpp"
#include <array>
#include "tracked_backend.hpp"
#include <iomanip>
using namespace extension_host;
namespace {
struct Read {std::size_t role{},offset{},bytes{};bool ok{},jump{};std::size_t range_index{};};
struct Trace {std::array<Read,4096> rows{};std::size_t count{};bool overflow{};} trace;
struct File final:ResourceReader {
 HostResourceFile inner;std::size_t role;mutable bool seen{false};mutable std::size_t end{};
 File(std::string path,std::size_t id,HostResourceMetrics& m):inner(path,1024,m),role(id){require(inner.valid(),"open fixed resource");}
 bool healthy()const override{return inner.healthy();}std::size_t size()const override{return inner.size();}
 bool read(std::size_t off,void* dst,std::size_t n)const override{bool ok=inner.read(off,dst,n);if(trace.count<trace.rows.size())trace.rows[trace.count++]={role,off,n,ok,seen&&end!=off,observe::counters.count};else trace.overflow=true;seen=true;end=off+n;return ok;}
};
const char* roles[]={"shared_packet","scheme_data","scheme_proof","order_data","order_proof"};
struct R:VerifiedRuntime {
 TrackedBackend storage;
 std::string scheme_id,scenario,op;std::size_t seq{};HostResourceMetrics baseline;BaseOrderLocateStats loc;
 std::size_t core_limit{200000}, local_limit{200000};
 std::chrono::steady_clock::time_point clock;std::uint64_t steps{},work{},local{},max_core{},max_local{},max_reads{},max_bytes{};
 void start(std::string label){op=std::move(label);++seq;
#ifdef C4_TRACE_ENABLED
 c4::operation(scheme_id,scenario,op,seq);
#endif
trace={};baseline=metrics;loc=order?order->locate_stats():BaseOrderLocateStats{};allocation_probe::reset_peak();clock=std::chrono::steady_clock::now();steps=work=local=max_core=max_local=max_reads=max_bytes=0;}
 void stage(const char* phase,Status status){
  auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-clock).count();const auto now=order?order->locate_stats():BaseOrderLocateStats{};const auto alloc=allocation_probe::stats();
  require(!trace.overflow,"bounded read trace overflow");require(trace.count==metrics.read_calls-baseline.read_calls,"physical reconciliation");
  std::cout<<"{\"kind\":\"stage\",\"scenario\":"<<std::quoted(scenario)<<",\"op\":"<<std::quoted(op)<<",\"op_id\":"<<seq<<",\"phase\":"<<std::quoted(phase)<<",\"status\":"<<std::quoted(name(status))<<",\"reads\":"<<trace.count<<",\"bytes\":"<<metrics.delivered_bytes-baseline.delivered_bytes<<",\"work\":"<<work<<",\"local\":"<<local<<",\"steps\":"<<steps<<",\"host_ns\":"<<ns<<",\"pread_ns\":"<<metrics.calls_ns-baseline.calls_ns<<",\"new_live\":"<<alloc.live<<",\"new_peak\":"<<alloc.peak<<",\"new_requested\":"<<alloc.requested<<",\"new_allocations\":"<<alloc.allocations<<",\"directory_work\":"<<now.directory.work_items-loc.directory.work_items<<",\"item_work\":"<<now.items.work_items-loc.items.work_items<<",\"member_work\":"<<now.members.work_items-loc.members.work_items<<",\"locate_hits\":"<<now.hits-loc.hits<<",\"locate_misses\":"<<now.misses-loc.misses<<",\"locate_evictions\":"<<now.evictions-loc.evictions<<",\"file_controls\":"<<metrics.control_calls-baseline.control_calls<<"}\n";
  std::cout<<"{\"kind\":\"step_max\",\"op_id\":"<<seq<<",\"phase\":"<<std::quoted(phase)<<",\"core\":"<<max_core<<",\"local\":"<<max_local<<",\"reads\":"<<max_reads<<",\"bytes\":"<<max_bytes<<"}\n";
  for(std::size_t i=0;i<trace.count;++i){const auto& x=trace.rows[i];require(x.ok,"physical read error");std::cout<<"{\"kind\":\"read\",\"op_id\":"<<seq<<",\"phase\":"<<std::quoted(phase)<<",\"role\":"<<std::quoted(roles[x.role])<<",\"offset\":"<<x.offset<<",\"bytes\":"<<x.bytes<<",\"discontinuous\":"<<x.jump<<",\"range_index\":"<<x.range_index<<"}\n";}
 }
 void mount(const char* logical,const char* transport,const std::string& id,std::size_t pool){
  scheme_id=id;const auto l=std::filesystem::path(logical),t=std::filesystem::path(transport);
  auto open=[&](const auto& data,const auto& proof,const ResourceReleaseSpec& spec,std::size_t blocks,std::size_t role,bool anchors){
   std::shared_ptr<File>d;if(spec.verification_format==1)d=std::make_shared<File>(data.string(),role,metrics);
   auto h=std::make_shared<File>(proof.string(),spec.verification_format==1?role+1:role,metrics);
   VerifiedResourceConfig cfg{blocks,8,1024};cfg.packed_anchor_slots=anchors?16:0;
   auto v=std::make_shared<VerifiedResourceReader>(d,h,spec,cfg);require(v->prepare_bootstrap(),"bootstrap");
   for(std::size_t i=0;!v->bootstrap_ready();++i){require(i<1000000,"bootstrap bound");auto x=v->advance_bootstrap(budget);work+=x.cost.work_items;++steps;require(v->healthy(),"bootstrap healthy");}return v;
  };
  start("mount");shared_reader=open(std::filesystem::path{},t/"shared.hnv",hanime_release::shared,16,0,true);
  scheme_reader=open(l/(id+".hnm"),t/(id+".hnv"),id=="xiaohe"?hanime_release::xiaohe:hanime_release::full_pinyin,16,1,false);
  order_reader=open(t/(id+"_base.hbo"),t/(id+"_base.hnv"),id=="xiaohe"?hanime_release::xiaohe_base:hanime_release::full_pinyin_base,8,3,false);
  std::string err;PagedSharedConfig cfg{2,1024,true};cfg.output_reuse={12,64,64};require(shared.open_verified(shared_reader,1,cfg,&err),err.c_str());require(scheme.open_verified(scheme_reader,shared,{1,1024},&err),err.c_str());
  order=std::make_shared<BaseOrderIndex>(order_reader,scheme,shared,pool);require(order->ready()&&scheme.attach_base_order(order)&&order->locate_stats().capacity==pool,"fixed pool");stage("bootstrap",Status::Ready);
  start("admission");require(admit()==TextAccessStatus::Ready,"text admission");work=admission.work;steps=admission.steps;stage("text_admission",Status::Ready);
 }
 void step(const Step& step_result) {
  ++steps;
  work += step_result.core_cost.work_items;
  local += step_result.local_work;
  max_core = std::max(max_core, static_cast<std::uint64_t>(step_result.core_cost.work_items));
  max_local = std::max(max_local, static_cast<std::uint64_t>(step_result.local_work));
  max_reads = std::max(max_reads, static_cast<std::uint64_t>(step_result.core_cost.read_calls));
  max_bytes = std::max(max_bytes, static_cast<std::uint64_t>(step_result.core_cost.read_bytes));
  require(step_result.core_cost.read_calls <= 1 && step_result.core_cost.read_bytes <= 1024 &&
      step_result.core_cost.work_items <= 128 && step_result.local_work <= 128, "slice budget");
 }
 void report(Composition& c,Status s){auto p=c.planning_report();auto snap=c.snapshot();const auto& o=observe::counters;require(!o.overflow,"range trace bounded");
  std::cout<<"{\"kind\":\"plan\",\"scenario\":"<<std::quoted(scenario)<<",\"op\":"<<std::quoted(op)<<",\"op_id\":"<<seq<<",\"raw\":"<<std::quoted(std::string(snap.raw))<<",\"input_id\":"<<snap.input_id<<",\"revision\":"<<snap.revision<<",\"epoch\":"<<snap.scheme_epoch<<",\"status\":"<<std::quoted(name(s))<<",\"valid\":"<<p.valid<<",\"limited\":"<<p.limited<<",\"incomplete\":"<<p.incomplete_tail<<",\"unknown\":"<<p.unknown_tail<<",\"direct\":"<<p.direct_range<<",\"focus\":"<<snap.focus<<",\"end\":"<<c.page().end<<",\"covered\":"<<p.covered_end<<",\"checked\":"<<p.checked_ranges<<",\"queries\":"<<p.queries<<",\"hits\":"<<p.cache_hits<<",\"planning_work\":"<<p.core_cost.work_items<<",\"planning_reads\":"<<p.core_cost.read_calls<<",\"planning_bytes\":"<<p.core_cost.read_bytes<<",\"local_work\":"<<p.local_work<<",\"command_work\":"<<c.command_work()<<",\"words\":"<<p.words<<",\"context\":"<<p.context_revision<<",\"score\":"<<p.score<<",\"syllables\":"<<p.syllables<<"}\n";
#ifdef OBSERVATIONAL
  std::cout<<"{\"kind\":\"prefix_stats\",\"op_id\":"<<seq<<",\"checks\":"<<o.prefix_checks<<",\"resets\":"<<o.prefix_resets<<",\"offers\":"<<o.prefix_offers<<",\"evictions\":"<<o.prefix_evictions<<",\"saves\":"<<o.prefix_saves<<"}\n";
  std::cout<<"{\"kind\":\"local\",\"op_id\":"<<seq<<",\"reusable\":"<<o.reusable<<",\"prefix\":"<<o.prefix<<",\"retained\":"<<o.retained<<",\"retired\":"<<o.retired<<",\"legal_intervals\":"<<o.legal_intervals<<",\"primary_intervals\":"<<o.primary_intervals<<",\"offers\":"<<o.offers<<",\"slot_checks\":"<<o.slot_checks<<",\"dp_edges\":"<<o.dp_edges<<",\"dp_updates\":"<<o.dp_updates<<",\"formal_calls\":"<<o.formal<<",\"feasibility_calls\":"<<o.feasibility<<",\"phases\":[";
  for(std::size_t i=0;i<o.phases.size();++i){std::cout<<(i?",":"")<<o.phases[i];}
  std::cout<<"]}\n";
  for(std::size_t i=0;i<o.count;++i){const auto& x=o.ranges[i];std::cout<<"{\"kind\":\"range\",\"op_id\":"<<seq<<",\"begin\":"<<x.begin<<",\"end\":"<<x.end<<",\"code\":"<<std::quoted(std::string(snap.raw.substr(x.begin,x.end-x.begin)))<<",\"hit\":"<<x.hit<<",\"terminal\":"<<x.terminal<<",\"reads\":"<<x.reads<<",\"bytes\":"<<x.bytes<<",\"work\":"<<x.work<<",\"units\":"<<x.units<<",\"weight\":"<<x.weight<<",\"matched\":"<<x.matched<<",\"key\":"<<std::quoted(format_slk1(x.item.key))<<",\"ref\":"<<x.item.ref<<",\"score\":"<<x.item.score<<",\"paths\":"<<x.item.path_count<<",\"count\":"<<x.item.effective_count<<",\"ordinal\":"<<x.item.representative_ordinal<<",\"source_begin\":"<<x.source_begin<<",\"source_end\":"<<x.source_end<<"}\n";}
#endif
  require(p.core_cost.work_items<=core_limit&&p.local_work<=local_limit,"unchanged planning caps");require(c.count_begin_calls()==0,"NoLearning count requests");
 }
 Status query(Composition& c,const std::string& label,std::size_t prefix=0,std::uint64_t context=1){observe::counters={};start(label);
#ifdef C4_TRACE_ENABLED
  c4::phase="planning";
  if((scenario=="SYN11" && label=="synthetic_first") || (scenario=="CONT1" && label=="continuous_1") || (scenario=="SYN3" && (label=="synthetic_first" || label=="synthetic_hot"))) {
    const auto snap=c.snapshot();
    std::cout << "{\"kind\":\"c4_binding\",\"scenario\":" << std::quoted(scenario) << ",\"op\":" << std::quoted(label) << ",\"op_id\":" << seq << ",\"raw\":" << std::quoted(std::string(snap.raw)) << ",\"focus\":" << snap.focus << ",\"input_id\":" << snap.input_id << ",\"revision\":" << snap.revision << ",\"scheme_epoch\":" << snap.scheme_epoch << ",\"context\":" << context << "}\n";
  }
#endif
  auto s=prefix?c.choose_prefix(prefix):c.query_next({},context);const auto handle=c.operation();bool planning=!prefix;
  for(std::size_t i=0;s==Status::Working;++i){require(i<5000000,"query bound");if(planning&&c.planning_report().valid){stage("planning",s);trace={};baseline=metrics;loc=order->locate_stats();allocation_probe::reset_peak();clock=std::chrono::steady_clock::now();steps=work=local=max_core=max_local=max_reads=max_bytes=0;planning=false;
#ifdef C4_TRACE_ENABLED
 c4::phase="formal";
#endif
}
   auto t=c.tick(handle,budget);step(t);s=t.status;}
  stage(planning?"planning":"formal",s);
  report(c,s);
  // Metadata-only snapshot: collecting full keys does not fetch candidate text.
  if(s == Status::PageReady) {
   for(std::size_t slot = 0; slot < c.page().size; ++slot) fact(c,slot,"");
  }
  return s;
 }
 Status drain(Composition& c,Status s,const std::string& label,const char* phase){start(label);auto h=c.operation();for(std::size_t i=0;s==Status::Working;++i){require(i<5000000,"operation bound");auto t=c.tick(h,budget);step(t);s=t.status;}stage(phase,s);return s;}
 std::string text(Composition& c,std::size_t slot,const std::string& label){auto h=c.selection(slot);auto s=drain(c,c.read_text(h),label,"text");require(s==Status::Ready,"text Ready");return std::string(c.last_text());}
 void fact(Composition& c,std::size_t slot,const std::string& value){const auto p=c.page();const auto& item=p.items[slot];std::cout<<"{\"kind\":\"candidate\",\"scenario\":"<<std::quoted(scenario)<<",\"op_id\":"<<seq<<",\"begin\":"<<p.begin<<",\"end\":"<<p.end<<",\"slot\":"<<slot<<",\"key\":"<<std::quoted(format_slk1(item.key))<<",\"ref\":"<<item.ref<<",\"score\":"<<item.score<<",\"paths\":"<<item.path_count<<",\"ordinal\":"<<item.representative_ordinal<<",\"text\":"<<std::quoted(value)<<"}\n";}
 std::string select(Composition& c,const std::string& label,const std::string& wanted=""){
  Selection h;std::string t;bool found=false;
  for(std::size_t page=0;!found&&page<128;++page){auto p=c.page();for(std::size_t i=0;i<p.size;++i){t=text(c,i,label+"_text"+std::to_string(page)+"_"+std::to_string(i));fact(c,i,t);if(wanted.empty()||t==wanted){h=c.selection(i);found=true;break;}}
   if(!found){require(p.has_more,"required mapping absent");require(drain(c,c.request_page(RangeDirection::After),label+"_search_page","page")==Status::PageReady,"search page");}}
  require(found,"selection bound");require(drain(c,c.select(h),label,"select")==Status::Ready,"select success");
  const auto snap=c.snapshot();for(std::size_t i=0;i<snap.member_count;++i){auto m=snap.members[i];std::cout<<"{\"kind\":\"member\",\"scenario\":"<<std::quoted(scenario)<<",\"op_id\":"<<seq<<",\"index\":"<<i<<",\"begin\":"<<m.begin<<",\"end\":"<<m.end<<",\"key\":"<<std::quoted(format_slk1(m.key))<<",\"ref\":"<<m.ref<<",\"valid\":"<<m.valid<<",\"text\":"<<std::quoted(std::string(snap.selected_text.substr(m.text_begin,m.text_size)))<<"}\n";}return t;
 }
 void memory(Composition& c){auto p=c.planning_memory_usage();auto m=c.memory_usage();std::cout<<"{\"kind\":\"memory\",\"scenario\":"<<std::quoted(scenario)<<",\"op_id\":"<<seq<<",\"planner_owner\":"<<p.owner<<",\"planner_impl\":"<<p.impl<<",\"planner_buffers\":"<<p.buffers<<",\"planner_constants\":"<<p.constant_table<<",\"extension_owner\":"<<m.owner_bytes<<",\"extension_impl\":"<<m.impl_bytes<<",\"composition_buffers\":"<<m.composition_buffers<<",\"rank_arrays\":"<<m.rank_arrays<<",\"page_array\":"<<m.page_array<<",\"output_buffers\":"<<m.output_buffers<<",\"core_index\":"<<order->memory_bytes()<<",\"core_pool\":"<<order->locate_stats().pool_bytes<<",\"trace_bytes\":"<<sizeof(trace)<<",\"observation_bytes\":"<<sizeof(observe::counters)<<"}\n";}
};
struct Document:FinalDocument {bool reject{};std::size_t attempts{};bool commit(std::string_view s)override{++attempts;return reject?false:FinalDocument::commit(s);}};
void commit(R& r,Composition& c,const std::string& expected,bool refuse){Document d;Submission sub(c,r.shared,d,lc1::submission_config(lc1::input_config(r.scheme_id=="xiaohe"?SchemeRules::R1Xiaohe:SchemeRules::FullPinyin)),r.storage.view());require(d.text().empty(),"no premature body");auto prep=[&](SubmissionResult p){const auto h=p.transaction;while(p.core.status==CompositionStatus::Preparing){auto s=sub.advance(h,r.budget);r.work+=s.cost.work_items;++r.steps;p=s.result;}return p;};r.start("prepare");auto p=prep(sub.prepare());r.stage("prepare",Status::Ready);require(p.core.status==CompositionStatus::Ready,"prepare ready");
 d.reject=refuse;r.start("confirm");p=sub.confirm(p.transaction);r.stage("confirm",Status::Ready);
 if(refuse){require(p.core.status==CompositionStatus::RetryableNoEffect&&d.text().empty()&&!c.snapshot().raw.empty(),"refusal retains input");d.reject=false;r.start("retry_prepare");p=prep(sub.retry(p.transaction));r.stage("retry_prepare",Status::Ready);require(p.core.status==CompositionStatus::Ready,"retry ready");p=sub.confirm(p.transaction);}
 require(p.core.status==CompositionStatus::Accepted&&d.text()==expected&&d.calls()==1&&c.snapshot().raw.empty(),"one whole body");require(sub.confirm(p.transaction).core.status==CompositionStatus::Accepted&&d.calls()==1,"repeat confirm");std::cout<<"{\"kind\":\"body\",\"scenario\":"<<std::quoted(r.scenario)<<",\"text\":"<<std::quoted(expected)<<",\"attempts\":"<<d.attempts<<",\"accepts\":"<<d.calls()<<"}\n";
}
}
int main(int argc,char** argv){try{
 require(argc==5,"logical transport scheme locate_slots");const std::string id=argv[3];const bool x=id=="xiaohe";const auto rules=x?SchemeRules::R1Xiaohe:SchemeRules::FullPinyin;const std::size_t pool=std::stoul(argv[4]);
 const std::string today=x?"wojbtm":"wojintian",word=x?"vshxrfmbgshego":"zhonghuarenmingongheguo",three=x?"wjdjle":"wandanle",shortcode=x?"ui":"shi";
 for(unsigned scene=1;scene<=10;++scene){
#ifdef P2_DAILY_REFERENCE
 if(scene!=1&&scene!=2&&scene!=4&&scene!=6&&scene!=9&&scene!=10)continue;
#endif
 R r;r.scenario="L"+std::to_string(scene);r.mount(argv[1],argv[2],id,pool);Config cfg=lc1::input_config(rules);
#ifdef P2_DAILY_REFERENCE
 cfg.planning.core_work=r.core_limit=50000000;cfg.planning.local_work=r.local_limit=2000000;cfg.planning.read_bytes=128*1024*1024;
#endif
 if(scene==10) { cfg.word_bytes=8; }
 Composition c(r.scheme,r.shared,rules,cfg,r.storage.view());
 auto raw=[&](const std::string& s){require(c.edit_raw(0,c.snapshot().raw.size(),s)==Status::Ready,"set complete raw");};
 if(scene==1){for(const auto& s:{shortcode,std::string("le")}){raw(s);require(r.query(c,s+"_first")==Status::PageReady,"short query");require(c.planning_report().direct_range&&c.planning_report().queries==0,"sole legal interval");auto t=r.text(c,0,"short_text");r.fact(c,0,t);require(r.query(c,s+"_repeat")==Status::PageReady,"short repeat");}}
 if(scene==2){raw(today);require(r.query(c,"today_first")==Status::PageReady,"today first");require(r.select(c,"select_front")=="我","front wo");require(r.query(c,"today_suffix")==Status::PageReady,"today suffix");auto p=c.planning_report();require(p.cache_hits==3&&p.queries==0&&p.core_cost.read_calls==0,"three suffix facts");require(r.select(c,"select_suffix")=="今天","suffix today");commit(r,c,"我今天",true);}
 if(scene==3||scene==5){const auto& s=scene==3?today:word;for(std::size_t i=0;i<s.size();++i){require(c.edit_raw(i,i,std::string_view(s.data()+i,1))==Status::Ready,"one letter append");auto status=r.query(c,"append_"+std::to_string(i+1));require(status==Status::PageReady||status==Status::Unresolved||status==Status::NoPrefixMatch,"typed status");require(c.snapshot().raw==s.substr(0,i+1),"raw retained");}
  require(c.edit_raw(s.size()-1,s.size(),"")==Status::Ready,"single Backspace");r.query(c,"backspace");require(c.edit_raw(s.size()-1,s.size()-1,s.substr(s.size()-1))==Status::Ready,"reappend");r.query(c,"reappend");r.query(c,"same_input_again");}
 if(scene==4){raw(word);require(r.query(c,"long_first")==Status::PageReady,"long existing word");auto t=r.text(c,0,"long_top");r.fact(c,0,t);require(t=="中华人民共和国"&&c.page().end==word.size(),"long word range");r.memory(c);r.query(c,"long_repeat");r.select(c,"long_select","中华人民共和国");commit(r,c,"中华人民共和国",false);}
 if(scene==6){raw(word+word);auto status=r.query(c,"double_long");require(status==Status::PageReady||status==Status::Unresolved,"bounded double long");r.memory(c);require(r.query(c,"manual_first_word",word.size())==Status::PageReady,"manual independent prefix");r.select(c,"select_long_front","中华人民共和国");r.query(c,"double_long_suffix");require(r.query(c,"manual_suffix",word.size()*2)==Status::PageReady,"manual suffix");r.select(c,"select_long_suffix","中华人民共和国");commit(r,c,"中华人民共和国中华人民共和国",false);}
 if(scene==7){raw(shortcode);require(r.query(c,"navigation_first")==Status::PageReady,"navigation ready");std::size_t page=0;for(std::size_t target:{1U,2U,3U,4U,3U,2U,1U,0U,10U,11U,10U,0U,25U}){while(page!=target){const bool after=page<target;require(r.drain(c,c.request_page(after?RangeDirection::After:RangeDirection::Before),"nav_"+std::to_string(page)+"_to_"+std::to_string(after?page+1:page-1),"page")==Status::PageReady,"page movement");page=after?page+1:page-1;}
  auto p=c.page();for(std::size_t i=0;i<p.size;++i){auto t=r.text(c,i,"nav_text_"+std::to_string(page)+"_"+std::to_string(i));r.fact(c,i,t);}}}
 if(scene==8){raw(three);require(r.query(c,"whole_front")==Status::PageReady,"front");r.select(c,"whole_select","完蛋");require(r.query(c,"le_suffix")==Status::PageReady,"le suffix");r.select(c,"le_select","了");auto retained=c.snapshot().members[1].key;require(c.reopen_segment(0)==Status::Ready&&!c.snapshot().members[1].valid,"reopen retains invalid intention");require(r.query(c,"short_prefix",x?2:3)==Status::PageReady,"manual short");r.select(c,"play","玩");require(r.query(c,"second_prefix",x?4:6)==Status::PageReady,"second prefix");r.select(c,"egg","蛋");require(r.drain(c,c.revalidate(),"revalidate","revalidate")==Status::Ready&&c.snapshot().members[2].key==retained,"suffix identity rechecked");commit(r,c,"玩蛋了",false);}
 if(scene==9){raw(std::string(64,'a'));require(c.edit_raw(64,64,"a")==Status::CapacityExceeded&&c.snapshot().raw.size()==64,"capacity 65 rejected");auto s=r.query(c,"raw64");require(s==Status::Unresolved||s==Status::PageReady||s==Status::NoPrefixMatch,"bounded capacity status");r.memory(c);require(c.edit_raw(0,2,"wo")==Status::Ready,"recover raw prefix");require(r.query(c,"manual_wo",2)==Status::PageReady,"manual after limit");r.select(c,"manual_wo_select","我");require(c.snapshot().raw.size()==64,"recovery retains suffix");}
 if(scene==10){raw(word);require(r.query(c,"small_text_long")==Status::PageReady,"small buffer range still valid");const auto input=c.raw_input().stamp;auto status=r.drain(c,c.select(c.selection(0)),"too_long_select","select_failure");require(status!=Status::Ready&&c.input_is_current(input)&&c.snapshot().member_count==0,"failure preserves raw selection state");require(r.query(c,"manual_recover",x?2:5)==Status::PageReady,"recover shorter prefix");r.select(c,"short_text_select");}
 std::cout<<"{\"kind\":\"whole_cost\",\"scenario\":"<<std::quoted(r.scenario)<<",\"reads\":"<<r.metrics.read_calls<<",\"bytes\":"<<r.metrics.delivered_bytes<<"}\n";
 require(c.count_begin_calls()==0,"NoLearning");std::cout<<"PASS "<<r.scenario<<" scheme="<<id<<" pool="<<pool<<'\n';}
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
