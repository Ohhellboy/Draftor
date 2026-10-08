#include "hanime/base_order.hpp"
#include "hanime/verified_resource.hpp"
#include "hanime_extension/extension.hpp"
#include "hanime_extension/submission.hpp"
#include "release_pin.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
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
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr WorkBudget budget{1, 1024, 1024};
constexpr std::size_t page_capacity = 6;
std::string_view perf_profile{"off"};
bool reuse_output() {return perf_profile=="output" || perf_profile=="balanced" || perf_profile=="extra";}
bool reuse_anchors() {return perf_profile=="anchors" || perf_profile=="balanced" || perf_profile=="extra";}

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::uint64_t elapsed_ns(Clock::time_point start) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}

std::string key_hex(const StableLexemeKey& key) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const auto byte : key.bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}

struct PhysicalCost {
  std::uint64_t reads{0}, bytes{0}, seeks{0}, opens{0};
  std::uint64_t read_ns{0}, seek_ns{0}, max_read_ns{0}, max_seek_ns{0};
};

// Only per-phase counters reside in RAM. Every raw read is streamed to a file.
struct Trace {
  explicit Trace(const fs::path& path) : file(path) {
    require(static_cast<bool>(file), "cannot create read trace");
  }
  std::ofstream file;
  std::string phase{"none"};
  std::array<PhysicalCost, 6> costs{};
  std::uint64_t sequence{0};

  void begin(const std::string& tag) {
    phase = tag;
    costs = {};
  }
};

constexpr std::array<const char*, 6> role_names{
    "shared_data", "shared_proof", "scheme_data", "scheme_proof",
    "order_data", "order_proof"};

// A real read+lseek port. Consecutive requests do not perform a redundant seek.
// This port has no resource cache. Durations are Host POSIX, never SD/TF timing.
class File final : public ResourceReader {
 public:
  File(const fs::path& path, Trace& trace, std::size_t role)
      : trace_(trace), role_(role) {
    descriptor_ = ::open(path.c_str(), O_RDONLY);
    require(descriptor_ >= 0, "open failed: " + path.string());
    struct stat info{};
    if (::fstat(descriptor_, &info) != 0 || info.st_size < 0) {
      ::close(descriptor_);
      descriptor_ = -1;
      throw std::runtime_error("fstat failed");
    }
    size_ = static_cast<std::size_t>(info.st_size);
    ++trace_.costs[role_].opens;
  }

  ~File() override {
    if (descriptor_ >= 0) ::close(descriptor_);
  }
  std::size_t size() const override { return size_; }
  bool healthy() const override { return descriptor_ >= 0; }

  bool read(std::size_t offset, void* destination, std::size_t bytes) const override {
    if (!bytes) return true;
    if (!destination || descriptor_ < 0 || bytes > 1024 || offset > size_ ||
        bytes > size_ - offset) return false;
    auto& cost = trace_.costs[role_];
    const bool discontinuous = offset != position_;
    std::uint64_t seek_time = 0;
    if (discontinuous) {
      const auto start = Clock::now();
      const auto positioned = ::lseek(descriptor_, static_cast<off_t>(offset), SEEK_SET);
      seek_time = elapsed_ns(start);
      ++cost.seeks;
      cost.seek_ns += seek_time;
      cost.max_seek_ns = std::max(cost.max_seek_ns, seek_time);
      if (positioned != static_cast<off_t>(offset)) return false;
      position_ = offset;
    }
    const auto start = Clock::now();
    const auto got = ::read(descriptor_, destination, bytes);
    const auto read_time = elapsed_ns(start);
    ++cost.reads;
    cost.bytes += bytes;
    cost.read_ns += read_time;
    cost.max_read_ns = std::max(cost.max_read_ns, read_time);
    if (got > 0) position_ += static_cast<std::size_t>(got);
    trace_.file << "{\"sequence\":" << ++trace_.sequence
                << ",\"phase\":" << std::quoted(trace_.phase)
                << ",\"role\":" << std::quoted(role_names[role_])
                << ",\"offset\":" << offset << ",\"bytes\":" << bytes
                << ",\"seek\":" << (discontinuous ? 1 : 0)
                << ",\"read_ns\":" << read_time << ",\"seek_ns\":" << seek_time
                << ",\"returned\":" << got << "}\n";
    return got >= 0 && static_cast<std::size_t>(got) == bytes;
  }

 private:
  Trace& trace_;
  std::size_t role_{0}, size_{0};
  int descriptor_{-1};
  mutable std::size_t position_{0};
};

