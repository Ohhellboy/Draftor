#pragma once
#include "hanime_extension/extension.hpp"
#include "hanime_extension/submission.hpp"
#include "hanime/host_resource_file.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace extension_host {
using namespace hanime;
using namespace hanime_extension;
inline void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
struct Totals {
  std::uint64_t steps{0}, reads{0}, bytes{0}, work{0}, local{0};
  void add(WorkCost c) { ++steps; reads+=c.read_calls; bytes+=c.read_bytes; work+=c.work_items; }
  void print(const char* phase) const {
    std::cout<<"COST "<<phase<<" steps="<<steps<<" reads="<<reads<<" bytes="<<bytes<<" work="<<work<<" local="<<local<<'\n';
  }
};
struct Runtime {
  HostResourceMetrics metrics;
  PagedSharedResource shared;
  PagedSchemeIndex scheme;
  WorkBudget budget{1,1024,128};
  Totals validation, admission, operations;
  std::size_t cache_shared{2}, cache_scheme{1};
  void mount(std::shared_ptr<const ResourceReader> h,std::shared_ptr<const ResourceReader> s, RegistryNamespaceId ns, MountEpoch epoch=1) {
    std::string error;
    require(shared.open(h,ns,0,epoch,{cache_shared,1024},&error),error.c_str());
    auto cursor=shared.validation_cursor();
    for (std::size_t i=0;!shared.ready();++i) {
      require(i<30000000,"shared validation timeout");
      auto r=shared.advance_validation(cursor,budget); validation.add(r.cost);
      require(r.status==ValidationStepStatus::InProgress||r.status==ValidationStepStatus::Ready,"shared validation failed");
    }
    require(scheme.open(s,ns,shared.info().shared_bundle_id,shared.info().sections[0].count,{cache_scheme,1024},&error),error.c_str());
    for (std::size_t i=0;!scheme.ready();++i) {
      require(i<30000000,"scheme validation timeout");
      auto r=scheme.advance_validation(budget); validation.add(r.cost);
      require(r.status==ValidationStepStatus::InProgress||r.status==ValidationStepStatus::Ready,"scheme validation failed");
    }
  }
  void mount_files(const std::filesystem::path& dir,const std::string& id) {
    auto h=std::make_shared<HostResourceFile>((dir/"shared.hns").string(),1024,metrics);
    auto s=std::make_shared<HostResourceFile>((dir/(id+".hnm")).string(),1024,metrics);
    require(h->valid()&&s->valid(),"resource file open failed");
    mount(h,s,0x48414e494d450001ULL);
  }
  TextAccessStatus admit(Config c={}) {
    TextAccessCheck check;
    auto status=check.begin(shared,c.word_bytes,c.name_bytes);
    while (status==TextAccessStatus::Pending) { auto r=check.advance(budget); admission.add(r.cost); status=r.status; }
    check.cancel(); return status;
  }
  Status drain(Composition& c, Status initial=Status::Working) {
    if (initial!=Status::Working) return initial;
    auto op=c.operation();
    for (std::size_t i=0;i<30000000;++i) {
      auto r=c.tick(op,budget); operations.add(r.core_cost); operations.local+=r.local_work;
      require(r.core_cost.read_calls<=budget.max_read_calls && r.core_cost.read_bytes<=budget.max_read_bytes &&
        r.core_cost.work_items<=budget.max_work_items && r.local_work<=budget.max_work_items,"tick budget exceeded");
      if (r.status!=Status::Working) return r.status;
    }
    throw std::runtime_error("extension operation timeout");
  }
};
// This object owns the final document, not a queue/forwarding receiver.
// A successful sink call atomically appends the complete UTF-8 string here.
class FinalDocument : public TextCommitSink {
 public:
  explicit FinalDocument(std::size_t capacity=4096): text_(capacity) {}
  bool accept(std::string_view text) {
    ++calls_;
    if (text.size()>text_.size()-size_) return false;
    if (!text.empty()) std::copy(text.begin(),text.end(),text_.begin()+static_cast<std::ptrdiff_t>(size_));
    size_+=text.size(); return true;
  }
  bool commit(std::string_view text) override { return accept(text); }
  std::string_view text() const { return {text_.data(),size_}; }
  std::size_t calls() const { return calls_; }
 private:
  std::vector<char> text_;
  std::size_t size_{0},calls_{0};
};
inline SubmissionResult drain_submission(Runtime& runtime,Submission& submission,SubmissionResult result) {
  const auto handle=result.transaction;
  for(std::size_t i=0;result.core.status==CompositionStatus::Preparing;++i) {
    require(i<30000000,"submission timeout");
    const auto step=submission.advance(handle,runtime.budget);
    require(step.cost.read_calls<=runtime.budget.max_read_calls&&step.cost.read_bytes<=runtime.budget.max_read_bytes&&
      step.cost.work_items<=runtime.budget.max_work_items,"submission budget exceeded");
    runtime.operations.add(step.cost);result=step.result;
  }
  return result;
}
inline Selection find_text(Runtime& r,Composition& c,std::string_view text) {
  for (std::size_t pages=0;pages<10000;++pages) {
    auto p=c.page(); require(p.size!=0,"no candidate page");
    for (std::size_t i=0;i<p.size;++i) {
      auto h=c.selection(i);
      auto s=r.drain(c,c.read_text(h));
      if (s==Status::Ready && c.last_text()==text) return h;
    }
    if (!p.has_more) break;
    require(r.drain(c,c.request_page(RangeDirection::After))==Status::PageReady,"next page failed");
  }
  throw std::runtime_error("requested text not mapped");
}
inline void choose(Runtime& r,Composition& c,std::size_t end,std::string_view text) {
  require(r.drain(c,c.choose_prefix(end))==Status::PageReady,"prefix query failed");
  auto selection=find_text(r,c,text);
  require(r.drain(c,c.select(selection))==Status::Ready,"segment selection failed");
}
inline void print_memory(Runtime& r,Composition& c) {
  const auto m=c.memory_usage(); const auto sh=r.shared.memory_usage();
  std::cout<<"MEMORY extension_owner="<<m.owner_bytes<<" impl="<<m.impl_bytes<<" composition_arrays="<<m.composition_buffers
    <<" rank_arrays="<<m.rank_arrays<<" page_arrays="<<m.page_array<<" output_buffers="<<m.output_buffers
    <<" shared_owner="<<sh.owner_fixed_bytes<<" shared_cache="<<sh.cache_payload_allocated_bytes
    <<" shared_cache_metadata="<<sh.cache_metadata_allocated_bytes<<" scheme_resident_pages="<<r.scheme.resident_page_bytes()
    <<" rank_impl="<<m.rank.impl_bytes<<" rank_lifetime="<<m.rank.lifetime_bytes<<" rank_raw_impl="<<m.rank.raw.impl_bytes
    <<" rank_raw_code_heap="<<m.rank.raw.code_heap_bytes<<" rank_raw_page="<<m.rank.raw.page_capacity_bytes<<'\n';
}
} // namespace extension_host
