#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
void retired(){
 LongRig r("owner");r.start(false,"ui");r.key(44);r.ready();CHECK(r.accepted==1);const std::string accepted(r.app.document.text().data(),r.app.document.text().size());
 r.letters("ui");r.ready();const auto old_owner=r.ime.resourceOwner();
 r.enqueue(4,true);r.enqueue(4,false);CHECK(r.fifo.size()==2);
 r.resources.mount()->available=false;r.ime.service();r.drain();
 CHECK(!r.ime.chinese()&&r.ime.resourceOwner()!=old_owner);r.dispatch();
 CHECK(r.ime.raw().empty()&&std::string_view(r.app.document.text())==accepted);
 size_t retired=0;for(const auto& rec:r.records)if(rec.kind==UiTraceKind::Intent&&rec.detail==ResourceOwnerRetired)++retired;
 CHECK(retired==1);r.key(4);CHECK(std::string_view(r.app.document.text())==accepted+"a");
 std::cout<<"PASS E1 old_CN_terminal_once new_EN_normal accepted_body_preserved\n";
}
void overlap(){
 LongRig r("overlap");r.start(false,"wjdjle");r.key(44);CHECK(r.ime.hasWork());r.key(44);CHECK(r.ime.queuedInput()>0);r.ready();
 CHECK(r.accepted==1&&r.ime.raw().empty()&&std::string_view(r.app.document.text())=="完蛋了");
 std::cout<<"PASS overlap real_second_choice_before_first_done accepted_once\n";
}
int main(){try{retired();overlap();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