// Final bounded document model, not an intermediate submission queue.
class Document final : public TextCommitSink {
 public:
  bool commit(std::string_view text) override {
    ++calls;
    if (text.size() > buffer.size() - size) return false;
    std::copy(text.begin(), text.end(), buffer.begin() + static_cast<std::ptrdiff_t>(size));
    size += text.size();
    return true;
  }
  std::string_view text() const { return {buffer.data(), size}; }
  std::array<char, 512> buffer{};
  std::size_t size{0}, calls{0};
};

struct PageValue {
  std::array<RankItem, page_capacity> items{};
  std::array<std::array<char, 64>, page_capacity> texts{};
  std::array<std::size_t, page_capacity> lengths{};
  std::size_t size{0}, begin{0}, end{0};
};

bool same_page(const PageValue& a, const PageValue& b) {
  if (a.size != b.size || a.begin != b.begin || a.end != b.end) return false;
  for (std::size_t i = 0; i < a.size; ++i) {
    const auto& x = a.items[i];
    const auto& y = b.items[i];
    if (x.key != y.key || x.ref != y.ref || x.score != y.score ||
        x.path_count != y.path_count || x.scheme_weight != y.scheme_weight ||
        x.representative_ordinal != y.representative_ordinal ||
        a.lengths[i] != b.lengths[i] || a.texts[i] != b.texts[i]) return false;
  }
  return true;
}

class Runtime {
 public:
  Runtime(const fs::path& resources, const fs::path& release, Trace& trace,
          std::string test_name, bool pinyin = false)
      : resources_(resources), release_(release), trace_(trace), test_name_(std::move(test_name)) {
    begin("shared_activation");
    VerifiedResourceConfig verified{perf_profile=="balanced" ? 15U : 16U,8,1024};
    PagedSharedConfig shared_config{perf_profile=="balanced" ? 1U : 2U,1024,true};
#if defined(HANIME_OUTPUT_REUSE_API)
    if (reuse_anchors()) verified.packed_anchor_slots=16;
    if (reuse_output()) shared_config.output_reuse={12,64,64};
#else
    require(!reuse_anchors() && !reuse_output(),"output reuse requires API0.12.0");
#endif
    open_reader(0, resources_ / "shared.hns", release_ / "shared.hnv",hanime_release::shared,verified);
    std::string error;
    require(shared.open_verified(readers_[0], 9, shared_config, &error), error);
    finish();
    const auto& layout = shared.info();
    std::cout << "SHARED_LAYOUT {\"page_bytes\":" << layout.page_bytes
              << ",\"header_end\":" << layout.sections[0].offset
              << ",\"payload_offset\":" << layout.payload_offset
              << ",\"payload_bytes\":" << layout.payload_bytes << ",\"sections\":[";
    for (std::size_t i = 0; i < layout.sections.size(); ++i) {
      if (i) std::cout << ',';
      const auto& section = layout.sections[i];
      std::cout << "{\"offset\":" << section.offset << ",\"pages\":" << section.page_count
                << ",\"record_bytes\":" << section.record_bytes
                << ",\"count\":" << section.count << '}';
    }
    std::cout << "]}\n";
    mount_scheme(pinyin);
  }

  void begin(const std::string& stage) {
    trace_.begin(test_name_ + "/" + scheme_name_ + "/" + stage);
    steps_ = work_ = local_work_ = fixed_command_work_ = logical_calls_ = logical_bytes_ = max_tick_ns_ = 0;
    for (std::size_t i = 0; i < readers_.size(); ++i) {
      snapshots_[i] = readers_[i] ? readers_[i]->snapshot() : VerifiedResourceSnapshot{};
    }
  }

  void add(const WorkCost& cost, std::size_t local_work, Clock::time_point start) {
    require(cost.read_calls <= budget.max_read_calls &&
            cost.read_bytes <= budget.max_read_bytes &&
            cost.work_items <= budget.max_work_items &&
            local_work <= budget.max_work_items, "advance exceeded fixed budget");
    ++steps_;
    work_ += cost.work_items;
    local_work_ += local_work;
    logical_calls_ += cost.read_calls;
    logical_bytes_ += cost.read_bytes;
    max_tick_ns_ = std::max(max_tick_ns_, elapsed_ns(start));
  }

