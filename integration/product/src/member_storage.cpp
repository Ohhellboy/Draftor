#include "member_storage.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#endif
namespace ime_device {
namespace {
void freeArray(void* p) noexcept {
#ifdef ARDUINO_ARCH_ESP32
 heap_caps_free(p);
#else
 std::free(p);
#endif
}
}
void* MemberStorage::allocate(void* context,hanime_extension::StorageRole role,size_t bytes,size_t alignment){
 auto& self=*static_cast<MemberStorage*>(context);
 const auto slot=size_t(role);
 if(slot>=self.records_.size()){++self.violations_;return nullptr;}
 auto& r=self.records_[slot];
 if(r.live||!bytes||!alignment||(alignment&(alignment-1))){++r.failed;++self.violations_;return nullptr;}
 r.bytes=bytes;r.alignment=alignment;
 const auto actual_alignment=std::max(alignment,sizeof(void*));
 void* p=nullptr;
#ifdef ARDUINO_ARCH_ESP32
 // Both capabilities are required. Never retry with INTERNAL or default heap.
 p=heap_caps_aligned_alloc(actual_alignment,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(p&&!esp_ptr_external_ram(p)){
  freeArray(p);p=nullptr;r.region=MemberRegion::InternalRejected;++self.violations_;
 }else r.region=p?MemberRegion::Psram:MemberRegion::None;
#else
 // Host verifies ownership/lifetimes, not ESP physical placement.
 if(posix_memalign(&p,actual_alignment,bytes)!=0)p=nullptr;
 r.region=p?MemberRegion::HostModel:MemberRegion::None;
#endif
 if(p&&uintptr_t(p)%alignment){freeArray(p);p=nullptr;++self.violations_;}
 if(!p){++r.failed;return nullptr;}
 self.addresses_[slot]=p;r.live=true;++r.allocated;r.allocated_bytes+=bytes;
 return p;
}
void MemberStorage::release(void* context,hanime_extension::StorageRole role,void* pointer,size_t bytes,size_t alignment) noexcept {
 auto& self=*static_cast<MemberStorage*>(context);const auto slot=size_t(role);
 if(slot>=self.records_.size()){++self.violations_;return;}
 auto& r=self.records_[slot];
 if(!r.live||self.addresses_[slot]!=pointer||r.bytes!=bytes||r.alignment!=alignment){
  ++self.violations_;return; // Never free an unowned/mismatched pointer.
 }
 freeArray(pointer);self.addresses_[slot]=nullptr;r.live=false;++r.released;r.released_bytes+=bytes;
}
const char* memberRoleName(size_t slot){
 static const char* const names[]={"members_a","members_b","plan","capture"};
 return slot<4?names[slot]:"unknown";
}
const char* memberRegionName(MemberRegion region){
 switch(region){case MemberRegion::Psram:return "PSRAM";case MemberRegion::HostModel:return "HOST_MODEL";
 case MemberRegion::InternalRejected:return "INTERNAL_REJECTED";default:return "NONE";}
}
}
