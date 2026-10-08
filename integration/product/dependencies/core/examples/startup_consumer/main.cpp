#include "hanime/verified_resource.hpp"
#include "hanime_extension/extension.hpp"
#include "hanime_extension/submission.hpp"
#include "release_pin.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace hanime;
using namespace hanime_extension;
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }

struct Meter {
  std::uint64_t calls{0}, bytes{0}, discontinuities{0}, opens{0}, max_request{0};
  std::uint64_t calls_le64{0}, calls_le256{0}, calls_le1024{0};
  std::uint64_t handles{0}, handle_peak{0};
};
Meter difference(const Meter& end, const Meter& begin) {
  Meter r=end;
  r.calls-=begin.calls; r.bytes-=begin.bytes;
  r.discontinuities-=begin.discontinuities; r.opens-=begin.opens;
  r.calls_le64-=begin.calls_le64; r.calls_le256-=begin.calls_le256;
  r.calls_le1024-=begin.calls_le1024;
  return r;
}
// This is a read-only test storage port, not an all-file memory fixture. POSIX
// pread performs no lseek; discontinuities are a separately labelled model input.
class File final : public ResourceReader {
 public:
  File(const std::filesystem::path& path, Meter& m): meter_(m) {
    fd_=::open(path.c_str(),O_RDONLY);
    if(fd_<0) throw std::runtime_error("open failed: "+path.string());
    struct stat st{};
    if(::fstat(fd_,&st)!=0 || st.st_size<0) {
      ::close(fd_); fd_=-1; throw std::runtime_error("fstat failed");
    }
    size_=static_cast<std::size_t>(st.st_size);
    ++meter_.opens; ++meter_.handles;
    meter_.handle_peak=std::max(meter_.handle_peak,meter_.handles);
  }
  ~File() override { if(fd_>=0) { ::close(fd_); --meter_.handles; } }
  std::size_t size() const override { return size_; }
  bool healthy() const override { return fd_>=0; }
  bool read(std::size_t offset,void* dest,std::size_t count) const override {
    if(fd_<0 || count>1024 || offset>size_ || count>size_-offset || (!dest&&count)) return false;
    if(!count)return true;
    ++meter_.calls; meter_.bytes+=count;
    meter_.max_request=std::max<std::uint64_t>(meter_.max_request,count);
    if(count<=64) ++meter_.calls_le64;
    else if(count<=256) ++meter_.calls_le256;
    else ++meter_.calls_le1024;
    if(!positioned_ || offset!=next_) ++meter_.discontinuities;
    positioned_=true; next_=offset+count;
    const auto got=::pread(fd_,dest,count,static_cast<off_t>(offset));
    return got>=0 && static_cast<std::size_t>(got)==count;
  }
 private:
  Meter& meter_; int fd_{-1}; std::size_t size_{0};
  mutable bool positioned_{false}; mutable std::size_t next_{0};
};
// Before-path control: fixed 16 KiB, 1 KiB block LRU, matching the published
// old adapter's cache dimensions. Not its private implementation or a new SDK.
class LegacyCache final : public ResourceReader {
 public:
  explicit LegacyCache(std::shared_ptr<const ResourceReader> source): source_(std::move(source)) {}
  std::size_t size() const override { return source_->size(); }
  bool read(std::size_t offset,void* destination,std::size_t count) const override {
    if(offset>size() || count>size()-offset || (!destination&&count)) return false;
    auto* out=static_cast<std::uint8_t*>(destination);
    while(count) {
      const auto block=offset/1024, within=offset%1024;
      std::size_t slot=entries_.size();
      for(std::size_t i=0;i<entries_.size();++i) if(entries_[i].valid&&entries_[i].block==block) {slot=i;break;}
      if(slot==entries_.size()) {
        slot=0;
        for(std::size_t i=0;i<entries_.size();++i) {
          if(!entries_[i].valid) {slot=i;break;}
          if(entries_[i].age<entries_[slot].age) slot=i;
        }
        auto& e=entries_[slot]; e.valid=false;
        const auto bytes=std::min<std::size_t>(1024,size()-block*1024);
        if(!source_->read(block*1024,e.bytes.data(),bytes)) return false;
        e.block=block;e.valid=true;
      }
      auto& e=entries_[slot]; e.age=++age_;
      const auto n=std::min(count,1024-within);
      std::memcpy(out,e.bytes.data()+within,n); out+=n;offset+=n;count-=n;
    }
    return true;
  }
 private:
  struct Entry { std::array<std::uint8_t,1024> bytes{};std::size_t block{0},age{0};bool valid{false}; };
  std::shared_ptr<const ResourceReader> source_;
  mutable std::array<Entry,16> entries_{}; mutable std::size_t age_{0};
};
struct Phase {
  WorkCost cost; std::uint64_t steps{0},local{0}; Meter start;
  VerifiedResourceSnapshot shared_before,scheme_before;
  std::uint64_t scheme_generation{0};
};
struct PageValue {
  std::array<RankItem,6> items{};
  std::array<std::array<char,64>,6> texts{};
  std::array<std::size_t,6> lengths{};
  std::size_t size{0}, begin{0},end{0}; bool more{false};
};
bool same(const PageValue& a,const PageValue& b,bool compare_directional_more=true) {
  if(a.size!=b.size||a.begin!=b.begin||a.end!=b.end||
     (compare_directional_more&&a.more!=b.more)) return false;
  for(std::size_t i=0;i<a.size;++i) {
    const auto& x=a.items[i];const auto& y=b.items[i];
    if(x.key!=y.key||x.score!=y.score||x.ref!=y.ref||x.path_count!=y.path_count||
       x.scheme_weight!=y.scheme_weight||x.representative_ordinal!=y.representative_ordinal||
       a.lengths[i]!=b.lengths[i]||a.texts[i]!=b.texts[i]) return false;
  }
  return true;
}
struct Results {
  std::array<PageValue,6> pages{};std::size_t size{0};
  std::array<double,3> collision_model{};
};
class Document final : public TextCommitSink {
 public:
  bool commit(std::string_view text) override {
    ++calls;
    if(text.size()>bytes.size()-size) return false;
    if(!text.empty())std::copy(text.begin(),text.end(),bytes.begin()+static_cast<std::ptrdiff_t>(size));
    size+=text.size();return true;
  }
  std::string_view text() const {return {bytes.data(),size};}
  std::array<char,512> bytes{};std::size_t size{0},calls{0};
};