  void finish() {
    PhysicalCost total;
    for (const auto& cost : trace_.costs) {
      total.reads += cost.reads;
      total.bytes += cost.bytes;
      total.seeks += cost.seeks;
      total.opens += cost.opens;
      total.read_ns += cost.read_ns;
      total.seek_ns += cost.seek_ns;
      total.max_read_ns = std::max(total.max_read_ns, cost.max_read_ns);
      total.max_seek_ns = std::max(total.max_seek_ns, cost.max_seek_ns);
    }
    std::cout << "COST {\"phase\":" << std::quoted(trace_.phase)
              << ",\"reads\":" << total.reads << ",\"bytes\":" << total.bytes
              << ",\"seeks\":" << total.seeks << ",\"opens\":" << total.opens
              << ",\"read_ns\":" << total.read_ns << ",\"seek_ns\":" << total.seek_ns
              << ",\"max_read_ns\":" << total.max_read_ns
              << ",\"max_seek_ns\":" << total.max_seek_ns
              << ",\"max_tick_ns\":" << max_tick_ns_
              << ",\"steps\":" << steps_ << ",\"work\":" << work_
              << ",\"local_work\":" << local_work_
              << ",\"fixed_composition_command_work\":" << fixed_command_work_
              << ",\"budget_port_calls\":" << logical_calls_
              << ",\"budget_port_bytes\":" << logical_bytes_;
    for (std::size_t i = 0; i < trace_.costs.size(); ++i) {
      std::cout << ",\"" << role_names[i] << "_bytes\":" << trace_.costs[i].bytes
                << ",\"" << role_names[i] << "_reads\":" << trace_.costs[i].reads
                << ",\"" << role_names[i] << "_seeks\":" << trace_.costs[i].seeks;
    }
    std::cout << "}\n";
#if defined(HANIME_OUTPUT_REUSE_API)
    const auto reuse=shared.output_reuse_stats();
    std::cout << "OUTPUT_REUSE {\"phase\":" << std::quoted(trace_.phase)
              << ",\"lookups\":" << reuse.lookups << ",\"hits\":" << reuse.hits
              << ",\"misses\":" << reuse.misses << ",\"inserts\":" << reuse.inserts
              << ",\"evictions\":" << reuse.evictions << ",\"resident\":" << reuse.resident << "}\n";
#endif
    for (std::size_t i = 0; i < readers_.size(); ++i) {
      if (!readers_[i]) continue;
      const auto current = readers_[i]->snapshot();
      const auto& before = snapshots_[i];
      std::uint32_t verification_format = 1;
      std::size_t logical_block_bytes = 1024;
      std::uint64_t packet_loads = 0;
#if defined(HANIME_VERIFICATION_FORMAT_MAX) && HANIME_VERIFICATION_FORMAT_MAX >= 2
      verification_format = readers_[i]->spec().verification_format;
      logical_block_bytes = current.data_block_bytes;
      packet_loads = current.authenticated_packet_loads - before.authenticated_packet_loads;
#endif
      std::cout << "AUTH {\"phase\":" << std::quoted(trace_.phase)
                << ",\"role\":" << std::quoted(role_names[2 * i])
                << ",\"verification_format\":" << verification_format
                << ",\"logical_block_bytes\":" << logical_block_bytes
                << ",\"packet_loads\":" << packet_loads
                << ",\"logical_requests\":" << current.logical_requests - before.logical_requests
                << ",\"logical_bytes\":" << current.logical_bytes - before.logical_bytes
                << ",\"cache_only_requests\":" << current.cache_only_requests - before.cache_only_requests
                << ",\"cache_only_bytes\":" << current.cache_only_bytes - before.cache_only_bytes
                << ",\"data_hits\":" << current.data_cache_hits - before.data_cache_hits
                << ",\"proof_hits\":" << current.index_cache_hits - before.index_cache_hits
                << ",\"data_loads\":" << current.authenticated_loads - before.authenticated_loads
                << ",\"proof_loads\":" << current.authenticated_index_loads - before.authenticated_index_loads
                << ",\"resident_data_begin\":" << before.resident_verified
                << ",\"resident_proof_begin\":" << before.resident_verified_index_pages
                << ",\"resident_data\":" << current.resident_verified
                << ",\"resident_proof\":" << current.resident_verified_index_pages
                << ",\"whole_file_checked\":" << (current.whole_file_checked ? "true" : "false");
#if defined(HANIME_OUTPUT_REUSE_API)
      std::cout << ",\"anchor_hits\":" << current.anchor_hits-before.anchor_hits
                << ",\"anchor_misses\":" << current.anchor_misses-before.anchor_misses
                << ",\"anchor_inserts\":" << current.anchor_inserts-before.anchor_inserts
                << ",\"proof_bytes_saved\":" << current.proof_bytes_saved-before.proof_bytes_saved;
#endif
      std::cout << "}\n";
    }
  }

