#include "member_storage.hpp"
#include "esp_heap_caps.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
static bool fail=false,external=true;static unsigned calls=0,frees=0;
void* heap_caps_aligned_alloc(size_t a,size_t n,unsigned caps){++calls;CHECK(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(fail)return nullptr;void* p=nullptr;return posix_memalign(&p,a,n)==0?p:nullptr;}
void heap_caps_free(void* p){++frees;std::free(p);}
bool esp_ptr_external_ram(const void*){return external;}
int main(){try{using namespace ime_device;using R=hanime_extension::StorageRole;
 MemberStorage owner;auto b=owner.backend();auto p=b.allocate(b.context,R::CompositionMembersA,4096,4);CHECK(p&&owner.records()[0].region==MemberRegion::Psram);b.deallocate(b.context,R::CompositionMembersA,p,4096,4);CHECK(frees==1);
 fail=true;CHECK(!b.allocate(b.context,R::CompositionMembersA,4096,4));CHECK(calls==2&&frees==1);fail=false;
 external=false;CHECK(!b.allocate(b.context,R::CompositionMembersA,4096,4));CHECK(calls==3&&frees==2&&owner.violations()==1);external=true;
 CHECK(!b.allocate(b.context,R::CompositionMembersA,4096,3));CHECK(calls==3);
 std::cout<<"PASS MEM1 target_call_model exact_caps alignment paired_free failure_no_fallback non_psram_rejected cases=4 actual_hardware=0\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
