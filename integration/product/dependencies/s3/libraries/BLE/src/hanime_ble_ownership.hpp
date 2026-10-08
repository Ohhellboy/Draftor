#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
namespace hanime_ble {
struct Counts {std::atomic<uint32_t> clients{0},services{0},characteristics{0},descriptors{0};};
inline Counts live;
// The instance index owns every service, including duplicate UUIDs. UUID is a
// lookup alias and may omit duplicates; deleting through it leaks those nodes.
template<class UuidMap,class InstanceMap> void releaseServices(UuidMap& aliases,InstanceMap& owners){
 aliases.clear();
 for(auto& row:owners)delete row.first;
 owners.clear();
}

// Handle is identity; UUID is only a first-match lookup. All throwing work is
// transactional: even a failed alias allocation leaves no unowned descriptor.
template<class Aliases,class Owners,class Factory,class Uuid>
auto retainDescriptor(Aliases& aliases,Owners& owners,uint16_t handle,Factory make,Uuid uuid)
 -> typename Owners::mapped_type {
 auto found=owners.find(handle);
 if(found!=owners.end())return found->second;
 using Object=typename std::remove_pointer<typename Owners::mapped_type>::type;
 std::unique_ptr<Object> pending(make());
 auto key=uuid(pending.get());
 auto inserted=owners.emplace(handle,pending.get());
 try { aliases.emplace(key,pending.get()); }
 catch (...) { owners.erase(inserted.first);throw; }
 return pending.release();
}
template<class Aliases,class Owners> void releaseDescriptors(Aliases& aliases,Owners& owners){
 aliases.clear();
 for(auto& row:owners)delete row.second;
 owners.clear();
}
}