class Runtime {
 public:
  Runtime(bool fast,const std::filesystem::path& resources,const std::filesystem::path& index,std::size_t data_blocks,std::size_t index_pages,std::size_t work)
    : fast_(fast),resources_(resources),indexes_(index),data_blocks_(data_blocks),index_pages_(index_pages) {budget.max_work_items=work;}
  void begin(const char* name) {
    phase_={};phase_.start=meter;phase_name_=name;
    if(verified_shared_)phase_.shared_before=verified_shared_->snapshot();
    if(verified_scheme_)phase_.scheme_before=verified_scheme_->snapshot();
    phase_.scheme_generation=scheme_generation_;
  }
  void add(WorkCost c,std::size_t local=0) {
    require(c.read_calls<=budget.max_read_calls&&c.read_bytes<=budget.max_read_bytes&&
            c.work_items<=budget.max_work_items&&local<=budget.max_work_items,"step budget exceeded");
    ++phase_.steps;phase_.cost.read_calls+=c.read_calls;phase_.cost.read_bytes+=c.read_bytes;
    phase_.cost.work_items+=c.work_items;phase_.local+=local;
  }
  double print(double gate=0) {
    const auto m=difference(meter,phase_.start);
    // 85,672 B/s is the supplied sequential interval INCLUDING scheduling,
    // not raw SD bandwidth. Extra discontinuity and work terms are conservative
    // stress assumptions, not separately calibrated observations.
    const double base=static_cast<double>(m.bytes)/85672.0;
    const double model=base+static_cast<double>(m.discontinuities)*0.007+
      static_cast<double>(m.opens)*0.028+static_cast<double>(phase_.steps)*0.00005+
      static_cast<double>(phase_.cost.work_items+phase_.local)/3000000.0;
    std::cout<<"COST mode="<<(fast_?"fast":"legacy")<<" data_blocks="<<data_blocks_<<" index_pages="<<index_pages_<<" work_budget="<<budget.max_work_items<<" phase="<<phase_name_
      <<" steps="<<phase_.steps<<" port_calls="<<phase_.cost.read_calls
      <<" port_bytes="<<phase_.cost.read_bytes<<" work="<<phase_.cost.work_items<<" local="<<phase_.local
      <<" pread_calls="<<m.calls<<" pread_bytes="<<m.bytes<<" offset_discontinuities="<<m.discontinuities
      <<" lseek_calls=0 opens="<<m.opens<<" calls_le64="<<m.calls_le64<<" calls_65_256="<<m.calls_le256
      <<" calls_257_1024="<<m.calls_le1024<<" cumulative_max_request="<<m.max_request
      <<" sequential_envelope_s="<<base<<" virtual_stress_s="<<model;
    if(gate) {std::cout<<" gate_s="<<gate<<" gate="<<(model<=gate?"PASS":"FAIL");if(fast_&&model>gate)++gate_failures;}
    if(fast_) {
      const auto s=verified_shared_?verified_shared_->snapshot():VerifiedResourceSnapshot{};
      const auto h=verified_scheme_?verified_scheme_->snapshot():VerifiedResourceSnapshot{};
      const auto a=phase_.shared_before;
      const auto b=phase_.scheme_generation==scheme_generation_?phase_.scheme_before:VerifiedResourceSnapshot{};
      std::cout<<" authenticated_logical_requests="<<s.logical_requests-a.logical_requests+h.logical_requests-b.logical_requests
        <<" authenticated_logical_bytes="<<s.logical_bytes-a.logical_bytes+h.logical_bytes-b.logical_bytes
        <<" fixed_cache_only_requests="<<s.cache_only_requests-a.cache_only_requests+h.cache_only_requests-b.cache_only_requests
        <<" fixed_cache_only_bytes="<<s.cache_only_bytes-a.cache_only_bytes+h.cache_only_bytes-b.cache_only_bytes
        <<" verified_data_hits="<<s.data_cache_hits-a.data_cache_hits+h.data_cache_hits-b.data_cache_hits
        <<" verified_index_hits="<<s.index_cache_hits-a.index_cache_hits+h.index_cache_hits-b.index_cache_hits;
    }
    std::cout<<'\n';return model;
  }
  void mount_shared() {
    auto raw=std::make_shared<File>(resources_/"shared.hns",meter);
    std::string error;
    if(fast_) {
      auto index=std::make_shared<File>(indexes_/"shared.hnv",meter);
      verified_shared_=std::make_shared<VerifiedResourceReader>(raw,index,hanime_release::shared,VerifiedResourceConfig{data_blocks_,index_pages_,1024});
      bootstrap(*verified_shared_);
      require(shared.open_verified(verified_shared_,epoch,{2,1024},&error),error.c_str());
    } else {
      auto cached=std::make_shared<LegacyCache>(raw);
      require(shared.open(cached,hanime_release::shared.registry_namespace,hanime_release::shared.shared_bundle_id,epoch,{2,1024},&error),error.c_str());
      auto cur=shared.validation_cursor();
      for(std::size_t n=0;!shared.ready();++n) {
        require(n<30000000,"shared full validation timeout");
        const auto step=shared.advance_validation(cur,budget);add(step.cost);
        require(step.status==ValidationStepStatus::InProgress||step.status==ValidationStepStatus::Ready,"shared full validation failed");
      }
    }
  }
  void mount_scheme(bool full) {
    ++scheme_generation_;
    scheme.deactivate();
    if(verified_scheme_) {verified_scheme_->deactivate();verified_scheme_.reset();}
    const std::string id=full?"full_pinyin":"xiaohe";
    auto raw=std::make_shared<File>(resources_/(id+".hnm"),meter);
    std::string error;
    if(fast_) {
      auto index=std::make_shared<File>(indexes_/(id+".hnv"),meter);
      verified_scheme_=std::make_shared<VerifiedResourceReader>(raw,index,full?hanime_release::full_pinyin:hanime_release::xiaohe,VerifiedResourceConfig{data_blocks_,index_pages_,1024});
      bootstrap(*verified_scheme_);
      require(scheme.open_verified(verified_scheme_,shared,{1,1024},&error),error.c_str());
    } else {
      auto cached=std::make_shared<LegacyCache>(raw);
      require(scheme.open(cached,shared.info().registry_namespace,shared.info().shared_bundle_id,
                         shared.info().sections[0].count,{1,1024},&error),error.c_str());
      for(std::size_t n=0;!scheme.ready();++n) {
        require(n<30000000,"scheme full validation timeout");
        const auto step=scheme.advance_validation(budget);add(step.cost);
        require(step.status==ValidationStepStatus::InProgress||step.status==ValidationStepStatus::Ready,"scheme full validation failed");
      }
    }
  }
  void admit() {
    TextAccessCheck check;auto status=check.begin(shared,64,128);
    for(std::size_t n=0;status==TextAccessStatus::Pending;++n) {
      require(n<30000000,"admission timeout");const auto step=check.advance(budget);add(step.cost);status=step.status;
    }
    require(status==TextAccessStatus::Ready,"text capacity admission failed");
    require(check.certificate().current(shared,64,128),"certificate not current");
  }
  Status drain(Composition& c,Status initial) {
    if(initial!=Status::Working)return initial;
    const auto operation=c.operation();
    for(std::size_t n=0;n<30000000;++n) {
      const auto step=c.tick(operation,budget);add(step.core_cost,step.local_work);
      if(step.status!=Status::Working)return step.status;
    }
    throw std::runtime_error("composition timeout");
  }
  PageValue capture(Composition& c) {
    PageValue v;const auto p=c.page();require(p.size<=v.items.size(),"page bound");
    v.size=p.size;v.begin=p.begin;v.end=p.end;v.more=p.has_more;
    for(std::size_t i=0;i<p.size;++i)v.items[i]=p.items[i];
    for(std::size_t i=0;i<p.size;++i) {
      require(drain(c,c.read_text(c.selection(i)))==Status::Ready,"candidate text failed");
      const auto text=c.last_text();require(text.size()<=v.texts[i].size(),"text bound");
      if(!text.empty())std::copy(text.begin(),text.end(),v.texts[i].begin());
      v.lengths[i]=text.size();
    }
    return v;
  }
  Selection find(Composition& c,std::string_view wanted) {
    for(std::size_t pages=0;pages<1000;++pages) {
      const auto p=c.page();
      for(std::size_t i=0;i<p.size;++i) {
        auto h=c.selection(i);require(drain(c,c.read_text(h))==Status::Ready,"selected text failed");
        if(c.last_text()==wanted)return h;
      }
      if(!p.has_more)break;
      require(drain(c,c.request_page(RangeDirection::After))==Status::PageReady,"find next failed");
    }
    throw std::runtime_error("target text absent");
  }
  void cold_block() {
    if(!fast_)return;
    begin("uncached_authentication_block");
    const ResourceReadContext context{std::make_shared<const int>(1),1};
    std::array<std::uint8_t,32> bytes{};ResourceReadStatus status=ResourceReadStatus::Pending;
    for(std::size_t n=0;status==ResourceReadStatus::Pending;++n) {
      require(n<100000,"block read timeout");
      const auto s=verified_shared_->read_budgeted(4*1024*1024+19,bytes.data(),bytes.size(),budget,context);add(s.cost);status=s.status;
    }
    if(status!=ResourceReadStatus::Ready)throw std::runtime_error(std::string("cold block failed: ")+verified_shared_->error());
    print(0.2);
    const auto m=meter;
    require(verified_shared_->read(4*1024*1024+19,bytes.data(),bytes.size()),"verified cache miss");
    require(m.calls==meter.calls&&m.bytes==meter.bytes,"cache-only read did I/O");
  }
  void memory(const Composition& c,const Submission* submission=nullptr) const {
    const auto m=c.memory_usage();
    const auto s=shared.memory_usage();
    // Enumerate only disjoint known allocations, not overlapping diagnostic
    // subfields (group/cursors/workspace alias the owning Impl/extension arrays).
    std::size_t known=s.owner_fixed_bytes+s.cache_payload_allocated_bytes+s.cache_metadata_allocated_bytes+
      scheme.resident_page_bytes()+m.impl_bytes+m.composition_buffers+m.rank_arrays+m.page_array+m.output_buffers+
      m.rank.owner_bytes+m.rank.impl_bytes+m.rank.lifetime_bytes+m.rank.batch_capacity_bytes+
      m.rank.raw.impl_bytes+m.rank.raw.code_heap_bytes+m.rank.raw.page_capacity_bytes;
    std::cout<<"MEMORY mode="<<(fast_?"fast":"legacy")<<" extension_impl="<<m.impl_bytes
      <<" extension_arrays="<<m.composition_buffers+m.rank_arrays+m.page_array+m.output_buffers
      <<" shared_owner="<<s.owner_fixed_bytes<<" shared_cache="<<s.cache_payload_allocated_bytes
      <<" scheme_resident="<<scheme.resident_page_bytes()<<" handle_peak="<<meter.handle_peak
      <<" sizeof_runtime="<<sizeof(Runtime)<<" sizeof_page_value="<<sizeof(PageValue);
    if(fast_) {
      const auto a=verified_shared_->memory_usage(),b=verified_scheme_->memory_usage();
      known+=a.owner_fixed_bytes+b.owner_fixed_bytes+a.data_payload_bytes+b.data_payload_bytes+
        a.index_payload_bytes+b.index_payload_bytes+a.data_metadata_bytes+b.data_metadata_bytes+
        a.index_metadata_bytes+b.index_metadata_bytes+4*sizeof(File);
      std::cout<<" verified_owner="<<a.owner_fixed_bytes+b.owner_fixed_bytes
        <<" verified_data="<<a.data_payload_bytes+b.data_payload_bytes
        <<" verified_index="<<a.index_payload_bytes+b.index_payload_bytes
        <<" verified_metadata="<<a.data_metadata_bytes+b.data_metadata_bytes+a.index_metadata_bytes+b.index_metadata_bytes
        <<" shared_full_file_checked="<<verified_shared_->snapshot().whole_file_checked;
    } else {
      known+=2*sizeof(LegacyCache)+2*sizeof(File);
      std::cout<<" legacy_cache_objects="<<2*sizeof(LegacyCache);
    }
    if(submission) {
      const auto x=submission->memory_usage();const auto& k=x.core;
      const auto submit_known=x.impl_bytes+x.capture_array_bytes+k.impl_bytes+k.members_bytes+
        k.request_names_bytes+k.resolver_name_bytes+k.member_text_bytes+k.total_text_bytes+
        k.owner_token_bytes+k.receipt_bytes;
      known+=submit_known;
      std::cout<<" submission_known="<<submit_known<<" submission_capture="<<x.capture_array_bytes
        <<" submission_members="<<k.members_bytes<<" submission_names="<<k.request_names_bytes
        <<" submission_text="<<k.member_text_bytes+k.total_text_bytes
        <<" submission_tokens_payload="<<k.owner_token_bytes+k.receipt_bytes;
    }
    std::cout<<" enumerated_disjoint_resident_components="<<known
      <<" exclusions=scheme_Impl,control_blocks,allocator_overhead,paths,iostream,stack,OS"
      <<" sizeof_results="<<sizeof(Results)<<" sizeof_document="<<sizeof(Document)
      <<" full_core_peak_measured=0\n";
  }
  PagedSharedResource shared;PagedSchemeIndex scheme;Meter meter;
  WorkBudget budget{1,1024,1024};MountEpoch epoch{9};std::size_t gate_failures{0};
 private:
  void bootstrap(VerifiedResourceReader& r) {
    require(r.prepare_bootstrap(),"bootstrap rejected");
    for(std::size_t n=0;!r.bootstrap_ready();++n) {
      require(n<100000,"bootstrap timeout");const auto s=r.advance_bootstrap(budget);add(s.cost);
      if(s.status==ResourceReadStatus::Error)throw std::runtime_error(r.error());
    }
  }
  bool fast_;std::filesystem::path resources_,indexes_;std::size_t data_blocks_,index_pages_;
  std::uint64_t scheme_generation_{0};Phase phase_;const char* phase_name_{"none"};
  std::shared_ptr<VerifiedResourceReader> verified_shared_,verified_scheme_;
};

