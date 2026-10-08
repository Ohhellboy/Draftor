#pragma once
#include "hanime_extension/extension.hpp"
#include "device_metrics.hpp"
#include <memory>
#include <string>
#include <array>
#include "core_memory.hpp"
#include "resource_release.hpp"
#include "io_profile.hpp"
#include "hanime/base_order.hpp"
#include "cache_profile.hpp"
namespace ime_device {
struct Mount { uint64_t epoch=1; bool available=true; };
// Single loop task only. At most one bounded POSIX read per logical request.
// Cache belongs to this immutable file + mount epoch; no pointers escape.
class TfReader final:public hanime::ResourceReader {
 std::shared_ptr<const Mount> mount_; uint64_t epoch_=0; int fd_=-1; size_t bytes_=0;
 struct Line {size_t offset=0,size=0;uint64_t used=0;};
 mutable std::array<Line,16> lines_{};
 draftor::memory::ArrayPtr<uint8_t> cache_;
 size_t line_count_=0;
 mutable uint64_t serial_=0;
 mutable size_t position_=0;
 mutable bool position_known_=false,failed_=false;
 mutable IoCounters io_{};
 mutable uint64_t max_read_us_=0,max_seek_us_=0;
 FileRole role_=FileRole::Other;
 bool readBounded(size_t,void*,size_t,size_t limit)const;
 public:
 static constexpr size_t MaximumRead=1024;
 static constexpr size_t MaximumBlockRead=4096;
 static constexpr size_t CacheLines=16,CacheBytes=CacheLines*MaximumRead;
#ifndef ARDUINO
 // Optional Host evidence hook. It reports actual POSIX accesses after timing;
 // it does not replace the reader or participate in resource authorization.
 // Open/close carry path/non-null or offset=SIZE_MAX; reads carry path=null.
 using ReadObserver=void(*)(int fd,const char* path,size_t offset,size_t bytes);
 static ReadObserver readObserver;
#endif
 TfReader(std::shared_ptr<const Mount>,const std::string&,size_t cacheLines=0);
 const IoCounters& io()const{return io_;}
 uint64_t maxReadUs()const{return max_read_us_;}
 uint64_t maxSeekUs()const{return max_seek_us_;}
 size_t cacheBytes()const{return line_count_*MaximumRead;}
 const void* cacheAllocation()const{return cache_.get();}
 ~TfReader() override;
 bool valid()const noexcept;
 bool healthy()const override{return valid();}
 size_t size()const override {return valid()?bytes_:0;}
 bool read(size_t,void*,size_t)const override;
 // Uncached sequential-font/diagnostic path. Core ResourceReader remains 1KiB.
 bool readBlock(size_t offset,void* dst,size_t count)const{return !line_count_&&readBounded(offset,dst,count,MaximumBlockRead);}
};
// Each open/index/bootstrap/fixed activation is a separate scheduled step.
enum class ResourceState { Dormant, OpenShared, OpenSharedIndex, ValidateShared, ActivateShared, AdmitText, OpenScheme, OpenSchemeIndex, ValidateScheme, ActivateScheme, OpenOrder, OpenOrderIndex, ValidateOrder, ActivateOrder, Ready, Error };
class Resources {
 std::string root_,index_root_; std::shared_ptr<Mount> mount_; ReleasePins pins_;
 std::shared_ptr<TfReader> shared_reader_,shared_index_,scheme_reader_,scheme_index_,order_reader_,order_index_;
 std::shared_ptr<hanime::VerifiedResourceReader> verified_shared_,verified_scheme_,verified_order_;
 hanime::TextAccessCheck admission_;
 ResourceState state_=ResourceState::Dormant;
 hanime_extension::SchemeRules rules_=hanime_extension::SchemeRules::R1Xiaohe;
 std::string error_;
 uint64_t stage_work_=0;
 hanime::TextAccessStep admission_progress_{};
 bool fail(const char*,const char* detail="");
 void closeScheme();bool checkHealth();
 const char* orderFile(bool proof=false)const;
 const char* schemeFile(bool proof=false)const;
 public:
 hanime::PagedSharedResource shared;
 hanime::PagedSchemeIndex scheme;
 static constexpr hanime::RegistryNamespaceId Namespace=0x48414e494d450001ULL;
 static constexpr hanime::WorkBudget Budget{1,1024,1024};
 static constexpr hanime::WorkBudget InitBudget=Budget;
 static constexpr hanime::VerifiedResourceConfig VerifiedConfig{16,8,1024};
 static constexpr hanime::VerifiedResourceConfig SharedVerifiedConfig{SharedDataKiB,8,1024,SharedAnchorSlots};
 static constexpr size_t SharedCachePages=SharedPages;
 static constexpr hanime::OutputReuseConfig SharedOutputReuse{OutputEntries,OutputTextBytes,OutputFormBytes};
 // Four extra proof pages reduce measured BaseOrder rereads. Data pools and
 // shared/scheme quotas stay fixed. Payload is 58,368 B with mixed PackedAuth;
 // legacy format1 fixtures retain 65,536 B.
 static constexpr hanime::VerifiedResourceConfig OrderConfig{8,8,1024};
 static constexpr unsigned ResourceOpenFiles=6,DocumentOpenFiles=3; // Mixed production actually uses 5; reserve supports format1 fixtures.
 static constexpr unsigned MaximumOpenFiles=ResourceOpenFiles+DocumentOpenFiles; // Close old scheme/order before opening replacement.
 size_t cacheBytes()const;
 // These identify outer owners only, not their private allocations.
 const void* sharedCacheAllocation()const{return verified_shared_.get();}
 const void* schemeCacheAllocation()const{return verified_scheme_.get();}
 hanime::VerifiedResourceSnapshot sharedSnapshot()const{return verified_shared_?verified_shared_->snapshot():hanime::VerifiedResourceSnapshot{};}
 hanime::VerifiedResourceSnapshot schemeSnapshot()const{return verified_scheme_?verified_scheme_->snapshot():hanime::VerifiedResourceSnapshot{};}
 hanime::VerifiedResourceMemory sharedMemory()const{return verified_shared_?verified_shared_->memory_usage():hanime::VerifiedResourceMemory{};}
 hanime::VerifiedResourceMemory schemeMemory()const{return verified_scheme_?verified_scheme_->memory_usage():hanime::VerifiedResourceMemory{};}
 hanime::VerifiedResourceSnapshot orderSnapshot()const{return verified_order_?verified_order_->snapshot():hanime::VerifiedResourceSnapshot{};}
 hanime::VerifiedResourceMemory orderMemory()const{return verified_order_?verified_order_->memory_usage():hanime::VerifiedResourceMemory{};}
 bool optimized()const{return scheme.base_order()&&scheme.base_order()->ready();}
 explicit Resources(std::string root,std::shared_ptr<Mount> mount=std::make_shared<Mount>(),ReleasePins pins=productionPins(),std::string indexRoot={})
  :root_(std::move(root)),index_root_(indexRoot.empty()?root_:std::move(indexRoot)),mount_(std::move(mount)),pins_(pins){}
 ~Resources();
 // Candidate boot admission only: fixed file/marker stat checks, not authentication.
 // Call while Dormant, before any reader/cursor is created. Error stays latched.
 bool checkInstallation();
 void start(hanime_extension::SchemeRules);
 // Caller destroys Submission, Composition and display tickets BEFORE this call.
 void switchScheme(hanime_extension::SchemeRules);
 // Ready calls perform only fixed-cost health checks, no reads/advancement.
 void service();
 void retire();
 // Only after upper users are stopped; preserve the latched failure for UI.
 void releaseFailed();
 ResourceState state()const{return state_;}
 uint64_t stageWork()const{return stage_work_;}
 const hanime::TextAccessStep& admissionProgress()const{return admission_progress_;}
 bool ready()const{return state_==ResourceState::Ready&&shared.ready()&&scheme.ready()&&optimized();}
 bool autoSafe(std::string_view code)const {
  return ready() && rules_==hanime_extension::SchemeRules::DirectExact && pins_.direct &&
   std::string_view(pins_.direct->scheme_id)=="wubi86" &&
   wubiAutoSafe(code,pins_.direct->auto_safe,pins_.shared,pins_.direct->scheme,pins_.direct->order);
 }
 const std::string& error()const{return error_;}
 hanime_extension::SchemeRules rules()const{return rules_;}
 hanime_extension::SchemePolicy policy()const {
  return rules_==hanime_extension::SchemeRules::DirectExact?
   hanime_extension::SchemePolicy{rules_,pins_.direct?pins_.direct->scheme_id:""}:hanime_extension::spelling_policy(rules_);
 }
 std::shared_ptr<Mount> mount()const{return mount_;}
};
}
