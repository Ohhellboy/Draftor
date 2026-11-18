#pragma once
#include "hanime/core.hpp"
#include <memory>
namespace hanime {
struct CountBatchAllocator {
 void* (*allocate)(std::size_t){nullptr};
 void (*release)(void*){nullptr};
};
struct CountBatchProgress {
 const char* phase="inactive";const char* stop="idle";
 std::uint64_t cursor=0;std::size_t members=0;
 std::uint64_t work=0,deadline_yields=0,zero_progress_returns=0;
};
// Query scratch only: never a second Count authority or a persistent cache.
class StateBatch {
 public:
 static constexpr std::size_t Limit=512;
 StateBatch(const void* first_key,std::size_t stride,std::size_t count,CountBatchAllocator={});
 ~StateBatch();
 StateBatch(const StateBatch&)=delete;
 std::size_t size() const;
 std::size_t memory_bytes() const;
 CountBatchProgress progress() const;
 private:
 struct Impl; std::unique_ptr<Impl> p_;
 friend class StateV3;
 friend class BoundedUserState;
 const StableLexemeKey& key(std::size_t) const;
 void ram(std::size_t,std::uint32_t);
 bool result(std::size_t,std::uint32_t&,bool&) const;
};
}