Results run(bool fast,const std::filesystem::path& files,const std::filesystem::path& indexes,std::size_t& gate_failures,std::size_t data_blocks,std::size_t index_pages,std::size_t work) {
  Runtime r(fast,files,indexes,data_blocks,index_pages,work);Results result;
  r.begin("shared_activation");r.mount_shared();r.print(0.6);
  r.begin("text_admission");r.admit();r.print(0.1);
  r.begin("xiaohe_activation");r.mount_scheme(false);r.print(1.3);
  const auto activation=r.shared.activation_handle();
  {
    Composition c(r.scheme,r.shared,SchemeRules::R1Xiaohe);require(c.valid(),"v0.5 consumer invalid");
    Document doc;Submission submit(c,r.shared,doc);
    r.begin("first_query_and_first_visible_page");
    require(c.edit_raw(0,0,"wjdjle")==Status::Ready,"initial raw edit");
    require(r.drain(c,c.query_longest())==Status::PageReady,"first query failed");
    result.pages[result.size++]=r.capture(c);r.print(2.0);
    r.begin("two_member_prepare_and_accept");
    require(r.drain(c,c.select(r.find(c,"完蛋")))==Status::Ready,"first select");
    require(r.drain(c,c.choose_prefix(6))==Status::PageReady,"second prefix");
    require(r.drain(c,c.select(r.find(c,"了")))==Status::Ready,"second select");
    require(doc.calls==0&&doc.size==0,"selection changed final document");
    auto prepared=submit.prepare();
    for(std::size_t n=0;prepared.core.status==CompositionStatus::Preparing;++n) {
      require(n<100000,"submission timeout");
      const auto step=submit.advance(prepared.transaction,r.budget);r.add(step.cost);prepared=step.result;
    }
    require(prepared.core.status==CompositionStatus::Ready&&doc.calls==0,"prepare changed document");
    const auto accepted=submit.confirm(prepared.transaction);
    require(accepted.core.status==CompositionStatus::Accepted&&doc.text()=="完蛋了"&&doc.calls==1,"atomic accept");
    submit.confirm(prepared.transaction);require(doc.calls==1,"duplicate sink call");
    require(c.count_begin_calls()==0,"Disabled accessed count source");r.print();
    r.begin("cancel_delete_empty_new_input");
    require(c.edit_raw(0,0,"wjdjle")==Status::Ready,"cancel setup");
    require(c.query_longest()==Status::Working,"cancel query setup");const auto old=c.operation();
    require(c.cancel(old)==Status::Cancelled,"cancel failed");
    require(c.edit_raw(0,c.snapshot().raw.size(),"")==Status::Ready,"delete empty failed");
    require(c.edit_raw(0,0,"wjdjle")==Status::Ready,"new input failed");
    require(c.query_longest()==Status::Working,"new query failed");
    const auto stale=c.tick(old,r.budget);r.add(stale.core_cost,stale.local_work);
    require(stale.status==Status::Stale,"old operation entered new query");
    require(r.drain(c,Status::Working)==Status::PageReady,"new query after stale failed");
    require(r.shared.activation_is_current(activation),"cancel restarted shared activation");r.print();
    r.begin("collision_first_range");
    require(c.edit_raw(0,c.snapshot().raw.size(),"ui")==Status::Ready,"collision edit");
    require(r.drain(c,c.choose_prefix(2))==Status::PageReady,"collision query");
    result.pages[result.size++]=r.capture(c);result.collision_model[0]=r.print();
    require(c.page().has_more,"expected high collision has no next page");
    r.begin("collision_after");
    require(r.drain(c,c.request_page(RangeDirection::After))==Status::PageReady,"After failed");
    result.pages[result.size++]=r.capture(c);result.collision_model[1]=r.print();
    r.begin("collision_before");
    require(r.drain(c,c.request_page(RangeDirection::Before))==Status::PageReady,"Before failed");
    result.pages[result.size++]=r.capture(c);result.collision_model[2]=r.print();
    // has_more is relative to the requested direction: Before at the first
    // page has no further previous range; First can still have later ranges.
    require(same(result.pages[1],result.pages[3],false),"Before did not rebuild first page");r.memory(c,&submit);
  }
  r.cold_block();
  r.begin("switch_to_full_pinyin");r.mount_scheme(true);r.print(2.0);
  require(r.shared.activation_is_current(activation),"scheme switch restarted shared activation");
  {
    Composition c(r.scheme,r.shared,SchemeRules::FullPinyin);require(c.valid(),"full consumer invalid");
    r.begin("full_pinyin_query_after_switch");
    require(c.edit_raw(0,0,"wandanle")==Status::Ready,"full edit");
    require(r.drain(c,c.query_longest())==Status::PageReady,"full query");
    result.pages[result.size++]=r.capture(c);r.print(2.0);r.memory(c);
  }
  gate_failures+=r.gate_failures;
  std::cout<<"PASS public_basic_host mode="<<(fast?"fast":"legacy")
    <<" stages=bootstrap,query,text,submit,cancel,delete,new_input,After,Before,switch Disabled=1\n";
  return result;
}
} // namespace

