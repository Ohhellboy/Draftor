#include "hanime/resource_publisher.hpp"
#include "hanime/bounded_output.hpp"
#include "hanime/host_resource_file.hpp"
#include "base_order_build.hpp"
#include "base_order_paged_build.hpp"
#include "packed_auth_build.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace hanime {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t kHostReadBytes = 65536;
void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
void add(WorkCost& a, const WorkCost& b) {
  require(b.read_calls <= std::numeric_limits<std::size_t>::max()-a.read_calls &&
          b.read_bytes <= std::numeric_limits<std::size_t>::max()-a.read_bytes &&
          b.work_items <= std::numeric_limits<std::size_t>::max()-a.work_items,
          "publication accounting overflow");
  a.read_calls+=b.read_calls; a.read_bytes+=b.read_bytes; a.work_items+=b.work_items;
}
void account(PublicationPhaseCost& phase, const WorkCost& cost, const WorkBudget& budget) {
  require(cost.read_calls<=budget.max_read_calls && cost.read_bytes<=budget.max_read_bytes &&
          cost.work_items<=budget.max_work_items, "complete validator exceeded budget");
  ++phase.steps; add(phase.cost,cost);
}
std::string hex(const Sha256Digest& value) {
  std::ostringstream out; out<<std::hex<<std::setfill('0');
  for (const auto byte:value) out<<std::setw(2)<<unsigned(byte);
  return out.str();
}
std::string quoted(const std::string& value) {
  std::ostringstream out; out<<'"';
  for (const unsigned char c:value) {
    if (c=='"' || c=='\\') out<<'\\'<<char(c);
    else if (c<32) out<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c)<<std::dec;
    else out<<char(c);
  }
  out<<'"'; return out.str();
}
bool valid_scheme_id(const std::string& name) {
  if (name.empty() || name.size()>=32 || name=="shared") return false;
  if (name.front()<'a' || name.front()>'z') return false;
  const std::string_view keywords=" alignas alignof and and_eq asm auto bitand bitor bool break case catch char "
      "char16_t char32_t class compl const constexpr const_cast continue decltype default delete do double "
      "dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable "
      "namespace new noexcept not not_eq nullptr operator or or_eq private protected public register "
      "reinterpret_cast return short signed sizeof static static_assert static_cast struct switch template "
      "this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile "
      "wchar_t while xor xor_eq concept requires co_await co_return co_yield module import ";
  if(keywords.find(" "+name+" ")!=std::string_view::npos)return false;
  return std::all_of(name.begin(),name.end(),[](char c) {
    return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_';
  });
}
Sha256Digest hash_file(const ResourceReader& file, PublicationPhaseCost& cost) {
  std::array<std::uint8_t,kHostReadBytes> buffer{}; Sha256 hash;
  for (std::size_t offset=0; offset<file.size();) {
    const auto n=std::min(buffer.size(),file.size()-offset);
    require(file.read(offset,buffer.data(),n),"resource SHA read failed");
    hash.update(buffer.data(),n); offset+=n; ++cost.steps;
    add(cost.cost,{1,n,n});
  }
  return hash.final();
}
void write_bytes(const fs::path& path, const void* data, std::size_t size) {
  require(size<=static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()),"output too large");
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  require(out.good(),"cannot create "+path.string());
  if (size) out.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));
  out.flush(); require(out.good(),"cannot write "+path.string());
  out.close(); require(!out.fail(),"cannot close "+path.string());
}
void write_text(const fs::path& path, const std::string& text) {
  write_bytes(path,text.data(),text.size());
}
class MemorySource final : public ResourceReader {
 public:
  explicit MemorySource(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
  std::size_t size() const override { return bytes_.size(); }
  bool read(std::size_t offset, void* output, std::size_t count) const override {
    if (!output || offset > bytes_.size() || count > bytes_.size() - offset) return false;
    std::copy_n(bytes_.data() + offset, count, static_cast<std::uint8_t*>(output));
    return true;
  }
 private:
  const std::vector<std::uint8_t>& bytes_;
};
std::vector<std::uint8_t> build_index(const ResourceReader& file, PublishedResource& output) {
  MerkleLayout layout;
  require(merkle_layout(file.size(),&layout),"resource is outside Merkle format limits");
  require(layout.index_bytes<=std::numeric_limits<std::size_t>::max(),"index exceeds Host address space");
  std::vector<std::uint8_t> index(static_cast<std::size_t>(layout.index_bytes),0);
  std::array<std::uint8_t,kVerifiedBlockBytes> data{}; Sha256 file_hash;
  std::size_t offset=0;
  for (std::uint64_t block=0; block<layout.data_blocks; ++block) {
    const auto n=std::min(data.size(),file.size()-offset);
    require(file.read(offset,data.data(),n),"resource read during index generation failed");
    file_hash.update(data.data(),n);
    const auto digest=merkle_leaf_hash(block,data.data(),n);
    const auto destination=static_cast<std::size_t>(layout.offsets[0]+block*digest.size());
    std::memcpy(index.data()+destination,digest.data(),digest.size());
    // Hash-input bytes, including the leaf domain prefix. This is NOT a CPU
    // instruction count and does not include copying/index stores or SHA padding.
    ++output.index_generation.steps; add(output.index_generation.cost,{1,n,2*n+13}); offset+=n;
  }
  require(file_hash.final()==output.spec.file_sha256,"resource changed during validation/index generation");
  for (std::uint32_t level=1; level<layout.levels; ++level) {
    for (std::uint32_t page=0; page<layout.page_counts[level-1]; ++page) {
      const auto child=static_cast<std::size_t>(layout.offsets[level-1]+std::uint64_t(page)*kVerifiedIndexPageBytes);
      const auto digest=merkle_page_hash(level-1,page,index.data()+child);
      const auto destination=static_cast<std::size_t>(layout.offsets[level]+std::uint64_t(page)*digest.size());
      std::memcpy(index.data()+destination,digest.data(),digest.size());
      ++output.index_generation.steps; add(output.index_generation.cost,{0,0,kVerifiedIndexPageBytes+9});
    }
  }
  const auto top=layout.levels-1;
  output.spec.merkle_root=merkle_page_hash(top,0,index.data()+static_cast<std::size_t>(layout.offsets[top]));
  ++output.index_generation.steps; add(output.index_generation.cost,{0,0,kVerifiedIndexPageBytes+9});
  output.index_bytes=index.size(); output.index_sha256=sha256(index.data(),index.size());
  ++output.index_generation.steps; add(output.index_generation.cost,{0,0,index.size()});
  return index;
}
std::string phase_json(const PublicationPhaseCost& c) {
  std::ostringstream out;
  out<<"{\"steps\":"<<c.steps<<",\"read_calls\":"<<c.cost.read_calls
     <<",\"read_bytes\":"<<c.cost.read_bytes<<",\"work_items\":"<<c.cost.work_items<<'}';
  return out.str();
}
std::string manifest(const PublishVerifiedResult& result, const WorkBudget& budget) {
  std::ostringstream out;
  out<<"{\n  \"publication_format\":1,\n  \"status\":\"HOST_FINAL_BYTES_VALIDATED\",\n"
     <<"  \"trust\":\"Pin the generated release_pin.hpp values in trusted firmware. This JSON is not a trust source. Resource updates require a new trusted pin/firmware.\",\n"
     <<"  \"resource_inputs_immutable_during_publication\":true,\n"
     <<"  \"transport_fields_scope\":\"Top-level format/block/fanout describe shared only. Each files[].verification_format is authoritative for that file; mixed publication is supported.\",\n"
     <<"  \"verification_index_format\":"<<result.resources.front().spec.verification_format<<",\n  \"verification_rule_version\":"<<kResourceVerificationRule
     <<",\n  \"data_block_bytes\":"<<(result.resources.front().spec.verification_format==2 ? kPackedDataBytes : kVerifiedBlockBytes)
     <<",\n  \"index_page_bytes\":"<<(result.resources.front().spec.verification_format==2 ? 0 : kVerifiedIndexPageBytes)
     <<",\n  \"index_fanout\":"<<(result.resources.front().spec.verification_format==2 ? 2 : kVerifiedFanout)<<",\n  \"validation_budget\":{\"reads\":"<<budget.max_read_calls
     <<",\"bytes\":"<<budget.max_read_bytes<<",\"work\":"<<budget.max_work_items<<"},\n"
     <<"  \"host_largest_temporary_index_vector_bytes\":"<<result.host_index_peak_bytes
     <<",\n  \"host_hash_read_scratch_bytes\":"<<kHostReadBytes
     <<",\n  \"host_packed_tree_peak_bytes\":"<<result.host_packed_tree_peak_bytes
     <<",\n  \"host_index_data_scratch_bytes\":"<<kVerifiedBlockBytes
     <<",\n  \"host_memory_scope\":\"Listed buffers only; validator/cache/file owner, paths, streams, hash state, allocator and remaining stack are additional. Not total Host or device peak.\""
     <<",\n  \"host_hash_phase_work_unit\":\"SHA-256 input bytes including explicit Merkle prefixes, excluding SHA padding and non-hash work; not comparable to validator record work or device CPU instructions.\""
     <<",\n  \"host_paged_live_vector_bytes\":"<<result.host_paged_live_vector_bytes
     <<",\n  \"files\":[\n";
  bool first=true;
  for (const auto& r:result.resources) {
    if (!first) out<<",\n";
    first=false;
    const auto& s=r.spec;
    out<<"    {\"logical_name\":"<<quoted(r.logical_name)<<",\"source_file\":"<<quoted(r.source_file)
       <<",\"verification_format\":"<<s.verification_format
       <<",\"index_file\":"<<quoted(r.index_file)<<",\"role\":"<<quoted(s.role==ResourceReleaseRole::Shared?"shared":s.role==ResourceReleaseRole::Scheme?"scheme":"base_order")
       <<",\"resource_format\":"<<s.resource_format<<",\"key_format\":"<<s.key_format
       <<",\"namespace\":"<<s.registry_namespace<<",\"shared_bundle_id\":"<<s.shared_bundle_id
       <<",\"runtime_ref_count\":"<<s.runtime_ref_count<<",\"file_bytes\":"<<s.file_bytes
       <<",\"file_sha256\":"<<quoted(hex(s.file_sha256))<<",\"index_bytes\":"<<r.index_bytes
       <<",\"index_sha256\":"<<quoted(hex(r.index_sha256))<<",\"merkle_root\":"<<quoted(hex(s.merkle_root))
       <<",\"required_shared_sha256\":"<<quoted(hex(s.required_shared_sha256))
       <<",\"scheme_id\":"<<quoted(s.scheme_id.data())<<",\"text_capacity\":"<<s.text_capacity
       <<",\"name_capacity\":"<<s.name_capacity<<",\"bootstrap_root_offset\":"<<s.bootstrap_root_offset
       << ",\"derived_source_bytes\":" << r.derived_source_bytes
       << ",\"derived_source_sha256\":" << quoted(hex(r.derived_source_sha256))
       << ",\"directory_and_tail_equivalent\":" << (r.directory_and_tail_equivalent ? "true" : "false")
       <<",\"bootstrap_root_bytes\":"<<s.bootstrap_root_bytes<<",\"costs\":{\"hash_before\":"<<phase_json(r.hash_before)
       <<",\"fixed_open\":"<<phase_json(r.fixed_open)<<",\"full_semantics\":"<<phase_json(r.semantic_validation)
       <<",\"text_admission\":"<<phase_json(r.text_admission)<<",\"index_generation\":"<<phase_json(r.index_generation)
       <<",\"hash_after\":"<<phase_json(r.hash_after)
       <<",\"final_transport_validation\":"<<phase_json(r.final_transport_validation)<<"}";
    if (s.verification_format==2) {
      PackedMerkleLayout layout;
      require(packed_merkle_layout(s.file_bytes,&layout),"invalid packed manifest layout");
      out<<",\"logical_block_bytes\":"<<kPackedDataBytes<<",\"packet_bytes\":"<<layout.packet_bytes
         <<",\"binary_proof_depth\":"<<layout.depth
         <<",\"transport\":\"data and binary authentication path; source file not needed at runtime\"";
    }
    if (s.role==ResourceReleaseRole::BaseOrder)
      out<<",\"required_scheme_sha256\":"<<quoted(hex(s.required_scheme_sha256))<<",\"ranking_rule_version\":"<<s.ranking_rule_version
         <<",\"derived_build\":"<<phase_json(r.derived_build)
         <<",\"derived_host_work_accounting\":\"build/check are synchronous Host map/sort passes; work_items=0 means uninstrumented, not zero CPU; read bytes measured across shared and scheme, plus final-file checker\"";
    out<<"}";
  }
  out<<"\n  ]\n}\n"; return out.str();
}
std::string digest_initializer(const Sha256Digest& value) {
  std::ostringstream out; out<<"{{"; bool first=true;
  for (const auto byte:value) {if (!first) out<<','; first=false; out<<unsigned(byte);}
  out<<"}}"; return out.str();
}
std::string pin_header(const PublishVerifiedResult& result) {
  std::ostringstream out;
  out<<"#pragma once\n#include \"hanime/verified_resource.hpp\"\n"
     <<"// Generated AFTER complete semantic validation of the final bytes.\n"
     <<"// Trust these values only through the explicit firmware release process.\n"
     <<"// Replacing a resource needs a new pin/firmware; TF JSON is not trusted.\n"
     <<"namespace hanime_release {\n";
  for (const auto& r:result.resources) {
    const auto& s=r.spec;
    out<<"inline const hanime::ResourceReleaseSpec "<<r.logical_name<<" = [] {\n"
       <<"  hanime::ResourceReleaseSpec s;\n  s.rule_version="<<s.rule_version<<"; s.resource_format="<<s.resource_format
       <<"; s.key_format="<<s.key_format<<";\n  s.role=hanime::ResourceReleaseRole::"
       <<(s.role==ResourceReleaseRole::Shared?"Shared":s.role==ResourceReleaseRole::Scheme?"Scheme":"BaseOrder")<<";\n  s.registry_namespace="<<s.registry_namespace
       <<"ULL; s.shared_bundle_id="<<s.shared_bundle_id<<"ULL;\n  s.file_bytes="<<s.file_bytes
       <<"ULL; s.file_sha256="<<digest_initializer(s.file_sha256)<<";\n  s.merkle_root="<<digest_initializer(s.merkle_root)
       <<";\n  s.required_shared_sha256="<<digest_initializer(s.required_shared_sha256)<<";\n  s.runtime_ref_count="<<s.runtime_ref_count
       <<"; s.text_capacity="<<s.text_capacity<<"; s.name_capacity="<<s.name_capacity<<";\n";
    if (s.verification_format!=1) out<<"  s.verification_format="<<s.verification_format<<";\n";
    for (std::size_t i=0; i<s.scheme_id.size() && s.scheme_id[i]; ++i)
      out<<"  s.scheme_id["<<i<<"]="<<unsigned(static_cast<unsigned char>(s.scheme_id[i]))<<";\n";
    if (s.role==ResourceReleaseRole::BaseOrder)
      out<<"  s.required_scheme_sha256="<<digest_initializer(s.required_scheme_sha256)<<"; s.ranking_rule_version="<<s.ranking_rule_version<<";\n";
    out<<"  s.bootstrap_root_offset="<<s.bootstrap_root_offset<<"ULL; s.bootstrap_root_bytes="<<s.bootstrap_root_bytes
       <<";\n  return s;\n}();\n";
  }
  out<<"} // namespace hanime_release\n"; return out.str();
}
// Re-read the actual output with the portable runtime verifier. The full
// logical SHA connects the physical transport to the semantic validation.
void verify_final_transport(const fs::path& path,
                            std::shared_ptr<const ResourceReader> data, PublishedResource& output) {
  HostResourceMetrics metrics;
  auto transport=std::make_shared<HostResourceFile>(path.string(),1024,metrics);
  require(transport->valid(),"cannot reopen packed transport");
  VerifiedResourceReader reader(output.spec.verification_format == 2 ? nullptr : data,
                                transport,output.spec,{8,2,1024});
  require(reader.healthy(),"invalid final packed transport dimensions");
  std::array<std::uint8_t,kPackedDataBytes> bytes{};
  Sha256 hash;
  const WorkBudget budget{64,65536,65536};
  ResourceReadContext context{std::make_shared<const int>(0),1};
  for(std::size_t offset=0;offset<output.spec.file_bytes;) {
    const auto count=std::min(bytes.size(),static_cast<std::size_t>(output.spec.file_bytes)-offset);
    for(;;) {
      const auto step=reader.read_budgeted(offset,bytes.data(),count,budget,context);
      account(output.final_transport_validation,step.cost,budget);
      if(step.status==ResourceReadStatus::Ready)break;
      require(step.status==ResourceReadStatus::Pending,"final packed transport authentication failed");
    }
    hash.update(bytes.data(),count);offset+=count;++context.revision;
  }
  require(hash.final()==output.spec.file_sha256,"final packed logical bytes differ from validated source");
}
} // namespace