  void mount_scheme(bool pinyin) {
    const auto shared_activation = shared.activation_handle();
    scheme.deactivate();
    order_.reset();
    readers_[1].reset();
    readers_[2].reset();
    scheme_name_ = pinyin ? "full_pinyin" : "xiaohe";
    begin("scheme_activation");
    open_reader(1, resources_ / (scheme_name_ + ".hnm"), release_ / (scheme_name_ + ".hnv"),
                pinyin ? hanime_release::full_pinyin : hanime_release::xiaohe,
                {16, 8, 1024});
    std::string error;
    require(scheme.open_verified(readers_[1], shared, {2, 1024}, &error), error);
    finish();
    begin("order_activation");
    open_reader(2, release_ / (scheme_name_ + "_base.hbo"), release_ / (scheme_name_ + "_base.hnv"),
                pinyin ? hanime_release::full_pinyin_base : hanime_release::xiaohe_base,
                {8, 8, 1024});
    order_ = std::make_shared<BaseOrderIndex>(readers_[2], scheme, shared);
    require(scheme.attach_base_order(order_), "attach BaseOrder failed");
    require(shared.activation_is_current(shared_activation), "scheme switch restarted shared activation");
    finish();
  }

  Status drain(Composition& composition, Status status) {
    fixed_command_work_ += composition.command_work();
    if (status != Status::Working) return status;
    const auto operation = composition.operation();
    for (std::size_t guard = 0; guard < 1000000; ++guard) {
      const auto start = Clock::now();
      const auto step = composition.tick(operation, budget);
      add(step.core_cost, step.local_work, start);
      if (step.status != Status::Working) return step.status;
    }
    throw std::runtime_error("Composition did not complete within bounded diagnostic guard");
  }

  Status query(Composition& composition, std::string_view raw, bool longest,
               const std::string& stage) {
    begin(stage);
    require(composition.edit_raw(0, composition.snapshot().raw.size(), raw) == Status::Ready,
            "raw edit failed");
    fixed_command_work_ += composition.command_work();
    const auto command = longest ? composition.query_longest() : composition.choose_prefix(raw.size());
    const auto status = drain(composition, command);
    std::cout << "STATE {\"phase\":" << std::quoted(trace_.phase)
              << ",\"status\":" << std::quoted(name(status))
              << ",\"command_work\":" << composition.command_work() << "}\n";
    finish();
    return status;
  }

  PageValue capture(Composition& composition, const std::string& stage) {
    PageValue value;
    const auto page = composition.page();
    require(page.size <= page_capacity, "unbounded visible page");
    value.size = page.size;
    value.begin = page.begin;
    value.end = page.end;
    for (std::size_t i = 0; i < page.size; ++i) value.items[i] = page.items[i];
    begin(stage);
    for (std::size_t i = 0; i < page.size; ++i) {
      const auto status = drain(composition, composition.read_text(composition.selection(i)));
      require(status == Status::Ready, "public candidate read_text failed");
      const auto text = composition.last_text();
      require(text.size() <= value.texts[i].size(), "visible word exceeds declared capacity");
      std::copy(text.begin(), text.end(), value.texts[i].begin());
      value.lengths[i] = text.size();
      const auto& item = value.items[i];
      std::cout << "ITEM {\"phase\":" << std::quoted(trace_.phase)
                << ",\"slot\":" << i << ",\"begin\":" << value.begin << ",\"end\":" << value.end
                << ",\"key\":" << std::quoted(key_hex(item.key)) << ",\"ref\":" << item.ref
                << ",\"score\":" << item.score << ",\"paths\":" << item.path_count
                << ",\"scheme_weight\":" << item.scheme_weight
                << ",\"representative_ordinal\":" << item.representative_ordinal
                << ",\"text\":" << std::quoted(std::string(text)) << "}\n";
    }
    require(composition.count_begin_calls() == 0, "Disabled ranking accessed count source");
    finish();
    return value;
  }

