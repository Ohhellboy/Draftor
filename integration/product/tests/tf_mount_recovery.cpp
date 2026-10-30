#include "tf_mount_recovery.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
struct Port {
 uint32_t time=0,call_ms=4000;unsigned succeeds_at=0,calls=0,parks=0,records=0;bool owned=false,parked=false;
 std::array<uint32_t,3> budgets{};
 uint32_t now()const{return time;} bool unowned()const{return !owned;}
 bool park(){++parks;parked=true;return true;} void pause(uint32_t n){time+=n;}
 s3tf::MountAttempt mount(uint32_t budget){CHECK(parked);parked=false;budgets.at(calls)=budget;++calls;time+=call_ms;return {call_ms,calls==succeeds_at,call_ms>=budget,"fault",3};}
 void record(unsigned n,const s3tf::MountAttempt&){CHECK(n==++records);}
};
int main(){try{
 Port failed;auto r=s3tf::recoverMount(failed);CHECK(!r.mounted&&r.attempts==3&&r.elapsed_ms==12500&&failed.parks==4&&failed.parked);
 Port second;second.succeeds_at=2;second.call_ms=100;r=s3tf::recoverMount(second);CHECK(r.mounted&&r.attempts==2&&r.elapsed_ms==450&&second.parks==2&&!second.parked);
 Port first;first.succeeds_at=1;first.call_ms=30;r=s3tf::recoverMount(first);CHECK(r.mounted&&r.attempts==1&&first.parks==1);
 Port busy;busy.owned=true;r=s3tf::recoverMount(busy);CHECK(!r.mounted&&!r.attempts&&!busy.parks);
 Port wrap;wrap.time=UINT32_MAX-100;r=s3tf::recoverMount(wrap);CHECK(r.elapsed_ms==12500&&r.attempts==3);
 Port blocking;blocking.call_ms=16000;r=s3tf::recoverMount(blocking);CHECK(!r.mounted&&r.attempts==1&&r.max_call_ms==16000&&r.elapsed_ms==16000&&blocking.parked);
 std::cout<<"PASS finite retry, cleanup, first/second success, owned rejection, wrap, non-preemptible overrun measurement; NO DEVICE CLAIM\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
