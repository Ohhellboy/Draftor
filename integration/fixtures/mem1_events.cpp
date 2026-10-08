#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
struct FaultBackend {
 MemberStorage owner;size_t calls=0,fail=0;
 static void* allocate(void* c,hanime_extension::StorageRole r,size_t b,size_t a){
  auto& f=*static_cast<FaultBackend*>(c);if(++f.calls==f.fail)return nullptr;
  auto backend=f.owner.backend();return backend.allocate(backend.context,r,b,a);
 }
 static void release(void* c,hanime_extension::StorageRole r,void* p,size_t b,size_t a)noexcept{
  auto backend=static_cast<FaultBackend*>(c)->owner.backend();backend.deallocate(backend.context,r,p,b,a);
 }
 hanime_extension::StorageBackend backend(){return {this,allocate,release,0x54455354};}
 size_t live()const{size_t n=0;for(const auto& r:owner.records())n+=r.live;return n;}
 void released()const{CHECK(live()==0&&owner.violations()==0);for(const auto& r:owner.records())CHECK(r.allocated==r.released);}
};
void fault(size_t at,bool escape){
 FaultBackend f;f.fail=at;
 {LongRig r("mem1_fail_"+std::to_string(at)+(escape?"_esc":"_retry"),f.backend());r.key(0x39);r.letters("wo");
 unsigned n=0;while(r.ime.hasWork()){r.ime.service();r.drain();CHECK(++n<2000000);}
 CHECK(!r.ime.composition()&&!r.ime.window()&&r.ime.raw()=="wo"&&r.accepted==0);f.released();
 CHECK(f.calls==at);for(unsigned i=0;i<10;++i)r.ime.service();CHECK(f.calls==at);
 if(escape){r.key(0x29);CHECK(r.ime.raw().empty()&&r.accepted==0);}
 else{f.fail=0;r.key(44);r.ready();CHECK(r.accepted==1&&std::string_view(r.app.document.text())=="我"&&f.live()==4);}
 }f.released();std::cout<<"PASS MEM1 fail="<<at<<" escape="<<escape<<" no_auto_retry atomic_publish paired_free\n";
}
void lifecycle(){
 FaultBackend f;
 {LongRig r("mem1_lifecycle",f.backend());r.start(false,"wo");CHECK(f.live()==4);
 for(size_t i=0;i<4;++i)CHECK(f.owner.records()[i].allocated==1);
 r.key(44);r.ready();CHECK(r.accepted==1&&f.live()==4);
 r.key(44,1);r.ready();CHECK(f.live()==4&&r.resources.rules()==hanime_extension::SchemeRules::FullPinyin);
 for(size_t i=0;i<4;++i){const auto& x=f.owner.records()[i];CHECK(x.allocated==2&&x.released==1&&x.live);}
 r.letters("fangeng");r.ready();r.key(0x29);CHECK(r.ime.raw().empty()&&std::string_view(r.app.document.text())=="我");
 }f.released();std::cout<<"PASS MEM1 lifecycle accepted scheme_destroy_before_create cancel close\n";
}
void acceptedThenFailure(){
 FaultBackend f;
 {LongRig r("mem1_accepted_then_failure",f.backend());r.start(false,"wo");r.key(44);r.ready();CHECK(r.accepted==1);
  f.fail=f.calls+1;r.key(44,1);unsigned n=0;while(r.ime.hasWork()){r.ime.service();r.drain();CHECK(++n<2000000);}
  CHECK(!r.ime.composition()&&std::string_view(r.app.document.text())=="我"&&r.ime.receipt().status==hanime::CompositionStatus::Accepted);f.released();
  r.letters("a");CHECK(r.ime.raw()=="a");const auto calls=f.calls;for(unsigned i=0;i<8;++i)r.ime.service();CHECK(f.calls==calls);
  f.fail=0;r.key(44);r.ready();CHECK(r.accepted==2&&std::string_view(r.app.document.text()).substr(0,3)=="我");
 }f.released();std::cout<<"PASS MEM1 prior_Accepted retained across failed_scheme_construction explicit_new_input_retry\n";
}
int main(){try{for(size_t at=1;at<=4;++at){fault(at,false);fault(at,true);}lifecycle();acceptedThenFailure();return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