  void select_slot(Composition& composition, std::size_t slot, std::string_view expected,
                   const std::string& stage) {
    begin(stage);
    require(drain(composition, composition.select(composition.selection(slot))) == Status::Ready,
            "public select failed");
    const auto snapshot = composition.snapshot();
    require(snapshot.member_count != 0, "selection did not capture member");
    const auto& member = snapshot.members[snapshot.member_count - 1];
    const auto text = snapshot.selected_text.substr(member.text_begin, member.text_size);
    require(text == expected, "fixed public slot did not select expected word");
    std::cout << "MEMBER {\"phase\":" << std::quoted(trace_.phase)
              << ",\"key\":" << std::quoted(key_hex(member.key)) << ",\"ref\":" << member.ref
              << ",\"begin\":" << member.begin << ",\"end\":" << member.end
              << ",\"text\":" << std::quoted(std::string(text)) << "}\n";
    finish();
  }

  void commit(Composition& composition, std::string_view expected, const std::string& label) {
    Document document;
    Submission submission(composition, shared, document);
    require(submission.valid(), "public Submission invalid");
    begin(label + "_prepare");
    auto prepared = submission.prepare();
    for (std::size_t guard = 0; prepared.core.status == CompositionStatus::Preparing; ++guard) {
      require(guard < 1000000, "Submission timeout");
      const auto start = Clock::now();
      const auto step = submission.advance(prepared.transaction, budget);
      add(step.cost, 0, start);
      prepared = step.result;
    }
    require(prepared.core.status == CompositionStatus::Ready && document.calls == 0 && document.size == 0,
            "prepare failed or modified final document");
    finish();
    const auto memory=submission.memory_usage();
    const auto& cm=memory.core;
    std::cout << "SUBMISSION_MEMORY {\"phase\":" << std::quoted(trace_.phase)
              << ",\"owner\":" << memory.owner_bytes << ",\"impl\":" << memory.impl_bytes
              << ",\"capture\":" << memory.capture_array_bytes << ",\"core_impl\":" << cm.impl_bytes
              << ",\"members\":" << cm.members_bytes << ",\"request_names\":" << cm.request_names_bytes
              << ",\"resolver\":" << cm.resolver_name_bytes << ",\"member_text\":" << cm.member_text_bytes
              << ",\"total_text\":" << cm.total_text_bytes << ",\"owner_token\":" << cm.owner_token_bytes
              << ",\"receipt\":" << cm.receipt_bytes << ",\"embedded_reader\":" << cm.output_reader_bytes
              << ",\"control_blocks_included\":false,\"composition_still_alive\":true}\n";
    begin(label + "_confirm");
    const auto accepted = submission.confirm(prepared.transaction);
    require(accepted.core.status == CompositionStatus::Accepted && document.calls == 1 &&
            document.text() == expected, "final document did not accept expected complete text once");
    submission.confirm(prepared.transaction);
    require(document.calls == 1 && composition.count_begin_calls() == 0, "duplicate submit or learning access");
    std::cout << "DOCUMENT {\"phase\":" << std::quoted(trace_.phase)
              << ",\"text\":" << std::quoted(std::string(document.text()))
              << ",\"sink_calls\":" << document.calls << ",\"learning\":\"Disabled\"}\n";
    finish();
  }

  void memory(const Composition& composition) const {
    const auto cm = composition.memory_usage();
    const auto sm = shared.memory_usage();
    std::cout << "MEMORY {\"case\":" << std::quoted(test_name_)
              << ",\"sizeof_runtime\":" << sizeof(Runtime)
              << ",\"sizeof_page_value\":" << sizeof(PageValue)
              << ",\"sizeof_document\":" << sizeof(Document)
              << ",\"extension_owner\":" << cm.owner_bytes
              << ",\"extension_impl\":" << cm.impl_bytes
              << ",\"extension_composition_buffers\":" << cm.composition_buffers
              << ",\"extension_rank_arrays\":" << cm.rank_arrays
              << ",\"extension_page\":" << cm.page_array
              << ",\"extension_output\":" << cm.output_buffers
              << ",\"shared_fixed\":" << sm.owner_fixed_bytes
              << ",\"shared_page_payload\":" << sm.cache_payload_allocated_bytes
              << ",\"shared_page_metadata\":" << sm.cache_metadata_allocated_bytes
              << ",\"full_core_peak\":false}\n";
#if defined(HANIME_OUTPUT_REUSE_API)
    const auto reuse=shared.output_reuse_stats();
    std::cout << "OUTPUT_MEMORY {\"case\":" << std::quoted(test_name_)
              << ",\"fixed\":" << reuse.fixed_bytes << ",\"metadata\":" << reuse.metadata_bytes
              << ",\"text\":" << reuse.text_bytes << ",\"forms\":" << reuse.form_bytes
              << ",\"entries\":" << reuse.entries << ",\"reader_sizeof\":" << sizeof(OutputReader) << "}\n";
#endif
    for (std::size_t i = 0; i < readers_.size(); ++i) {
      const auto m = readers_[i]->memory_usage();
      std::cout << "CACHE_MEMORY {\"case\":" << std::quoted(test_name_)
                << ",\"role\":" << std::quoted(role_names[2 * i])
                << ",\"fixed\":" << m.owner_fixed_bytes
                << ",\"data_payload\":" << m.data_payload_bytes
                << ",\"proof_payload\":" << m.index_payload_bytes
                << ",\"data_metadata\":" << m.data_metadata_bytes
                << ",\"proof_metadata\":" << m.index_metadata_bytes;
#if defined(HANIME_OUTPUT_REUSE_API)
      std::cout << ",\"anchor_bytes\":" << m.anchor_bytes;
#endif
      std::cout << "}\n";
    }
  }