int main(int argc,char** argv) {
  try {
    if(argc<3||argc>7) {std::cerr<<"usage: startup_consumer RESOURCE_DIR VERIFIED_INDEX_DIR [DATA_BLOCKS=default] [both|fast] [INDEX_PAGES=default] [WORK_ITEMS=1024]\n";return 2;}
    const auto data_blocks=argc>=4?static_cast<std::size_t>(std::stoul(argv[3])):VerifiedResourceConfig{}.data_cache_blocks;
    require(data_blocks>=3&&data_blocks<=32,"data blocks outside measured bounded choices");
    const bool only_fast=argc>=5&&std::string_view(argv[4])=="fast";
    const auto index_pages=argc>=6?static_cast<std::size_t>(std::stoul(argv[5])):VerifiedResourceConfig{}.index_cache_pages;
    require(index_pages>=1&&index_pages<=32,"index pages outside measured bounded choices");
    const auto work=argc==7?static_cast<std::size_t>(std::stoul(argv[6])):1024;
    require(work>=128&&work<=1024,"work items outside measured bounded choices");
    std::cout<<std::fixed<<std::setprecision(6);
    std::cout<<"MODEL block_bytes="<<kVerifiedBlockBytes
      <<" sequential_wall_envelope_Bps=85672 extra_discontinuity_s=0.007 open_s=0.028"
      <<" extra_schedule_s_per_step=0.00005 work_items_per_s=3000000"
      <<" extra_terms=uncalibrated_conservative_assumptions font_ble_excluded=1\n";
    std::size_t failures=0;
    Results old;
    if(!only_fast)old=run(false,argv[1],argv[2],failures,data_blocks,index_pages,work);
    const auto fast=run(true,argv[1],argv[2],failures,data_blocks,index_pages,work);
    if(!only_fast) {
      require(old.size==fast.size,"comparison page count mismatch");
      for(std::size_t i=0;i<old.size;++i)require(same(old.pages[i],fast.pages[i]),"legacy/fast ranking, identity, path or text mismatch");
      std::cout<<"PASS full_validation_vs_trusted_release pages="<<old.size
        <<" independent_complete_keys=1 unchanged_basic_host=0.5.0\n";
      for(std::size_t i=0;i<old.collision_model.size();++i) {
        const auto ratio=fast.collision_model[i]/old.collision_model[i];
        std::cout<<"RELATIVE_COLLISION_GATE range="<<i<<" model_ratio="<<ratio
          <<" maximum=1.250000 status="<<(ratio<=1.25?"PASS":"FAIL")<<'\n';
        if(ratio>1.25)++failures;
      }
    }
    if(failures) {std::cerr<<"FAIL virtual startup/interaction gates="<<failures<<" (not a measured S3 result)\n";return 3;}
    std::cout<<"PASS named_sample_virtual_cost_gates; DEVICE_STARTUP_30_SECONDS=OPEN;"
      <<" collision_absolute_latency_not_accepted=1 no learning files opened\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
