#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
struct CountingSink:hanime::TextCommitSink {
 EditorSink actual;unsigned calls=0;explicit CountingSink(s3app::App& app):actual(app){CHECK(actual.bind(1,1));}
 bool commit(std::string_view s)override{++calls;return actual.commit(s);}
};
int main(){try{
 for(size_t count:{24u,25u}){
  LongRig r("text512_"+std::to_string(count));r.start(false,"vshxrfmbgshego");
  const auto item=r.ime.view()->rows[0].items[0];CHECK(item.bytes==21);
  // A formal Core request boundary, not a claim that this repeated form fits
  // raw64. Every member uses a real verified key/ref/output identity.
  std::vector<hanime::CompositionMember> members;
  for(size_t i=0;i<count;++i)members.push_back({item.ref,item.key,{i*14,(i+1)*14},{}});
  CountingSink sink(r.app);const auto cfg=submissionConfig(compositionConfig(hanime_extension::SchemeRules::R1Xiaohe));CHECK(cfg.total_text_bytes==512);
  hanime::CompositionCommitter core(r.resources.shared,sink,cfg);
  auto result=core.prepare(r.resources.shared.activation_handle(),{1,1,{0,count*14}},members.data(),members.size());
  unsigned n=0;while(result.status==hanime::CompositionStatus::Preparing){CHECK(sink.calls==0);result=core.advance(result.transaction,Resources::Budget).result;CHECK(++n<2000000);}
  CHECK(sink.calls==0&&r.app.document.text().empty());
  if(count==24){CHECK(result.status==hanime::CompositionStatus::Ready);CHECK(core.confirm(result.transaction).status==hanime::CompositionStatus::Accepted);CHECK(sink.calls==1&&r.app.document.text().size()==504);}
  else{CHECK(result.status==hanime::CompositionStatus::CapacityExceeded&&result.capacity==hanime::CompositionCapacity::TotalText&&result.required_capacity==525&&result.member_index==24);CHECK(core.confirm(result.transaction).status==hanime::CompositionStatus::CapacityExceeded&&sink.calls==0&&r.app.document.text().empty());}
  std::cout<<"PASS formal_text512 members="<<count<<" utf8="<<count*21<<" calls="<<sink.calls<<" physical_raw64=0\n";
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
