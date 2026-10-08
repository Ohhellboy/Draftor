#include "model_support.hpp"
#include "input_harness.hpp"
#include "r1_regressions.hpp"
#include <vector>
#include <limits>

struct Run { std::string bytes; dx::TextFailure failure{}; dx::Flow flow=dx::Flow::NeedInput; std::size_t consumed=0; };
// Separate whole-buffer oracle for VALID input. It neither instantiates nor
// calls the streaming implementation and uses lookahead for CRLF.
static std::string oracle(std::string_view source, bool import) {
  std::size_t i = import && source.substr(0,3) == bom ? 3 : 0;
  std::string result;
  for (; i<source.size(); ++i) {
    if (source[i]=='\r') { result.push_back('\n'); if(i+1<source.size()&&source[i+1]=='\n')++i; }
    else result.push_back(source[i]);
  }
  return result;
}
template<class Stream>
static Run drive(Stream& stream, const std::string& input,
                 const std::vector<std::size_t>& chunks, std::size_t output_cap=7) {
  Run run;
  std::size_t at=0, round=0;
  for(auto length:chunks) {
    CHECK(length <= input.size()-at);
    std::size_t within=0;
    do {
      std::array<char,19> out; out.fill(static_cast<char>(0x6a));
      auto r=stream.feed(std::string_view(input).substr(at+within,length-within),{out.data()+1,output_cap});
      CHECK(r.consumed<=length-within && r.produced<=output_cap);
      CHECK(out[0]==0x6a && out[output_cap+1]==0x6a);
      run.bytes.append(out.data()+1,r.produced);within+=r.consumed;run.consumed+=r.consumed;
      if(r.flow==dx::Flow::Error){run.failure=r.failure;run.flow=r.flow;return run;}
      CHECK(r.flow!=dx::Flow::Done);
      if(r.flow==dx::Flow::NeedInput){CHECK(within==length);break;}
      CHECK(r.consumed || r.produced || !output_cap);
      CHECK(++round<input.size()*10+100);
    }while(true);
    at+=length;
  }
  CHECK(at==input.size());
  for(unsigned i=0;i<20;++i){
    std::array<char,19> out{};
    auto r=stream.finish({out.data(),output_cap});
    CHECK(!r.consumed && r.produced<=output_cap);
    run.bytes.append(out.data(),r.produced);run.flow=r.flow;run.failure=r.failure;
    if(r.flow==dx::Flow::Done || r.flow==dx::Flow::Error)return run;
  }
  CHECK(false);return run;
}
static Run run(const std::string& s,bool import,std::size_t b,std::vector<std::size_t> chunks={},std::size_t cap=7) {
  if(chunks.empty())chunks.push_back(s.size());
  if(import){dx::ImportWire t(b);return drive(t,s,chunks,cap);}
  dx::ExportBody t(b);return drive(t,s,chunks,cap);
}
static void expect(const std::string& source,const std::string& expected,bool import,std::size_t b=65536) {
  for(std::size_t cap:{1U,2U,7U}) {
    auto r=run(source,import,b,{},cap);CHECK(r.flow==dx::Flow::Done);CHECK(bytesEqual(r.bytes,expected));
  }
}
static void txt_boundaries() {
  for(std::size_t b:{0U,1U,7U,65536U}){
    dx::Limits limits;CHECK(dx::Limits::checked(b,limits));CHECK(limits.wire==2*b+3);
    for(std::size_t n:{std::size_t(0),b?b-1:0,b,b+1}){
      for(bool import:{false,true}){
        auto r=run(std::string(n,'x'),import,b);
        CHECK((r.flow==dx::Flow::Done)==(n<=b));
        if(n<=b)CHECK(bytesEqual(r.bytes,std::string(n,'x')));
        if(n>b){CHECK(r.failure.offset==b);CHECK(r.failure.code==(import?dx::TextError::BodyLimit:dx::TextError::InputLimit));}
      }
    }
    expect(std::string(b,'\n'),std::string(b,'\n'),true,b);
    expect(std::string(b,'\n'),std::string(b,'\n'),false,b);
    std::string crlf=bom;for(std::size_t i=0;i<b;++i)crlf+="\r\n";
    CHECK(crlf.size()==limits.wire);
    expect(crlf,std::string(b,'\n'),true,b);
    auto over=run(crlf+"x",true,b);CHECK(over.failure.code==dx::TextError::InputLimit);CHECK(over.failure.offset==limits.wire);
  }
  const std::size_t maximum=std::numeric_limits<std::size_t>::max();
  dx::Limits limits;CHECK(!dx::Limits::checked(maximum,limits));CHECK(limits.body==0&&limits.wire==0);
  for(bool import:{false,true}){
    auto r=run("",import,maximum);CHECK(r.failure.code==dx::TextError::InvalidConfig);
    const auto maximum_body=std::string(65532,'a')+u8"𨿍";
    r=run(maximum_body,import,65536);CHECK(r.flow==dx::Flow::Done&&bytesEqual(r.bytes,maximum_body));
  }
  // Limit counts complete output scalars: error at the source scalar's lead.
  auto r=run(bom+u8"𨿍",true,3);CHECK(r.failure.code==dx::TextError::BodyLimit&&r.failure.offset==3);
}
static void txt_splits() {
  const std::vector<std::string> inputs={"", "x\r\n y\r",bom+bom+"x",u8"a𨿍b",u8"中é", "\t \r\n\n"};
  std::size_t partitions=0;
  for(const auto& s:inputs)for(bool import:{false,true}){
    auto reference=oracle(s,import);
    const auto combinations=s.empty()?1U:1U<<(s.size()-1);
    for(unsigned mask=0;mask<combinations;++mask){
      std::vector<std::size_t> chunks;std::size_t length=0;
      for(std::size_t i=0;i<s.size();++i){++length;if(i+1==s.size()||(mask&(1U<<i))){chunks.push_back(length);chunks.push_back(0);length=0;}}
      if(chunks.empty())chunks.push_back(0);
      auto r=run(s,import,64,chunks,mask%3+1);CHECK(r.flow==dx::Flow::Done);CHECK(bytesEqual(r.bytes,reference));++partitions;
    }
  }
  std::printf("small_partitions=%zu\n",partitions);
  std::uint32_t random=0x20260925;
  std::string large;
  const std::array<std::string,7> tokens={"x"," \t",u8"𨿍",u8"中","\r\n","\r",u8"e\u0301"};
  while(large.size()<64000){random^=random<<13;random^=random>>17;random^=random<<5;large+=tokens[random%tokens.size()];}
  for(bool import:{false,true})for(unsigned seed=1;seed<=5;++seed){
    std::vector<std::size_t> chunks;std::size_t at=0;random=seed;
    while(at<large.size()){random=random*1664525U+1013904223U;auto n=std::min(std::size_t(random%513+1),large.size()-at);chunks.push_back(n);at+=n;}
    auto r=run(large,import,65536,chunks);CHECK(r.flow==dx::Flow::Done);CHECK(bytesEqual(r.bytes,oracle(large,import)));
  }
}
static void txt_invalid() {
  struct Invalid {std::string bytes;dx::TextError error;std::size_t offset;};
  std::vector<Invalid> cases={
    {std::string(1,'\0'),dx::TextError::Nul,0},
    {"a"+std::string("\xe0\x80",2),dx::TextError::InvalidUtf8,1},
    {"a"+std::string("\xed\xa0\x80",3),dx::TextError::InvalidUtf8,1},
    {std::string("\xf4\x90\x80\x80",4),dx::TextError::InvalidUtf8,0},
    {std::string("\xc0\xaf",2),dx::TextError::InvalidUtf8,0},
    {std::string("\x80",1),dx::TextError::InvalidUtf8,0},
    {std::string("\xf5\x80\x80\x80",4),dx::TextError::InvalidUtf8,0},
    {"z"+std::string("\xe4\xb8",2),dx::TextError::TruncatedUtf8,1},
    {std::string("\xf0\x90\x80",3),dx::TextError::TruncatedUtf8,0},
    {std::string("\xe2X",2),dx::TextError::InvalidUtf8,0},
    {std::string("\xff\xfe",2),dx::TextError::InvalidUtf8,0},
    {std::string("\xfe\xff",2),dx::TextError::InvalidUtf8,0},
    {std::string("\0\0\xfe\xff",4),dx::TextError::Nul,0},
    {std::string("\xff\xfe\0\0",4),dx::TextError::InvalidUtf8,0}
  };
  for(auto& c:cases)for(bool import:{false,true})for(std::size_t cut=0;cut<=c.bytes.size();++cut){
    auto r=run(c.bytes,import,64,{cut,c.bytes.size()-cut},1);
    CHECK(r.flow==dx::Flow::Error);CHECK(r.failure.code==c.error);CHECK(r.failure.offset==c.offset);
  }
  auto r=run(bom+"x"+std::string(1,'\0'),true,64,{1,2,1,1});
  CHECK(r.failure.code==dx::TextError::Nul&&r.failure.offset==4);
  for(std::size_t split=0;split<=5;++split) {
    auto q=run(bom+"ab",true,1,{split,5-split},1);
    CHECK(q.failure.code==dx::TextError::BodyLimit && q.failure.offset==4);
  }
}
static void txt_backpressure() {
  dx::ImportWire stream(8);
  auto r=stream.feed("abc",{nullptr,0});CHECK(r.flow==dx::Flow::NeedOutput&&r.consumed==1&&!r.produced);
  r=stream.feed("bc",{nullptr,0});CHECK(!r.consumed&&!r.produced&&r.flow==dx::Flow::NeedOutput);
  char out[3]{};
  r=stream.feed("bc",{out,1});CHECK(r.produced==1&&out[0]=='a'&&r.consumed==1&&r.flow==dx::Flow::NeedOutput);
  r=stream.feed("c",{out+1,1});CHECK(r.produced==1&&r.consumed==1);
  r=stream.finish({nullptr,0});CHECK(r.flow==dx::Flow::NeedOutput);
  r=stream.finish({out+2,1});CHECK(r.flow==dx::Flow::Done);
  CHECK(std::string(out,3)=="abc");r=stream.finish({nullptr,0});CHECK(r.flow==dx::Flow::Done&&!r.produced);
  r=stream.feed("",{nullptr,0});CHECK(r.flow==dx::Flow::Done);
  r=stream.feed("x",{out,1});CHECK(r.flow==dx::Flow::Error&&r.failure.code==dx::TextError::AfterFinish&&r.failure.offset==3);
  const auto sticky=r.failure;r=stream.finish({out,1});CHECK(r.failure.code==sticky.code&&r.failure.offset==sticky.offset);
  dx::ExportBody empty(0);r=empty.feed("",{nullptr,0});CHECK(r.flow==dx::Flow::NeedInput);
  r=empty.finish({nullptr,0});CHECK(r.flow==dx::Flow::Done);
  dx::ImportWire invalid(10);r=invalid.feed("x",{nullptr,1});CHECK(r.failure.code==dx::TextError::InvalidArgument);
  dx::ImportWire cap(0);r=cap.feed("x",{nullptr,0});CHECK(r.failure.code==dx::TextError::BodyLimit);
}
static void txt_semantics() {
  const std::string controls=std::string("\1\2\7\10\13\14\16\37\177",9)+u8"\u0080\u009f";
  const std::vector<std::string> texts={semantic,controls,u8"e\u0301 é 😀", " \t  \n\n \t","tail","tail\n","tail\r"};
  for(auto& t:texts){
    expect(t,oracle(t,false),false);expect(t,oracle(t,true),true);
    auto exported=run(t,false,65536);auto imported=run(exported.bytes,true,65536);
    CHECK(bytesEqual(imported.bytes,oracle(t,false)));
  }
  expect(bom+"x",bom+"x",false);expect(bom+"x","x",true);
  expect(bom+bom+"x",bom+"x",true);expect(bom+bom+"x",bom+bom+"x",false);
  auto e=run(bom+"x",false,64);auto i=run(e.bytes,true,64);CHECK(i.bytes=="x"&&i.bytes!=bom+"x");
  auto twice=run(bom+bom+"x",true,64);CHECK(twice.bytes==bom+"x"); // no second import decode
  CHECK(!bytesEqual("abc","abd"));CHECK(bytesEqual("abc","abc")); // same-length negative oracle
}
static void session_preparing() {
  for(unsigned kind=0;kind<6;++kind){
    ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{64});
    if(kind==0){o.state.has_current=false;}
    if(kind==1){o.state.composition=true;}
    if(kind==2){o.state.semantic_pending=true;}
    if(kind==3){o.state.file_operation=true;}
    if(kind==4){o.state.pending_body_edits=true;}
    if(kind==5){o.allow_freeze=false;}
    CHECK(s.begin()!=dx::SessionError::None);CHECK(!o.frozen&&!a.live&&!o.starts);CHECK(o.body=="abc");
  }
  {ModelOwner o("a\r\nb");TrackingAllocator a;dx::ExportSession s(o,a,{64,1});
   o.state.body_dirty=true;o.state.position_dirty=true;
   CHECK(s.begin()==dx::SessionError::None);CHECK(s.tick()==dx::Phase::Saving&&o.state.save_busy);
   CHECK(!a.live);ready(s);CHECK(!o.state.body_dirty&&!o.state.position_dirty);
   CHECK(o.starts==1&&o.services==5);dx::Lease l;CHECK(s.acquire(l)==dx::ReaderStatus::Ok);
   CHECK(leaseBytes(l)=="a\nb"&&l.source_bytes==4&&l.size==3);s.release(l.token);s.stop();CHECK(!a.live&&!o.frozen);}
  {ModelOwner o("old");o.state.body_dirty=true;o.beginAutoSave();o.edit("new");
   TrackingAllocator a;dx::ExportSession s(o,a,{64});CHECK(s.begin()==dx::SessionError::None);ready(s);
   CHECK(o.starts==2);dx::Lease l;s.acquire(l);CHECK(leaseBytes(l)=="new");s.release(l.token);s.stop();}
  for(auto result:{dx::SaveState::Failed,dx::SaveState::Partial}){
    ModelOwner o;o.state.body_dirty=o.state.position_dirty=true;o.next_save=result;
    TrackingAllocator a;dx::ExportSession s(o,a,{64});s.begin();
    for(int k=0;k<20&&s.phase()!=dx::Phase::Closed;++k)s.tick();
    CHECK(s.phase()==dx::Phase::Closed&&!a.live&&!o.frozen);
    CHECK(s.error()==(result==dx::SaveState::Partial?dx::SessionError::SavePartial:dx::SessionError::SaveFailed));
    CHECK(o.state.body_dirty==(result==dx::SaveState::Failed));
    CHECK(s.lastSave().body_committed==(result==dx::SaveState::Partial));CHECK(o.state.position_dirty);
  }
  {ModelOwner o;o.state.body_dirty=true;TrackingAllocator a;dx::ExportSession s(o,a,{64});
   s.begin();s.tick();s.cancel();CHECK(s.phase()==dx::Phase::Closed&&o.state.save_busy);
   // Persistence belongs to owner, continues safely after modal cancellation.
   while(o.state.save_busy){o.serviceSave();}
   CHECK(!o.state.body_dirty);}
  {ModelOwner o;TrackingAllocator a;a.fail_after=0;dx::ExportSession s(o,a,{64});s.begin();s.tick();
   CHECK(s.error()==dx::SessionError::AllocationFailed&&s.phase()==dx::Phase::Closed&&!o.frozen);}
  {ModelOwner o("");TrackingAllocator a;dx::ExportSession s(o,a,{0});s.begin();ready(s);
   dx::Lease l;CHECK(s.acquire(l)==dx::ReaderStatus::Ok&&l.size==0&&!l.data);s.release(l.token);s.stop();CHECK(!a.calls);}
}
static void snapshot_ownership() {
  leaseOutputRegressions();
  ModelOwner o(bom+"x\r\ny");TrackingAllocator a;dx::ExportSession s(o,a,{64,1});
  CHECK(s.begin()==dx::SessionError::None);ready(s);CHECK(a.peak_live==1);
  CHECK(s.begin()==dx::SessionError::Busy);
  dx::Lease first, second;CHECK(s.acquire(first)==dx::ReaderStatus::Ok);
  const auto expected=bom+"x\ny";CHECK(leaseBytes(first)==expected);
  CHECK(s.acquire(second)==dx::ReaderStatus::Busy&&!second.data);
  CHECK(s.release({first.token.session+1,first.token.generation})==dx::ReaderStatus::Stale);
  CHECK(a.live==1);CHECK(s.release(first.token,dx::ReaderEnd::SimulatedSendComplete)==dx::ReaderStatus::Ok);
  CHECK(s.release(first.token)==dx::ReaderStatus::Stale);
  CHECK(s.acquire(second)==dx::ReaderStatus::Ok&&second.data==first.data);
  s.cancel();s.cancel();CHECK(s.phase()==dx::Phase::Closing&&!s.canDestroy()&&a.live==1&&o.frozen);
  CHECK(leaseBytes(second)==expected);dx::Lease refused;CHECK(s.acquire(refused)==dx::ReaderStatus::NotReady);
  CHECK(s.release(first.token)==dx::ReaderStatus::Stale&&a.live==1);
  CHECK(s.release(second.token,dx::ReaderEnd::SendFailed)==dx::ReaderStatus::Ok);
  CHECK(s.phase()==dx::Phase::Closed&&s.canDestroy()&&!a.live&&s.cancelled());
  const auto old=second.token;CHECK(s.begin()==dx::SessionError::None);ready(s);
  dx::Lease current;s.acquire(current);CHECK(s.release(old)==dx::ReaderStatus::Stale&&s.readerActive());
  CHECK(leaseBytes(current)==expected);s.release(current.token);s.stop();
  for(unsigned failure=0;failure<4;++failure){
    ModelOwner bad(failure==3?std::string(1,'\0'):"text");TrackingAllocator pool;
    dx::ExportSession test(bad,pool,{64,1});test.begin();test.tick();
    if(failure==0){bad.read_fails=true;}
    if(failure==1){bad.bad_borrow=true;}
    if(failure==2){bad.mutate_in_borrow=true;}
    test.tick();CHECK(test.phase()==dx::Phase::Closed&&!pool.live&&!bad.frozen);
  }
  for(unsigned after=0;after<7;++after){
    ModelOwner value("abcde");TrackingAllocator pool;dx::ExportSession test(value,pool,{64,1});
    test.begin();for(unsigned n=0;n<after;++n)test.tick();
    test.cancel();test.cancel();CHECK(test.phase()==dx::Phase::Closed&&!pool.live&&!value.frozen);
    CHECK(pool.calls==pool.frees);
  }
  for(const auto& invalid:{std::string("x\xe2",2),std::string("x\xed\xa0\x80",4)}){
    ModelOwner value(invalid);TrackingAllocator pool;dx::ExportSession test(value,pool,{64,1});test.begin();
    for(unsigned n=0;n<10&&test.phase()!=dx::Phase::Closed;++n)test.tick();
    CHECK(test.error()==dx::SessionError::Text&&test.textFailure().offset==1&&!pool.live);
    value.edit("recovered");CHECK(test.begin()==dx::SessionError::None);ready(test);
    dx::Lease l;test.acquire(l);CHECK(leaseBytes(l)=="recovered");test.release(l.token);test.stop();
  }
}
static void session_identity() {
  for(unsigned change=0;change<7;++change){
    ModelOwner o("abcd");TrackingAllocator a;dx::ExportSession s(o,a,{64,1});s.begin();s.tick();s.tick();
    if(change==0){++o.state.identity.document[0];}
    if(change==1){++o.state.identity.collection[0];}
    if(change==2){++o.state.identity.mount;}
    if(change==3){++o.state.identity.document_session;}
    if(change==4){++o.state.identity.body_revision;}
    if(change==5){++o.state.identity.revision_epoch;}
    if(change==6)o.state.revision_reliable=false;
    s.tick();CHECK(s.phase()==dx::Phase::Closed&&!a.live);
  }
  {ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{64});s.begin();ready(s);dx::Lease l;s.acquire(l);
   ++o.state.identity.mount;s.tick();CHECK(s.phase()==dx::Phase::Closing&&a.live);
   dx::Lease x;CHECK(s.acquire(x)==dx::ReaderStatus::NotReady);s.release(l.token);CHECK(!a.live);}
  {ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{64,256,UINT64_MAX,0});
   CHECK(s.begin()==dx::SessionError::GenerationExhausted&&!o.frozen);}
  {ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{64,256,UINT64_MAX-1,UINT64_MAX-1});
   s.begin();ready(s);dx::Lease l;CHECK(s.acquire(l)==dx::ReaderStatus::Ok);s.release(l.token);
   CHECK(s.acquire(l)==dx::ReaderStatus::Exhausted&&s.phase()==dx::Phase::Closed);
   CHECK(s.begin()==dx::SessionError::GenerationExhausted);}
  {ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{64,0});CHECK(s.begin()==dx::SessionError::InvalidConfig);}
  {ModelOwner o;o.state.identity.body_revision=UINT32_MAX;TrackingAllocator a;
   dx::ExportSession s(o,a,{64});s.begin();o.state.identity.body_revision=0;s.tick();
   CHECK(s.error()==dx::SessionError::IdentityChanged&&s.phase()==dx::Phase::Closed);}
  {ModelOwner o;TrackingAllocator a;dx::ExportSession s(o,a,{2});
   CHECK(s.begin()==dx::SessionError::SourceTooLarge&&!o.frozen);}
}
static void input_model() {
  modifierOwnershipRegressions();
  ModelOwner o(std::string(1000,'x'));o.state.body_dirty=true;
  TrackingAllocator a;dx::ExportSession s(o,a,{2048,16});InputHarness h;
  h.push({4,0,1,1,10,true});CHECK(h.enter(s)==dx::SessionError::None&&h.applied==1);
  CHECK(h.held[4].suppressed);h.event({4,0,1,1,10,false});CHECK(!h.held[4].press);
  h.event({224,1,1,h.route,50,true});CHECK(h.modifiers==1); // explicit Control Down
  for(unsigned tick=0;tick<100 && s.phase()!=dx::Phase::Ready;++tick){
    for(unsigned j=0;j<8;++j)h.push({unsigned(5+j),1,1,h.route,100+tick*8+j,true});
    h.iteration(s);
  }
  CHECK(s.phase()==dx::Phase::Ready&&o.services==5&&s.stats().copy_ticks<=63);
  CHECK(h.applied==1&&h.discarded>100);CHECK(h.layout==1&&h.cursor==7&&h.selection==2&&h.history==3&&h.viewport==9);
  dx::Lease l;s.acquire(l);
  h.push({41,0,1,h.route,900,true,false,true});h.iteration(s);
  CHECK(h.cancels==1&&s.phase()==dx::Phase::Closing&&a.live==1);
  s.release(l.token);h.exit(s);const auto new_route=h.route;
  h.event({224,0,1,new_route-1,50,false});CHECK(h.modifiers==0); // old matching modifier Up
  h.event({5,0,1,new_route,999,true,true});CHECK(h.applied==1);
  h.presented(new_route-1);CHECK(h.awaiting_frame);h.presented(new_route);
  const auto press=h.held[5].press;h.event({5,0,1,new_route-1,press,false});CHECK(!h.held[5].press);
  h.event({225,2,1,new_route,1000,true});CHECK(h.modifiers==2); // explicit Shift Down
  h.event({5,2,1,new_route,1001,true});CHECK(h.applied==2&&h.modifiers==2);
  h.event({5,0,1,new_route-1,press,false});CHECK(h.held[5].press==1001&&h.modifiers==2); // late old Up
  h.event({6,0,0,new_route,1002,true});CHECK(h.applied==2); // old physical epoch
  h.event({7,0,1,new_route-1,1003,true,true});CHECK(h.applied==2); // old repeat
  h.event({5,0,1,new_route,1001,false});CHECK(h.modifiers==2&&!h.held[5].press);
  h.event({225,0,1,new_route,1000,false});CHECK(!h.modifiers&&!h.held[225].press);
  // Repeated entries exercise model route/frame/held ownership independently
  // from the 120 session allocation cycles below.
  for(unsigned cycle=0;cycle<20;++cycle){
    CHECK(h.enter(s)==dx::SessionError::None);const auto count=h.applied;
    // Movement, selection, Find/file command and F4 are equally discarded.
    for(unsigned usage:{40U,58U,61U,79U,80U})h.event({usage,3,1,h.route,2000+cycle*100+usage,true,false,false,true});
    CHECK(h.applied==count);
    for(unsigned n=0;n<64;++n)h.push({4,0,1,h.route,5000+cycle*100+n,true});
    h.requestCancel();h.iteration(s);CHECK(s.phase()==dx::Phase::Closed&&h.size==56);
    h.exit(s);h.presented(h.route);
    while(h.size){h.iteration(s);}
    CHECK(h.applied==count); // queued old events never replay
  }
}
static void lifecycle_100() {
  ModelOwner o(semantic);TrackingAllocator a;dx::ExportSession s(o,a,{65536,3});
  dx::ReaderToken old{};
  for(unsigned cycle=0;cycle<120;++cycle){
    CHECK(s.begin()==dx::SessionError::None);
    if(cycle%4==0){s.tick();s.cancel();}
    else{
      ready(s);dx::Lease l;CHECK(s.acquire(l)==dx::ReaderStatus::Ok);CHECK(leaseBytes(l)==semantic);
      CHECK(s.release(old)==dx::ReaderStatus::Stale);old=l.token;
      if(cycle%4==1)s.cancel();
      CHECK(s.release(l.token,cycle%4==2?dx::ReaderEnd::SendFailed:dx::ReaderEnd::Released)==dx::ReaderStatus::Ok);
      s.stop();
    }
    CHECK(s.phase()==dx::Phase::Closed&&!a.live&&!a.bytes&&!o.frozen);
  }
  CHECK(a.calls==a.frees&&a.peak_live==1);
  std::printf("host_cycles=120 live_allocations=%zu live_bytes=%zu calls=%zu frees=%zu\n",a.live,a.bytes,a.calls,a.frees);
}
static void module_budget() {
  ModelOwner o(std::string(65536,'x'));TrackingAllocator a;dx::ExportSession s(o,a,{65536,256});
  s.begin();ready(s);dx::Lease l;s.acquire(l);CHECK(l.size==65536);s.cancel();CHECK(a.live==1);
  s.release(l.token);CHECK(a.live==0);
  const auto m=s.stats();
  CHECK(m.allocation_bytes==65536&&a.largest==65536&&m.max_borrow<=256&&a.peak_live==1);
  CHECK(m.borrowed_bytes==65536&&m.output_bytes==65536&&m.borrow_calls==256);
  std::printf("RESOURCE {\"ExportSession\":%zu,\"ImportWire\":%zu,\"ExportBody\":%zu,\"Identity\":%zu,\"Lease\":%zu,"
      "\"largest_allocation\":%zu,\"peak_live\":%zu,\"peak_bytes\":%zu,\"source_passes\":1,\"borrow_calls\":%zu,"
      "\"max_borrow\":%zu,\"new_network_pool\":0,\"live_after_close\":%zu}\n",
      sizeof(s),sizeof(dx::ImportWire),sizeof(dx::ExportBody),sizeof(dx::Identity),sizeof(dx::Lease),
      a.largest,a.peak_live,a.peak_bytes,m.borrow_calls,m.max_borrow,a.live);
}
int main(int argc,char** argv) {
  struct Test {const char* name;void(*run)();};
  const Test tests[]={{"txt_boundaries",txt_boundaries},{"txt_splits",txt_splits},{"txt_invalid",txt_invalid},
    {"txt_backpressure",txt_backpressure},{"txt_semantics",txt_semantics},{"session_preparing",session_preparing},
    {"snapshot_ownership",snapshot_ownership},{"session_identity",session_identity},{"input_model",input_model},
    {"lifecycle_100",lifecycle_100},{"module_budget",module_budget}};
  if(argc!=2)return 2;
  for(auto t:tests)if(std::string_view(argv[1])==t.name){t.run();std::printf("PASS %s checks=%zu\n",t.name,checks);return 0;}
  return 2;
}