  PagedSharedResource shared;
  PagedSchemeIndex scheme;

 private:
  void open_reader(std::size_t slot, const fs::path& data, const fs::path& proof,
                   const ResourceReleaseSpec& pin, VerifiedResourceConfig config) {
    std::shared_ptr<File> raw;
#if defined(HANIME_VERIFICATION_FORMAT_MAX) && HANIME_VERIFICATION_FORMAT_MAX >= 2
    if (pin.verification_format == 1) raw = std::make_shared<File>(data, trace_, 2 * slot);
#else
    raw = std::make_shared<File>(data, trace_, 2 * slot);
#endif
    auto index = std::make_shared<File>(proof, trace_, 2 * slot + 1);
    readers_[slot] = std::make_shared<VerifiedResourceReader>(raw, index, pin, config);
    require(readers_[slot]->prepare_bootstrap(), "bootstrap rejected");
    for (std::size_t guard = 0; !readers_[slot]->bootstrap_ready(); ++guard) {
      require(guard < 100000, "bootstrap timeout");
      const auto start = Clock::now();
      const auto step = readers_[slot]->advance_bootstrap(budget);
      add(step.cost, 0, start);
      require(step.status != ResourceReadStatus::Error, readers_[slot]->error());
    }
  }
  fs::path resources_, release_;
  Trace& trace_;
  std::string test_name_, scheme_name_{"none"};
  std::array<std::shared_ptr<VerifiedResourceReader>, 3> readers_{};
  std::array<VerifiedResourceSnapshot, 3> snapshots_{};
  std::shared_ptr<BaseOrderIndex> order_;
  std::uint64_t steps_{0}, work_{0}, local_work_{0}, fixed_command_work_{0};
  std::uint64_t logical_calls_{0}, logical_bytes_{0}, max_tick_ns_{0};
};

void cold_ui(const fs::path& resources, const fs::path& release, Trace& trace) {
  Runtime runtime(resources, release, trace, "cold_ui");
  Composition composition(runtime.scheme, runtime.shared, SchemeRules::R1Xiaohe);
  require(composition.valid(), "Basic Host 0.6 invalid");
  require(runtime.query(composition, "ui", false, "rank") == Status::PageReady, "ui rank failed");
  const auto first = runtime.capture(composition, "visible_text");
  require(first.size == 6, "ui first page must have six items");
  runtime.select_slot(composition, 0, "是", "select_is");
  runtime.commit(composition, "是", "single_submit");
  runtime.memory(composition);
}

void segments(const fs::path& resources, const fs::path& release, Trace& trace, bool pinyin) {
  Runtime runtime(resources, release, trace, pinyin ? "segments_py" : "segments_xh", pinyin);
  Composition composition(runtime.scheme, runtime.shared,
                          pinyin ? SchemeRules::FullPinyin : SchemeRules::R1Xiaohe);
  const std::string raw = pinyin ? "wandanle" : "wjdjle";
  require(runtime.query(composition, raw, true, "first_rank") == Status::PageReady, "longest query failed");
  const auto page = runtime.capture(composition, "first_visible_text");
  require(page.size > 0 && std::string_view(page.texts[0].data(), page.lengths[0]) == "完蛋",
          "first public candidate is not 完蛋");
  runtime.select_slot(composition, 0, "完蛋", "select_wandan");
  runtime.begin("remaining_le_rank");
  require(runtime.drain(composition, composition.choose_prefix(raw.size())) == Status::PageReady,
          "remaining le query failed");
  runtime.finish();
  const auto remaining = runtime.capture(composition, "remaining_le_visible_text");
  require(remaining.begin == (pinyin ? 6U : 4U) && remaining.end == raw.size(), "segment interval changed");
  runtime.select_slot(composition, 0, "了", "select_le");
  runtime.begin("member_revalidation");
  require(runtime.drain(composition, composition.revalidate()) == Status::Ready, "member revalidation failed");
  runtime.finish();
  runtime.commit(composition, "完蛋了", "whole_submit");
  runtime.memory(composition);
}