PublishVerifiedResult publish_verified_resources(const PublishVerifiedOptions& options) {
  PublishVerifiedResult result;
  std::vector<fs::path> created_files;
  bool created_directory=false;
  const fs::path destination(options.output_directory);
  try {
    require(options.registry_namespace && options.text_capacity && options.name_capacity,
            "namespace and capacities must be nonzero");
    require(!options.shared_path.empty() && !options.output_directory.empty() && !options.schemes.empty(),
            "shared resource, independently chosen scheme(s), and fresh output directory are required");
    require(options.validation_budget.max_read_calls && options.validation_budget.max_read_bytes &&
            options.validation_budget.max_work_items,"invalid validation budget");
    require(!fs::exists(destination),"output directory already exists; accepted publications are immutable");
    require(options.base_order_format == 1 ||
            (options.base_order && options.base_order_format == 2),
            "base-order format must be 1, or 2 with --base-order");
    std::set<std::string> names;
    for (const auto& scheme:options.schemes)
      require(valid_scheme_id(scheme.scheme_id) && names.insert(scheme.scheme_id).second && !scheme.path.empty(),
              "invalid or duplicate scheme input");
    HostResourceMetrics metrics;
    std::vector<std::shared_ptr<HostResourceFile>> files;
    std::vector<std::string> input_paths;
    files.reserve(options.schemes.size()+1);
    input_paths.reserve(options.schemes.size()+1);
    auto open_file=[&](const std::string& path) {
      auto file=std::make_shared<HostResourceFile>(path,kHostReadBytes,metrics);
      require(file->valid(),"cannot open final resource "+path);
      files.push_back(file); input_paths.push_back(path); return file;
    };
    result.resources.reserve(options.schemes.size()+1);
    PublishedResource shared;
    shared.logical_name="shared"; shared.source_file=fs::path(options.shared_path).filename().string();
    shared.index_file="shared.hnv"; shared.spec.text_capacity=options.text_capacity; shared.spec.name_capacity=options.name_capacity;
    const auto shared_file=open_file(options.shared_path);
    shared.spec.file_bytes=shared_file->size(); shared.spec.file_sha256=hash_file(*shared_file,shared.hash_before);
    PagedSharedResource resource; std::string error;
    const bool shared_opened=resource.open(shared_file,options.registry_namespace,options.expected_shared_bundle_id,1,{2,256},&error);
    require(shared_opened,"shared fixed open failed: "+error);
    shared.fixed_open={1,resource.info().open_cost};
    const auto cursor=resource.validation_cursor();
    for (;;) {
      const auto step=resource.advance_validation(cursor,options.validation_budget);
      account(shared.semantic_validation,step.cost,options.validation_budget);
      if (step.status==ValidationStepStatus::Ready) break;
      require(step.status==ValidationStepStatus::InProgress,"shared full validation failed: "+step.error);
    }
    TextAccessCheck text;
    require(text.begin(resource,options.text_capacity,options.name_capacity)==TextAccessStatus::Pending,"text admission cannot begin");
    for (;;) {
      const auto step=text.advance(options.validation_budget);
      account(shared.text_admission,step.cost,options.validation_budget);
      if (step.status==TextAccessStatus::Ready) break;
      require(step.status==TextAccessStatus::Pending,"text admission failed at record "+std::to_string(step.location_index)+
              ", required bytes="+std::to_string(step.required_bytes));
    }
    shared.spec.registry_namespace=resource.info().registry_namespace;
    shared.spec.shared_bundle_id=resource.info().shared_bundle_id;
    shared.spec.key_format=resource.info().key_format;
    shared.spec.runtime_ref_count=resource.info().sections[0].count;
    result.resources.push_back(shared);
    for (const auto& input:options.schemes) {
      PublishedResource scheme;
      scheme.logical_name=input.scheme_id; scheme.source_file=fs::path(input.path).filename().string();
      scheme.index_file=input.scheme_id+".hnv"; scheme.spec=shared.spec; scheme.spec.role=ResourceReleaseRole::Scheme;
      scheme.spec.required_shared_sha256=shared.spec.file_sha256;
      std::copy(input.scheme_id.begin(),input.scheme_id.end(),scheme.spec.scheme_id.begin());
      const auto file=open_file(input.path);
      scheme.spec.file_bytes=file->size(); scheme.spec.file_sha256=hash_file(*file,scheme.hash_before);
      PagedSchemeIndex index; error.clear();
      const bool scheme_opened=index.open(file,shared.spec.registry_namespace,shared.spec.shared_bundle_id,shared.spec.runtime_ref_count,{2,256},&error);
      require(scheme_opened,"scheme fixed open failed: "+error);
      require(index.info().scheme_id==input.scheme_id,"scheme ID does not match requested role");
      scheme.fixed_open={1,{index.info().initial_read_calls,index.info().initial_read_bytes,0}};
      for (;;) {
        const auto step=index.advance_validation(options.validation_budget);
        account(scheme.semantic_validation,step.cost,options.validation_budget);
        if (step.status==ValidationStepStatus::Ready) break;
        require(step.status==ValidationStepStatus::InProgress,"scheme full validation failed: "+step.error);
      }
      scheme.spec.bootstrap_root_offset=index.info().root_offset;
      scheme.spec.bootstrap_root_bytes=index.info().page_bytes;
      require(scheme.spec.bootstrap_root_offset>=40 && scheme.spec.bootstrap_root_offset<scheme.spec.file_bytes &&
              scheme.spec.bootstrap_root_bytes<=scheme.spec.file_bytes-scheme.spec.bootstrap_root_offset,
              "validated scheme did not expose a valid bootstrap root span");
      result.resources.push_back(scheme);
    }
    // No index or trusted output is created before every requested complete
    // validator and shared text admission have succeeded.
    require(fs::create_directory(destination),"cannot create fresh output directory");
    created_directory=true;
    if (options.base_order) {
      const auto primary_count=files.size();
      for (std::size_t i=1;i<primary_count;++i) {
        PublishedResource order;
        order.logical_name=result.resources[i].logical_name+"_base";
        require(names.insert(order.logical_name).second,"derived logical name collides with scheme");
        order.source_file=order.logical_name+".hbo";order.index_file=order.logical_name+".hnv";
        order.spec=result.resources[i].spec;order.spec.role=ResourceReleaseRole::BaseOrder;
        order.spec.resource_format=options.base_order_format;order.spec.ranking_rule_version=1;
        order.spec.required_scheme_sha256=result.resources[i].spec.file_sha256;
        order.spec.bootstrap_root_offset=0;order.spec.bootstrap_root_bytes=0;
        const auto before_build=metrics;
        auto bytes=detail::build_base_order(*files[i],resource);
        std::vector<std::uint8_t> validated_hbo1;
        if (options.base_order_format == 2) {
          const MemorySource original(bytes);
          require(detail::check_base_order(original, *files[i], resource, &error),
                  "HBO1 source semantic validation failed: " + error);
          order.derived_source_bytes = bytes.size();
          order.derived_source_sha256 = sha256(bytes.data(), bytes.size());
          validated_hbo1 = std::move(bytes);
          std::size_t auxiliary_capacity = 0;
          bytes = detail::build_paged_base_order(validated_hbo1, &auxiliary_capacity);
          result.host_paged_live_vector_bytes = std::max(result.host_paged_live_vector_bytes,
              validated_hbo1.capacity() + bytes.capacity() + auxiliary_capacity);
        }
        order.derived_build={1,{static_cast<std::size_t>(metrics.read_calls-before_build.read_calls),
          static_cast<std::size_t>(metrics.requested_bytes-before_build.requested_bytes),0}};
        const auto path=destination/order.source_file;created_files.push_back(path);
        write_bytes(path,bytes.data(),bytes.size());
        const auto final_file=open_file(path.string());
        const auto before_check=metrics;
        const bool valid = options.base_order_format == 2
            ? detail::check_paged_base_order(*final_file, validated_hbo1, &error)
            : detail::check_base_order(*final_file, *files[i], resource, &error);
        require(valid, "base-order final semantic validation failed: " + error);
        order.directory_and_tail_equivalent = options.base_order_format == 2;
        order.semantic_validation={1,{static_cast<std::size_t>(metrics.read_calls-before_check.read_calls),
          static_cast<std::size_t>(metrics.requested_bytes-before_check.requested_bytes),0}};
        order.spec.file_bytes=final_file->size();order.spec.file_sha256=hash_file(*final_file,order.hash_before);
        result.resources.push_back(order);
      }
    }
    for (std::size_t i=0; i<files.size(); ++i) {
      auto& output=result.resources[i];
      const bool packed=options.packed_auth && (options.packed_auth_all || output.spec.role==ResourceReleaseRole::Shared);
      output.spec.verification_format=packed ? 2 : 1;
      const auto index=packed ?
        detail::build_packed_auth(*files[i],output,&result.host_packed_tree_peak_bytes) : build_index(*files[i],output);
      result.host_index_peak_bytes=std::max(result.host_index_peak_bytes,index.capacity());
      // Reopen the pathname as well as rehash: an atomic replacement or an
      // appended file must not pass merely because the old fd still has the
      // previously sampled size/inode. Immutability is still a caller contract.
      HostResourceFile final_input(input_paths[i],kHostReadBytes,metrics);
      require(final_input.valid() && final_input.size()==output.spec.file_bytes,
              "resource size/path changed after index generation");
      require(hash_file(final_input,output.hash_after)==output.spec.file_sha256,"resource changed after index generation");
      const auto path=destination/output.index_file; created_files.push_back(path);
      write_bytes(path,index.data(),index.size());
      verify_final_transport(path, files[i], output);
    }
    const auto manifest_path=destination/"release_manifest.json";
    const auto pin_path=destination/"release_pin.hpp";
    const auto manifest_pending=destination/"release_manifest.json.pending";
    const auto pin_pending=destination/"release_pin.hpp.pending";
    created_files.push_back(manifest_pending); write_text(manifest_pending,manifest(result,options.validation_budget));
    created_files.push_back(pin_pending); write_text(pin_pending,pin_header(result));
    created_files.push_back(manifest_path); fs::rename(manifest_pending,manifest_path);
    created_files.push_back(pin_path); fs::rename(pin_pending,pin_path);
    result.ok=true;
  } catch (const std::exception& error) {
    result.error=error.what(); result.resources.clear();
    // Only our exact files in a directory created by this invocation. No
    // recursive removal and never any pre-existing accepted publication.
    for (auto i=created_files.rbegin(); i!=created_files.rend(); ++i) {
      std::error_code ignored; fs::remove(*i,ignored);
    }
    if (created_directory) {std::error_code ignored; fs::remove(destination,ignored);}
  }
  return result;
}
} // namespace hanime
