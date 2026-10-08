#pragma once
#include "hanime_extension/storage.hpp"
#include <array>

namespace ime_device {
enum class MemberRegion : uint8_t { None, HostModel, Psram, InternalRejected };
struct MemberStorageRecord {
 size_t bytes=0, alignment=0;
 uint32_t allocated=0, released=0, failed=0;
 uint64_t allocated_bytes=0,released_bytes=0;
 MemberRegion region=MemberRegion::None;
 bool live=false;
};
// One controller owns one set of four arrays. No global allocator policy and
// no ownership swap: INPUT's A/B roles follow their original allocations.
class MemberStorage {
 std::array<MemberStorageRecord,4> records_{};
 std::array<void*,4> addresses_{};
 uint32_t violations_=0;
 static void* allocate(void*,hanime_extension::StorageRole,size_t,size_t);
 static void release(void*,hanime_extension::StorageRole,void*,size_t,size_t) noexcept;
 public:
 static constexpr uint32_t BackendId=0x4d454d31; // MEM1, diagnostic only.
 MemberStorage()=default;
 MemberStorage(const MemberStorage&)=delete;
 MemberStorage& operator=(const MemberStorage&)=delete;
 hanime_extension::StorageBackend backend(){return {this,allocate,release,BackendId};}
 const std::array<MemberStorageRecord,4>& records()const{return records_;}
 uint32_t violations()const{return violations_;}
};
const char* memberRoleName(size_t);
const char* memberRegionName(MemberRegion);
}