void paging(const fs::path& resources, const fs::path& release, Trace& trace, bool pinyin) {
  Runtime runtime(resources, release, trace, pinyin ? "paging_py" : "paging_xh", pinyin);
  Composition composition(runtime.scheme, runtime.shared,
                          pinyin ? SchemeRules::FullPinyin : SchemeRules::R1Xiaohe);
  require(runtime.query(composition, pinyin ? "shi" : "ui", false, "first_rank") == Status::PageReady,
          "paging first query failed");
  const auto first = runtime.capture(composition, "forward_0_text");
  std::size_t pages = 1, candidates = first.size;
  while (composition.page().has_more) {
    require(pages < 1000, "paging guard exceeded");
    runtime.begin("forward_" + std::to_string(pages) + "_rank");
    require(runtime.drain(composition, composition.request_page(RangeDirection::After)) == Status::PageReady,
            "After failed");
    runtime.finish();
    candidates += runtime.capture(composition, "forward_" + std::to_string(pages) + "_text").size;
    ++pages;
  }
  PageValue rebuilt;
  for (std::size_t page = pages - 1; page > 0; --page) {
    runtime.begin("backward_" + std::to_string(page - 1) + "_rank");
    require(runtime.drain(composition, composition.request_page(RangeDirection::Before)) == Status::PageReady,
            "Before failed");
    runtime.finish();
    rebuilt = runtime.capture(composition, "backward_" + std::to_string(page - 1) + "_text");
  }
  require(pages > 4 && same_page(first, rebuilt), "eviction/reverse access did not rebuild exact first range");
  std::cout << "PAGING {\"scheme\":" << std::quoted(pinyin ? "full_pinyin" : "xiaohe")
            << ",\"pages\":" << pages << ",\"candidates\":" << candidates
            << ",\"retained_reference_pages\":1,\"current_window_bound\":6}\n";
}

