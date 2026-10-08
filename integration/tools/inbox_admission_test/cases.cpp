#include "actual_admission.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
using usb::Status;
constexpr std::size_t K=1024,requested=512*K,reserve=256*K;
void reset(std::size_t free=requested+reserve,std::size_t block=requested){CHECK(live==0);available=free;largest=block;allocations=releases=queries=extra_consumption=0;deny=false;}
void reject(std::size_t free,std::size_t block){reset(free,block);Admission a(requested);CHECK(a.disk_.bytes()==0&&a.disk_.sectorCount()==0);CHECK(a.allocateInbox()==Status::Full);CHECK(allocations==0&&live==0);CHECK(a.disk_.sync()==Status::NotReady);}
void admit(std::size_t free){reset(free);Admission a(requested);CHECK(a.allocateInbox()==Status::Ok);CHECK(allocations==1&&live==requested&&available>=reserve);CHECK(a.disk_.bytes()==requested&&a.disk_.sectorCount()==1024);char z=1;CHECK(a.disk_.read(0,&z,1).status==Status::Ok&&z==0);}
int main(){
 std::vector<std::pair<const char*,std::function<void()>>> cases={
 {"600KiB_free_must_not_call_allocator",[]{reject(600*K,requested);}},
 {"fragmented_total_sufficient",[]{reject(2*requested,requested-1);}},
 {"reserve_boundary_minus_one",[]{reject(requested+reserve-1,requested);}},
 {"reserve_boundary_exact",[]{admit(requested+reserve);}},
 {"reserve_boundary_plus_one",[]{admit(requested+reserve+1);}},
 {"below_requested",[]{reject(requested-1,requested);}},
 {"zero_free",[]{reject(0,0);}},
 {"actual_allocator_refusal",[]{reset();deny=true;Admission a(requested);CHECK(a.allocateInbox()==Status::Io);CHECK(allocations==1&&live==0&&a.disk_.bytes()==0&&a.disk_.sectorCount()==0);CHECK(a.disk_.read(0,nullptr,1).status==Status::NotReady);}},
 {"post_allocation_reserve_lost",[]{reset();extra_consumption=1;Admission a(requested);CHECK(a.allocateInbox()==Status::Full);CHECK(allocations==1&&releases==1&&live==0&&a.disk_.bytes()==0);}},
 {"post_allocation_reserve_exact",[]{admit(requested+reserve);}},
 {"idempotent_owned_disk_low_current_free",[]{reset();Admission a(requested);CHECK(a.allocateInbox()==Status::Ok);available=1;largest=1;const auto q=queries;CHECK(a.allocateInbox()==Status::Ok);CHECK(allocations==1&&releases==0&&live==requested&&queries==q);}},
 {"release_then_retry",[]{reset();Admission a(requested);CHECK(a.allocateInbox()==Status::Ok);a.disk_.release();CHECK(live==0&&a.disk_.bytes()==0&&a.disk_.sectorCount()==0);CHECK(a.allocateInbox()==Status::Ok);CHECK(allocations==2);}},
 {"refusal_then_retry",[]{reset();deny=true;Admission a(requested);CHECK(a.allocateInbox()==Status::Io);deny=false;CHECK(a.allocateInbox()==Status::Ok&&allocations==2);}},
 {"postcheck_failure_then_retry",[]{reset();extra_consumption=1;Admission a(requested);CHECK(a.allocateInbox()==Status::Full);extra_consumption=0;available=requested+reserve;CHECK(a.allocateInbox()==Status::Ok&&allocations==2);}},
 {"zero_capacity",[]{reset();Admission a(0);CHECK(a.allocateInbox()==Status::InvalidArgument&&allocations==0&&a.disk_.sectorCount()==0);}},
 {"unaligned_capacity",[]{reset();Admission a(requested+1);CHECK(a.allocateInbox()==Status::InvalidArgument&&allocations==0);}},
 {"sector_count_conversion_overflow",[]{reset(std::numeric_limits<std::size_t>::max(),std::numeric_limits<std::size_t>::max());const auto n=(std::size_t(UINT32_MAX)+1)*512;Admission a(n);CHECK(a.allocateInbox()==Status::InvalidArgument&&allocations==0);}},
 {"size_max_near_addition_overflow",[]{reset(std::numeric_limits<std::size_t>::max(),std::numeric_limits<std::size_t>::max());Admission a(std::numeric_limits<std::size_t>::max()&~std::size_t(511));CHECK(a.allocateInbox()==Status::InvalidArgument&&allocations==0);}}
 };
 unsigned failed=0;for(const auto& c:cases){try{c.second();CHECK(live==0);std::cout<<"PASS "<<c.first<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<c.first<<": "<<e.what()<<'\n';}}
 return failed?86:0;
}
