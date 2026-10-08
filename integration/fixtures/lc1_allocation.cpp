#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
extern bool fail_next_new;
int main(){try{
 LongRig r("init_allocation");r.key(0x39);r.letters("wo");
 unsigned n=0;while(!r.resources.ready()){r.ime.service();r.drain();CHECK(++n<2000000);}
 CHECK(!r.ime.composition());fail_next_new=true;r.ime.service();CHECK(!fail_next_new);r.drain();
 CHECK(!r.ime.composition()&&r.ime.raw()=="wo"&&!r.ime.hasWork()&&r.app.document.text().empty());
 for(unsigned i=0;i<8;++i){
  r.ime.service();
 }
 CHECK(!r.ime.composition()&&r.ime.raw()=="wo");
 r.key(44);r.ready();CHECK(r.ime.raw().empty()&&std::string_view(r.app.document.text())=="我"&&r.accepted==1);
 std::cout<<"PASS init_allocation raw_retained no_auto_retry explicit_choice_retry accepted_once\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