void other_inputs(const fs::path& resources, const fs::path& release, Trace& trace) {
  Runtime runtime(resources, release, trace, "samples");
  {
    Composition composition(runtime.scheme, runtime.shared, SchemeRules::R1Xiaohe);
    for (const auto code : {"yi", "wj", "dj", "le", "aakesdhasakezuzivixm", "notacode"}) {
      const auto status = runtime.query(composition, code, false, std::string(code) + "_rank");
      if (std::string_view(code) == "notacode") {
        require(status == Status::NoMappingForPrefix, "expected no-mapping state");
      } else {
        require(status == Status::PageReady, "sample exact code missing");
        runtime.capture(composition, std::string(code) + "_text");
      }
    }
    require(runtime.query(composition, "ui", false, "cancel_setup_rank") == Status::PageReady,
            "cancel setup query");
    runtime.begin("cancel_visible_text_and_replace");
    require(composition.read_text(composition.selection(0)) == Status::Working, "cancel text setup");
    const auto old = composition.operation();
    auto start = Clock::now();
    const auto one = composition.tick(old, budget);
    runtime.add(one.core_cost, one.local_work, start);
    require(composition.cancel(old) == Status::Cancelled, "cancel text failed");
    require(composition.edit_raw(0, composition.snapshot().raw.size(), "") == Status::Ready, "delete empty");
    require(composition.edit_raw(0, 0, "le") == Status::Ready, "new input");
    require(composition.choose_prefix(2) == Status::Working, "new query");
    start = Clock::now();
    const auto stale = composition.tick(old, budget);
    runtime.add(stale.core_cost, stale.local_work, start);
    require(stale.status == Status::Stale, "old operation affected new input");
    require(runtime.drain(composition, Status::Working) == Status::PageReady, "new query after cancel");
    runtime.finish();
    runtime.capture(composition, "after_cancel_le_text");
  }
  runtime.mount_scheme(true);
  {
    Composition composition(runtime.scheme, runtime.shared, SchemeRules::FullPinyin);
    for (const auto code : {"shi", "ren", "qiandongnanmiaozudongzuzizhizhou"}) {
      const auto status = runtime.query(composition, code, false, std::string(code) + "_rank");
      require(status == Status::PageReady, "pinyin sample missing");
      runtime.capture(composition, std::string(code) + "_text");
    }
  }
}
// Additional sequence, separate from the frozen 355-phase comparison.
void output_revisits(const fs::path& resources,const fs::path& release,Trace& trace,bool pinyin) {
  Runtime runtime(resources,release,trace,pinyin ? "reuse_py" : "reuse_xh",pinyin);
  const auto code=pinyin ? "shi" : "ui";
  PageValue first;
  {
    Composition c(runtime.scheme,runtime.shared,pinyin ? SchemeRules::FullPinyin : SchemeRules::R1Xiaohe);
    require(runtime.query(c,code,false,"cold_rank")==Status::PageReady,"reuse cold rank");
    first=runtime.capture(c,"cold_text");
    const auto old=c.selection(0);
    require(runtime.query(c,code,false,"new_input_rank")==Status::PageReady,"reuse new input rank");
    require(c.select(old)==Status::Stale,"cache resurrected old public Selection");
    require(same_page(first,runtime.capture(c,"new_input_hot_text")),"new input changed visible identities");
    require(same_page(first,runtime.capture(c,"adjacent_hot_text")),"adjacent revisit changed outputs");
    for (unsigned page=1;page<=4;++page) {
      runtime.begin("evict_"+std::to_string(page)+"_rank");
      require(runtime.drain(c,c.request_page(RangeDirection::After))==Status::PageReady,"eviction range");runtime.finish();
      runtime.capture(c,"evict_"+std::to_string(page)+"_text");
    }
    require(runtime.query(c,code,false,"evicted_return_rank")==Status::PageReady,"evicted return");
    require(same_page(first,runtime.capture(c,"evicted_return_text")),"eviction lost candidates");
    runtime.begin("generic_missing_form");
    require(runtime.drain(c,c.read_text(c.selection(0),{"test:not-present"}))==Status::Ready,"generic fallback");
    require(c.last_text()==std::string_view(first.texts[0].data(),first.lengths[0]),"fallback text");runtime.finish();
    runtime.capture(c,"default_after_form_text");
    runtime.memory(c);
  }
  runtime.mount_scheme(!pinyin);
  {
    Composition c(runtime.scheme,runtime.shared,pinyin ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin);
    require(runtime.query(c,pinyin ? "ui" : "shi",false,"cross_scheme_rank")==Status::PageReady,"reuse cross scheme");
    runtime.capture(c,"cross_scheme_text");
    runtime.memory(c);
  }
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 4 || argc > 6) {
      std::cerr << "usage: visible_text_consumer RESOURCES RELEASE READ_TRACE.jsonl [hotspots|all|reuse] [off|output|anchors|balanced|extra]\n";
      return 2;
    }
    const bool all = argc < 5 || std::string_view(argv[4]) == "all";
    if (argc==6) perf_profile=argv[5];
    require(perf_profile=="off" || reuse_output() || reuse_anchors(),"unknown profile");
    Trace trace(argv[3]);
    std::cout << "CONFIG {\"basic_host\":\"0.6\",\"profile\":" << std::quoted(perf_profile)
              << ",\"shared_data_blocks\":" << (perf_profile=="balanced" ? 15 : 16) << ",\"scheme_data_blocks\":16,"
              << "\"proof_pages_each\":8,\"order_data_blocks\":8,\"config_unit_bytes\":1024,"
              << "\"authentication_payload_limit_bytes\":65536,\"shared_pages\":" << (perf_profile=="balanced" ? 1 : 2) << ",\"shared_page_bytes\":4096,"
              << "\"shared_max_read_bytes\":1024,\"page_items\":6,\"fourth_range_prefetch\":false,"
              << "\"learning\":\"Disabled\",\"read_port\":\"POSIX_read_lseek\",\"device_measurement\":false}\n";
    if (argc>=5 && std::string_view(argv[4])=="reuse") {
      output_revisits(argv[1],argv[2],trace,false);output_revisits(argv[1],argv[2],trace,true);
    } else {
    cold_ui(argv[1], argv[2], trace);
    segments(argv[1], argv[2], trace, false);
    if (all) {
      segments(argv[1], argv[2], trace, true);
      paging(argv[1], argv[2], trace, false);
      paging(argv[1], argv[2], trace, true);
      other_inputs(argv[1], argv[2], trace);
    }
    }
    require(static_cast<bool>(trace.file), "trace write failed");
    std::cout << "PASS public Basic Host 0.6 visible-text chain; DEVICE=OPEN; learning_files=0\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
