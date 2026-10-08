#include "hanime/core.hpp"

#include "paged_scheme_index_internal.hpp"
#include "paged_shared_internal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace hanime {
namespace {

constexpr std::uint32_t kNoString = kNoSharedString;
constexpr std::uint32_t kMaximumRecords = 1'000'000U;

BuildResult build_failure(std::string error) {
  BuildResult result;
  result.error = std::move(error);
  return result;
}

class StringPool {
 public:
  std::uint32_t add(const std::string& value) {
    const auto existing = indexes_.find(value);
    if (existing != indexes_.end()) {
      return existing->second;
    }
    const auto index = static_cast<std::uint32_t>(strings_.size());
    strings_.push_back(value);
    indexes_.emplace(value, index);
    return index;
  }

  const std::vector<std::string>& strings() const { return strings_; }

 private:
  std::vector<std::string> strings_;
  std::unordered_map<std::string, std::uint32_t> indexes_;
};

std::string json_escape(std::string_view input) {
  std::ostringstream output;
  for (const unsigned char value : input) {
    switch (value) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (value < 0x20U) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<unsigned>(value) << std::dec;
        } else {
          output << static_cast<char>(value);
        }
    }
  }
  return output.str();
}

std::string hex_u64(std::uint64_t value) {
  std::ostringstream output;
  output << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
  return output.str();
}

struct StringSpan { std::size_t offset{0}; std::uint32_t length{0}; };
struct ParsedRelation {
  std::uint32_t form_id{kNoString}, text{kNoString}, condition{kNoString}, selection_basis{kNoString};
  bool preferred{false};
  ConditionKind condition_kind{ConditionKind::None};
  std::uint8_t availability{3};
  OutputAdmission admission{OutputAdmission::Ready};
};
struct ParsedLexeme {
  StableLexemeKey stable_key{};
  LexemeKind kind{LexemeKind::Word};
  std::int32_t base_weight{0};
  std::uint32_t default_output{kNoString}, relation_start{0}, relation_count{0};
};
struct ParsedShared {
  RegistryNamespaceId registry_namespace{0}; SharedBundleId shared_bundle_id{0};
  std::vector<StringSpan> strings;
  std::vector<ParsedLexeme> lexemes;
  std::vector<ParsedRelation> relations;
};
bool read_span(const ResourceReader& reader, const StringSpan& span, std::string* output) {
  output->resize(span.length);
  return !span.length || reader.read(span.offset, output->data(), span.length);
}
bool parse_shared_bundle(const ResourceReader& reader, ParsedShared* out, std::string* error) {
  detail::DecodedSharedBundle decoded;
  if (!detail::decode_shared_host(reader, &decoded, error)) return false;
  out->registry_namespace = decoded.info.registry_namespace;
  out->shared_bundle_id = decoded.info.shared_bundle_id;
  for (const auto& s : decoded.descriptors)
    out->strings.push_back({static_cast<std::size_t>(decoded.info.payload_offset + s.offset), s.length});
  for (const auto& l : decoded.tables.lexemes)
    out->lexemes.push_back({l.identity.stable_key, l.identity.kind, l.identity.base_weight,
                           l.default_payload, l.relation_start, l.relation_count});
  for (const auto& r : decoded.tables.relations) {
    const auto& selector = decoded.tables.selectors[r.selector];
    out->relations.push_back({selector.name, r.target_payload, r.condition_descriptor,
                              selector.selection_basis, r.preferred, r.condition_kind,
                              r.availability, r.admission});
  }
  return true;
}

struct SchemeEntry {
  std::string code;
  RuntimeLexemeRef runtime_ref{0};
  std::int32_t scheme_weight{0};
  std::uint32_t mapping_ordinal{0};
};

struct ParsedScheme {
  std::string scheme_id;
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId required_shared_bundle_id{0};
  std::vector<SchemeEntry> entries;
};

bool parse_scheme_bundle(const ResourceReader& reader, ParsedScheme* output,
                         std::string* error) {
  detail::DecodedPagedScheme decoded;
  if (!detail::decode_paged_scheme_bundle(reader, &decoded, error)) {
    return false;
  }
  output->scheme_id = std::move(decoded.scheme_id);
  output->registry_namespace = decoded.registry_namespace;
  output->required_shared_bundle_id = decoded.required_shared_bundle_id;
  output->entries.clear();
  output->entries.reserve(decoded.mappings.size());
  for (detail::PagedSchemeBuildMapping& mapping : decoded.mappings) {
    output->entries.push_back(
        {std::move(mapping.code), mapping.runtime_ref, mapping.scheme_weight,
         mapping.mapping_ordinal});
  }
  return true;
}

}  // namespace

bool is_valid_utf8(std::string_view text) {
  std::size_t index = 0;
  while (index < text.size()) {
    const auto first = static_cast<std::uint8_t>(text[index]);
    std::uint32_t code_point = 0;
    std::size_t length = 0;
    if (first <= 0x7fU) {
      code_point = first;
      length = 1;
    } else if (first >= 0xc2U && first <= 0xdfU) {
      code_point = first & 0x1fU;
      length = 2;
    } else if (first >= 0xe0U && first <= 0xefU) {
      code_point = first & 0x0fU;
      length = 3;
    } else if (first >= 0xf0U && first <= 0xf4U) {
      code_point = first & 0x07U;
      length = 4;
    } else {
      return false;
    }
    if (index + length > text.size()) {
      return false;
    }
    for (std::size_t continuation = 1; continuation < length;
         ++continuation) {
      const auto value = static_cast<std::uint8_t>(text[index + continuation]);
      if ((value & 0xc0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (value & 0x3fU);
    }
    if ((length == 2U && code_point < 0x80U) ||
        (length == 3U && code_point < 0x800U) ||
        (length == 4U && code_point < 0x10000U) ||
        (code_point >= 0xd800U && code_point <= 0xdfffU) ||
        code_point > 0x10ffffU) {
      return false;
    }
    index += length;
  }
  return true;
}

std::string BuildManifest::to_json() const {
  std::ostringstream output;
  output << "{\n"
         << "  \"registry_namespace\": \"" << hex_u64(registry_namespace)
         << "\",\n"
         << "  \"shared_bundle_id\": \"" << hex_u64(shared_bundle_id)
         << "\",\n"
         << "  \"source_name\": \"" << json_escape(source_name) << "\",\n"
         << "  \"source_version\": \"" << json_escape(source_version)
         << "\",\n"
         << "  \"relation_basis\": \"" << json_escape(relation_basis)
         << "\",\n"
         << "  \"approved_lexeme_count\": " << approved_lexeme_count
         << ",\n"
         << "  \"pending_mapping_count\": " << pending_mapping_count
         << ",\n"
         << "  \"string_pool_count\": " << string_pool_count << ",\n"
         << "  \"output_relation_count\": " << output_relation_count
         << ",\n"
         << "  \"shared_key_format\": \"SLK1_256\",\n"
         << "  \"shared_page_bytes\": " << shared_page_bytes << ",\n"
         << "  \"uniqueness_sequence_bytes\": " << uniqueness_sequence_bytes << ",\n"
         << "  \"shared_bundle_bytes\": " << shared_bundle_bytes << ",\n"
         << "  \"identity_trace\": [";
  for (std::size_t index = 0; index < identity_trace.size(); ++index) {
    const IdentityTraceRecord& record = identity_trace[index];
    if (index != 0U) {
      output << ',';
    }
    output << "\n    {\"stable_key\": \"" << format_slk1(record.stable_key)
           << "\", \"kind\": \""
           << (record.kind == LexemeKind::Character ? "character" : "word")
           << "\", \"provenance\": \"" << json_escape(record.provenance)
           << "\", \"output_relation_count\": "
           << record.output_relation_count << '}';
  }
  if (!identity_trace.empty()) {
    output << '\n';
  }
  output << "  ],\n"
         << "  \"pending_mapping_records\": [";
  for (std::size_t index = 0; index < pending_mapping_records.size(); ++index) {
    if (index != 0U) {
      output << ", ";
    }
    output << '"' << json_escape(pending_mapping_records[index]) << '"';
  }
  output << "],\n"
         << "  \"schemes\": {";
  bool first = true;
  for (const auto& [scheme, count] : scheme_mapping_counts) {
    if (!first) {
      output << ',';
    }
    first = false;
    output << "\n    \"" << json_escape(scheme) << "\": {\"mapping_count\": "
           << count << ", \"bundle_bytes\": "
           << scheme_bundle_bytes.at(scheme) << ", \"page_bytes\": "
           << scheme_page_bytes.at(scheme) << ", \"tree_depth\": "
           << scheme_tree_depths.at(scheme) << '}';
  }
  if (!scheme_mapping_counts.empty()) {
    output << '\n';
  }
  output << "  }\n}\n";
  return output.str();
}

BuildResult ResourceBuilder::build(const ResourceBuildInput& input) const {
  return build(input, ResourceBuildOptions{});
}

BuildResult ResourceBuilder::build(
    const ResourceBuildInput& input,
    const ResourceBuildOptions& options) const {
  if (input.registry_namespace == 0U) {
    return build_failure("registry namespace must be non-zero");
  }
  if (input.source_name.empty() || input.source_version.empty()) {
    return build_failure("source name and version are required");
  }
  if (!is_valid_utf8(input.source_name) ||
      !is_valid_utf8(input.source_version) ||
      !is_valid_utf8(input.relation_basis)) {
    return build_failure("source metadata must be valid UTF-8");
  }

  std::vector<const LexemeSourceRecord*> approved;
  std::size_t pending_count = 0;
  std::unordered_set<StableLexemeKey> all_nonzero_keys;
  for (const LexemeSourceRecord& lexeme : input.lexemes) {
    if (!lexeme.stable_key.empty() &&
        !all_nonzero_keys.insert(lexeme.stable_key).second) {
      return build_failure("stable lexeme key is duplicated in build input");
    }
    if (lexeme.status == RegistryStatus::PendingMapping) {
      if (!is_valid_utf8(lexeme.identity_definition) ||
          !is_valid_utf8(lexeme.provenance)) {
        return build_failure("pending mapping metadata must be valid UTF-8");
      }
      ++pending_count;
      continue;
    }
    if (lexeme.status != RegistryStatus::Approved || lexeme.stable_key.empty() ||
        lexeme.identity_definition.empty() || lexeme.provenance.empty() ||
        lexeme.default_output_utf8.empty() ||
        !is_valid_utf8(lexeme.identity_definition) ||
        !is_valid_utf8(lexeme.provenance) ||
        !is_valid_utf8(lexeme.default_output_utf8) ||
        (lexeme.kind != LexemeKind::Character &&
         lexeme.kind != LexemeKind::Word)) {
      return build_failure("approved lexeme record is incomplete or invalid");
    }
    for (const OutputRelationInput& relation : lexeme.output_relations) {
      if (relation.form_id.empty() ||
          (relation.admission == OutputAdmission::Ready && relation.text_utf8.empty()) ||
          (relation.admission != OutputAdmission::Ready && relation.admission != OutputAdmission::Blocked) ||
          !relation.availability || (relation.availability & ~7U) ||
          (relation.condition_kind == ConditionKind::None ? !relation.condition.empty() :
             relation.condition_kind != ConditionKind::WordOrSense || relation.condition.empty()) ||
          !is_valid_utf8(relation.form_id) ||
          !is_valid_utf8(relation.text_utf8) ||
          !is_valid_utf8(relation.condition) ||
          !is_valid_utf8(relation.selection_basis)) {
        return build_failure("output relation is incomplete or invalid UTF-8");
      }
    }
    approved.push_back(&lexeme);
  }
  if (approved.empty() || approved.size() > kMaximumRecords) {
    return build_failure("build requires a bounded non-empty approved registry");
  }

  StringPool strings;
  detail::SharedBuildTables tables;
  std::unordered_map<StableLexemeKey, RuntimeLexemeRef> refs_by_key;
  for (std::size_t index = 0; index < approved.size(); ++index) {
    const auto& source = *approved[index];
    if (source.output_relations.size() > kMaximumRecords || tables.relations.size() >
        kMaximumRecords - source.output_relations.size()) return build_failure("too many output relations");
    SharedLexemeRecord lexeme;
    lexeme.identity = {source.stable_key, source.kind, source.base_weight};
    lexeme.default_payload = strings.add(source.default_output_utf8);
    lexeme.relation_start = static_cast<std::uint32_t>(tables.relations.size());
    lexeme.relation_count = static_cast<std::uint32_t>(source.output_relations.size());
    for (const auto& relation : source.output_relations) {
      auto optional_string = [&](const std::string& s) { return s.empty() ? kNoString : strings.add(s); };
      SharedSelectorRecord selector{strings.add(relation.form_id), optional_string(relation.selection_basis)};
      const auto selector_id = static_cast<std::uint32_t>(tables.selectors.size());
      tables.selectors.push_back(selector);
      tables.relations.push_back({selector_id, optional_string(relation.text_utf8),
          optional_string(relation.condition), relation.condition_kind, relation.availability,
          relation.admission, relation.preferred});
    }
    tables.lexemes.push_back(lexeme);
    refs_by_key.emplace(source.stable_key, static_cast<RuntimeLexemeRef>(index + 1));
  }
  tables.strings = strings.strings();
  BuiltResources resources;
  PagedSharedInfo shared_info;
  std::string shared_error;
  if (!detail::build_shared_bundle(input.registry_namespace, tables, options.shared_page_bytes,
                                   &resources.shared_bundle, &shared_info, &shared_error))
    return build_failure(shared_error);
  const SharedBundleId shared_bundle_id = shared_info.shared_bundle_id;
  resources.manifest.shared_page_bytes = shared_info.page_bytes;
  resources.manifest.uniqueness_sequence_bytes =
      std::size_t(shared_info.sections[4].page_count) * shared_info.page_bytes;

  std::map<std::string, std::vector<const SchemeMappingInput*>> by_scheme;
  for (const SchemeMappingInput& mapping : input.mappings) {
    if (mapping.scheme_id.empty() || mapping.code.empty() ||
        !is_valid_utf8(mapping.scheme_id) || !is_valid_utf8(mapping.code)) {
      return build_failure("scheme mapping has an invalid scheme or code");
    }
    if (refs_by_key.find(mapping.stable_key) == refs_by_key.end()) {
      return build_failure(
          "scheme mapping references a missing or unapproved stable key");
    }
    by_scheme[mapping.scheme_id].push_back(&mapping);
  }
  if (by_scheme.empty()) {
    return build_failure("at least one scheme bundle is required");
  }

  for (const auto& [scheme_id, mappings] : by_scheme) {
    std::vector<detail::PagedSchemeBuildMapping> paged_mappings;
    paged_mappings.reserve(mappings.size());
    for (std::size_t index = 0; index < mappings.size(); ++index) {
      const SchemeMappingInput& mapping = *mappings[index];
      paged_mappings.push_back(
          {mapping.code, refs_by_key.at(mapping.stable_key),
           mapping.scheme_weight, static_cast<std::uint32_t>(index)});
    }
    std::vector<std::uint8_t> scheme_bundle;
    detail::PagedSchemeBuildStats scheme_stats;
    std::string scheme_error;
    if (!detail::build_paged_scheme_bundle(
            scheme_id, input.registry_namespace, shared_bundle_id,
            paged_mappings, options.scheme_page_bytes, &scheme_bundle,
            &scheme_stats, &scheme_error)) {
      return build_failure("could not build paged scheme index: " +
                           scheme_error);
    }
    resources.scheme_bundles.emplace(scheme_id, std::move(scheme_bundle));
    resources.manifest.scheme_page_bytes[scheme_id] =
        scheme_stats.page_bytes;
    resources.manifest.scheme_tree_depths[scheme_id] =
        scheme_stats.tree_depth;
  }

  resources.manifest.registry_namespace = input.registry_namespace;
  resources.manifest.shared_bundle_id = shared_bundle_id;
  resources.manifest.source_name = input.source_name;
  resources.manifest.source_version = input.source_version;
  resources.manifest.relation_basis = input.relation_basis;
  resources.manifest.approved_lexeme_count = approved.size();
  resources.manifest.pending_mapping_count = pending_count;
  resources.manifest.string_pool_count = strings.strings().size();
  resources.manifest.output_relation_count = tables.relations.size();
  resources.manifest.shared_bundle_bytes = resources.shared_bundle.size();
  for (const LexemeSourceRecord& lexeme : input.lexemes) {
    if (lexeme.status == RegistryStatus::Approved) {
      resources.manifest.identity_trace.push_back(
          {lexeme.stable_key, lexeme.kind, lexeme.provenance,
           lexeme.output_relations.size()});
    } else if (lexeme.status == RegistryStatus::PendingMapping) {
      resources.manifest.pending_mapping_records.push_back(
          lexeme.provenance.empty() ? lexeme.identity_definition
                                    : lexeme.provenance);
    }
  }
  for (const auto& [scheme_id, mappings] : by_scheme) {
    resources.manifest.scheme_mapping_counts[scheme_id] = mappings.size();
    resources.manifest.scheme_bundle_bytes[scheme_id] =
        resources.scheme_bundles.at(scheme_id).size();
  }

  BuildResult result;
  result.ok = true;
  result.resources = std::move(resources);
  return result;
}

struct MountedResourceSet::Impl {
  bool is_mounted{false};
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId shared_bundle_id{0};
  MountEpoch mount_epoch{0};
  std::shared_ptr<const ResourceReader> shared_reader;
  std::vector<StringSpan> strings;
  std::vector<ParsedLexeme> lexemes;
  std::vector<ParsedRelation> relations;
  std::map<std::string, ParsedScheme> schemes;

  bool read_string(std::uint32_t index, std::string* output) const {
    return shared_reader && index < strings.size() &&
           read_span(*shared_reader, strings[index], output);
  }
};

MountedResourceSet::MountedResourceSet() : impl_(std::make_unique<Impl>()) {}
MountedResourceSet::~MountedResourceSet() = default;

bool MountedResourceSet::mount(
    std::shared_ptr<const ResourceReader> shared_bundle,
    const std::vector<std::shared_ptr<const ResourceReader>>& scheme_bundles,
    std::string* error) {
  std::string local_error;
  if (error == nullptr) {
    error = &local_error;
  }
  if (!shared_bundle || scheme_bundles.empty()) {
    *error = "mount requires one shared bundle and at least one scheme bundle";
    return false;
  }

  ParsedShared parsed_shared;
  if (!parse_shared_bundle(*shared_bundle, &parsed_shared, error)) {
    return false;
  }
  std::map<std::string, ParsedScheme> parsed_schemes;
  for (const auto& scheme_reader : scheme_bundles) {
    if (!scheme_reader) {
      *error = "scheme reader is null";
      return false;
    }
    ParsedScheme scheme;
    if (!parse_scheme_bundle(*scheme_reader, &scheme, error)) {
      return false;
    }
    if (scheme.registry_namespace != parsed_shared.registry_namespace ||
        scheme.required_shared_bundle_id != parsed_shared.shared_bundle_id) {
      *error = "scheme bundle is bound to another shared bundle";
      return false;
    }
    for (const SchemeEntry& entry : scheme.entries) {
      if (entry.runtime_ref == 0U ||
          entry.runtime_ref > parsed_shared.lexemes.size()) {
        *error = "scheme bundle contains an invalid runtime reference";
        return false;
      }
    }
    if (!parsed_schemes.emplace(scheme.scheme_id, std::move(scheme)).second) {
      *error = "scheme bundle id is duplicated";
      return false;
    }
  }

  impl_->registry_namespace = parsed_shared.registry_namespace;
  impl_->shared_bundle_id = parsed_shared.shared_bundle_id;
  impl_->shared_reader = std::move(shared_bundle);
  impl_->strings = std::move(parsed_shared.strings);
  impl_->lexemes = std::move(parsed_shared.lexemes);
  impl_->relations = std::move(parsed_shared.relations);
  impl_->schemes = std::move(parsed_schemes);
  impl_->mount_epoch =
      impl_->mount_epoch == std::numeric_limits<MountEpoch>::max()
          ? 1U
          : impl_->mount_epoch + 1U;
  impl_->is_mounted = true;
  error->clear();
  return true;
}

bool MountedResourceSet::mounted() const { return impl_->is_mounted; }

RegistryNamespaceId MountedResourceSet::registry_namespace() const {
  return impl_->registry_namespace;
}

SharedBundleId MountedResourceSet::shared_bundle_id() const {
  return impl_->shared_bundle_id;
}

MountEpoch MountedResourceSet::mount_epoch() const {
  return impl_->mount_epoch;
}

bool MountedResourceSet::has_scheme(std::string_view scheme_id) const {
  return impl_->schemes.find(std::string(scheme_id)) != impl_->schemes.end();
}

std::vector<QueryHit> MountedResourceSet::query(
    std::string_view scheme_id, std::string_view code) const {
  std::vector<QueryHit> hits;
  const auto scheme = impl_->schemes.find(std::string(scheme_id));
  if (scheme == impl_->schemes.end()) {
    return hits;
  }
  for (const SchemeEntry& entry : scheme->second.entries) {
    if (entry.code == code) {
      hits.push_back({entry.runtime_ref, entry.scheme_weight});
    }
  }
  return hits;
}

bool MountedResourceSet::lexeme_info(RuntimeLexemeRef runtime_ref,
                                     RuntimeLexemeInfo* output) const {
  if (!output || runtime_ref == 0U || runtime_ref > impl_->lexemes.size()) {
    return false;
  }
  const ParsedLexeme& lexeme = impl_->lexemes[runtime_ref - 1U];
  output->stable_key = lexeme.stable_key;
  output->kind = lexeme.kind;
  output->base_weight = lexeme.base_weight;
  return true;
}

OutputResolution MountedResourceSet::resolve_output(
    RuntimeLexemeRef runtime_ref, std::string_view form_id, OutputInvocation invocation) const {
  OutputResolution result;
  if (runtime_ref == 0U || runtime_ref > impl_->lexemes.size()) {
    result.status = OutputResolutionStatus::InvalidReference;
    return result;
  }
  const ParsedLexeme& lexeme = impl_->lexemes[runtime_ref - 1U];
  const bool default_requested = form_id.empty() || form_id == "default";
  if (!default_requested) {
    const ParsedRelation* match = nullptr;
    std::size_t match_count = 0;
    for (std::uint32_t offset = 0; offset < lexeme.relation_count; ++offset) {
      const ParsedRelation& relation =
          impl_->relations[lexeme.relation_start + offset];
      std::string relation_form;
      if (!impl_->read_string(relation.form_id, &relation_form)) {
        result.status = OutputResolutionStatus::ReadError;
        return result;
      }
      if (relation_form == form_id) {
        match = &relation;
        ++match_count;
      }
    }
    if (match_count > 1U) {
      result.status = OutputResolutionStatus::MultipleFormsUnsupported;
      return result;
    }
    if (match_count == 1U) {
      if (match->admission == OutputAdmission::Blocked) {
        result.status = OutputResolutionStatus::OutputNotReady; return result;
      }
      if (!(match->availability & static_cast<std::uint8_t>(invocation))) {
        result.status = OutputResolutionStatus::NotAvailable; return result;
      }
      if (match->condition_kind != ConditionKind::None) {
        std::string condition;
        if (!impl_->read_string(match->condition, &condition)) {
          result.status = OutputResolutionStatus::ReadError;
          return result;
        }
        if (!condition.empty()) {
          result.status = OutputResolutionStatus::ConditionUndetermined;
          return result;
        }
      }
      if (!impl_->read_string(match->text, &result.text_utf8)) {
        result.status = OutputResolutionStatus::ReadError;
        return result;
      }
      result.status = OutputResolutionStatus::Exact;
      return result;
    }
  }
  if (!impl_->read_string(lexeme.default_output, &result.text_utf8)) {
    result.status = OutputResolutionStatus::ReadError;
    return result;
  }
  result.status = default_requested ? OutputResolutionStatus::Exact
                                    : OutputResolutionStatus::FallbackDefault;
  return result;
}

bool MountedResourceSet::output_relations(
    RuntimeLexemeRef runtime_ref,
    std::vector<OutputRelationView>* output) const {
  if (!output || runtime_ref == 0U || runtime_ref > impl_->lexemes.size()) {
    return false;
  }
  output->clear();
  const ParsedLexeme& lexeme = impl_->lexemes[runtime_ref - 1U];
  output->reserve(lexeme.relation_count);
  for (std::uint32_t offset = 0; offset < lexeme.relation_count; ++offset) {
    const ParsedRelation& relation =
        impl_->relations[lexeme.relation_start + offset];
    OutputRelationView view;
    if (!impl_->read_string(relation.form_id, &view.form_id) ||
        (relation.text != kNoString && !impl_->read_string(relation.text, &view.text_utf8)) ||
        (relation.condition != kNoString &&
         !impl_->read_string(relation.condition, &view.condition)) ||
        (relation.selection_basis != kNoString &&
         !impl_->read_string(relation.selection_basis,
                             &view.selection_basis))) {
      output->clear();
      return false;
    }
    view.preferred = relation.preferred;
    view.condition_kind = relation.condition_kind;
    view.availability = relation.availability;
    view.admission = relation.admission;
    output->push_back(std::move(view));
  }
  return true;
}

std::size_t MountedResourceSet::output_relation_count(
    RuntimeLexemeRef runtime_ref) const {
  if (runtime_ref == 0U || runtime_ref > impl_->lexemes.size()) {
    return 0U;
  }
  return impl_->lexemes[runtime_ref - 1U].relation_count;
}

std::uint32_t MountedResourceSet::default_payload_index(
    RuntimeLexemeRef runtime_ref) const {
  if (runtime_ref == 0U || runtime_ref > impl_->lexemes.size()) {
    return kNoString;
  }
  return impl_->lexemes[runtime_ref - 1U].default_output;
}

std::size_t MountedResourceSet::string_pool_count() const {
  return impl_->strings.size();
}

UserStateStore::UserStateStore(RegistryNamespaceId registry_namespace,
                               UserStatePersistence& persistence)
    : registry_namespace_(registry_namespace), persistence_(persistence) {}

RegistryNamespaceId UserStateStore::registry_namespace() const {
  return registry_namespace_;
}

bool UserStateStore::ensure_loaded(StableLexemeKey stable_key,
                                   Entry** output) {
  Entry& entry = entries_[stable_key];
  if (!entry.loaded) {
    std::uint32_t persisted = 0;
    const PersistenceLoadResult result =
        persistence_.load(registry_namespace_, stable_key, &persisted);
    if (result == PersistenceLoadResult::Error || result == PersistenceLoadResult::Incompatible) {
      return false;
    }
    entry.count = result == PersistenceLoadResult::Found ? persisted : 0U;
    entry.loaded = true;
  }
  *output = &entry;
  return true;
}

bool UserStateStore::selection_count(StableLexemeKey stable_key,
                                     std::uint32_t* output) {
  if (!output || stable_key.empty()) {
    return false;
  }
  Entry* entry = nullptr;
  if (!ensure_loaded(stable_key, &entry)) {
    return false;
  }
  *output = entry->count;
  return true;
}

StateUpdateResult UserStateStore::increment_and_persist(
    StableLexemeKey stable_key) {
  Entry* entry = nullptr;
  if (!ensure_loaded(stable_key, &entry)) {
    return StateUpdateResult::LoadError;
  }
  if (entry->count != std::numeric_limits<std::uint32_t>::max()) {
    ++entry->count;
  }
  entry->dirty = true;
  if (persistence_.write(registry_namespace_, stable_key, entry->count)) {
    entry->dirty = false;
    return StateUpdateResult::Persisted;
  }
  return StateUpdateResult::Dirty;
}

bool UserStateStore::retry_dirty() {
  bool all_persisted = true;
  for (auto& [stable_key, entry] : entries_) {
    if (!entry.dirty) {
      continue;
    }
    if (persistence_.write(registry_namespace_, stable_key, entry.count)) {
      entry.dirty = false;
    } else {
      all_persisted = false;
    }
  }
  return all_persisted;
}

std::size_t UserStateStore::dirty_count() const {
  return static_cast<std::size_t>(std::count_if(
      entries_.begin(), entries_.end(),
      [](const auto& item) { return item.second.dirty; }));
}

bool CandidateHandle::operator==(const CandidateHandle& other) const {
  return snapshot_id == other.snapshot_id && query_id == other.query_id &&
         mount_epoch == other.mount_epoch &&
         runtime_ref == other.runtime_ref;
}

ImeSession::ImeSession(MountedResourceSet& resources,
                       UserStateStore& user_state,
                       CandidatePresenter& presenter,
                       TextCommitSink& commit_sink,
                       std::string initial_scheme)
    : resources_(resources),
      user_state_(user_state),
      presenter_(presenter),
      commit_sink_(commit_sink),
      active_scheme_(std::move(initial_scheme)) {}

bool ImeSession::query_complete_code(std::string code, std::string* error) {
  std::string local_error;
  if (error == nullptr) {
    error = &local_error;
  }
  if (snapshot_state_ == CandidateSnapshotState::Committing) {
    *error = "a commit is in progress";
    return false;
  }
  if (!resources_.mounted() || !resources_.has_scheme(active_scheme_)) {
    *error = "active resources or scheme are unavailable";
    return false;
  }
  if (code.empty() || !is_valid_utf8(code)) {
    *error = "complete code must be non-empty valid UTF-8";
    return false;
  }

  learning_available_ = resources_.registry_namespace() == user_state_.registry_namespace();
  candidates_.clear();
  snapshot_state_ = CandidateSnapshotState::Invalid;
  raw_code_ = std::move(code);
  const std::uint64_t query_id = next_query_id_++;
  const std::uint64_t snapshot_id = next_snapshot_id_++;
  const std::vector<QueryHit> raw_hits =
      resources_.query(active_scheme_, raw_code_);

  std::unordered_map<RuntimeLexemeRef, std::int32_t> display_hits;
  for (const QueryHit& hit : raw_hits) {
    const auto existing = display_hits.find(hit.runtime_ref);
    if (existing == display_hits.end()) {
      display_hits.emplace(hit.runtime_ref, hit.scheme_weight);
    } else {
      existing->second = std::max(existing->second, hit.scheme_weight);
    }
  }

  struct PreparedCandidate {
    CandidateSlot slot;
    StableLexemeKey stable_key{};
  };
  std::vector<PreparedCandidate> prepared;
  prepared.reserve(display_hits.size());
  for (const auto& [runtime_ref, scheme_weight] : display_hits) {
    RuntimeLexemeInfo info;
    if (!resources_.lexeme_info(runtime_ref, &info)) {
      *error = "query returned an invalid runtime reference";
      candidates_.clear();
      return false;
    }
    std::uint32_t selection_count = 0;
    if (resources_.registry_namespace() != user_state_.registry_namespace() ||
        !user_state_.selection_count(info.stable_key, &selection_count)) {
      // Input first: an unavailable learning record is not a failed text query.
      selection_count = 0;
      learning_available_ = false;
    }
    OutputResolution output =
        resources_.resolve_output(runtime_ref, output_form_);
    if (output.status == OutputResolutionStatus::InvalidReference ||
        output.status == OutputResolutionStatus::ReadError ||
        output.status == OutputResolutionStatus::ConditionUndetermined ||
        output.status == OutputResolutionStatus::MultipleFormsUnsupported ||
        output.status == OutputResolutionStatus::OutputNotReady ||
        output.status == OutputResolutionStatus::NotAvailable) {
      if (output.status == OutputResolutionStatus::MultipleFormsUnsupported) {
        *error = "selected output form has multiple unresolved variants";
      } else if (output.status ==
                 OutputResolutionStatus::ConditionUndetermined) {
        *error = "selected output form requires an unevaluated condition";
      } else {
        *error = "candidate output could not be read";
      }
      candidates_.clear();
      return false;
    }
    CandidateSlot slot;
    slot.handle = {snapshot_id, query_id, resources_.mount_epoch(), runtime_ref};
    slot.text_utf8 = std::move(output.text_utf8);
    slot.selection_count = selection_count;
    slot.score = static_cast<std::int64_t>(info.base_weight) + scheme_weight +
                 static_cast<std::int64_t>(selection_count) * 100;
    slot.output_status = output.status;
    prepared.push_back({std::move(slot), info.stable_key});
  }

  std::sort(prepared.begin(), prepared.end(),
            [](const PreparedCandidate& left, const PreparedCandidate& right) {
              if (left.slot.score != right.slot.score) {
                return left.slot.score > right.slot.score;
              }
              return left.stable_key < right.stable_key;
            });
  for (PreparedCandidate& candidate : prepared) {
    candidates_.push_back(std::move(candidate.slot));
  }
  snapshot_state_ = candidates_.empty() ? CandidateSnapshotState::None
                                        : CandidateSnapshotState::Ready;
  presenter_.present(snapshot_id, current_candidates());
  error->clear();
  return true;
}

ConfirmResult ImeSession::confirm(const CandidateHandle& handle) {
  if (snapshot_state_ != CandidateSnapshotState::Ready) {
    return ConfirmResult::RejectedStale;
  }
  const MountEpoch current_epoch = resources_.mount_epoch();
  const bool current_snapshot_is_current =
      !candidates_.empty() &&
      std::all_of(candidates_.begin(), candidates_.end(),
                  [current_epoch](const CandidateSlot& slot) {
                    return slot.handle.mount_epoch == current_epoch;
                  });
  if (!current_snapshot_is_current) {
    invalidate_snapshot();
    return ConfirmResult::RejectedStale;
  }
  if (handle.mount_epoch != current_epoch) {
    return ConfirmResult::RejectedStale;
  }
  const auto candidate = std::find_if(
      candidates_.begin(), candidates_.end(),
      [&handle](const CandidateSlot& slot) { return slot.handle == handle; });
  if (candidate == candidates_.end()) {
    return ConfirmResult::RejectedStale;
  }

  RuntimeLexemeInfo info;
  if (!resources_.lexeme_info(candidate->handle.runtime_ref, &info)) {
    invalidate_snapshot();
    return ConfirmResult::RejectedStale;
  }
  snapshot_state_ = CandidateSnapshotState::Committing;
  if (!commit_sink_.commit(candidate->text_utf8)) {
    snapshot_state_ = CandidateSnapshotState::Ready;
    return ConfirmResult::FailedRetryable;
  }

  invalidate_snapshot();
  if (resources_.registry_namespace() != user_state_.registry_namespace()) {
    learning_available_ = false;
    return ConfirmResult::CommittedStateLoadError;
  }
  const StateUpdateResult state_result =
      user_state_.increment_and_persist(info.stable_key);
  if (state_result == StateUpdateResult::Persisted) {
    return ConfirmResult::CommittedPersisted;
  }
  if (state_result == StateUpdateResult::Dirty) {
    return ConfirmResult::CommittedStateDirty;
  }
  learning_available_ = false;
  return ConfirmResult::CommittedStateLoadError;
}

void ImeSession::cancel() {
  if (snapshot_state_ != CandidateSnapshotState::Committing) {
    invalidate_snapshot();
  }
}

bool ImeSession::switch_scheme_when_idle(std::string scheme_id) {
  if (!idle() || !resources_.has_scheme(scheme_id)) {
    return false;
  }
  active_scheme_ = std::move(scheme_id);
  return true;
}

bool ImeSession::set_output_form_when_idle(std::string form_id) {
  if (!idle() || !is_valid_utf8(form_id)) {
    return false;
  }
  output_form_ = std::move(form_id);
  return true;
}

bool ImeSession::learning_available() const { return learning_available_; }

bool ImeSession::idle() const {
  return raw_code_.empty() && candidates_.empty() &&
         snapshot_state_ != CandidateSnapshotState::Committing;
}

std::string_view ImeSession::active_scheme() const { return active_scheme_; }

std::string_view ImeSession::output_form() const { return output_form_; }

CandidateSnapshotState ImeSession::snapshot_state() const {
  return snapshot_state_;
}

std::vector<CandidateView> ImeSession::current_candidates() const {
  std::vector<CandidateView> views;
  views.reserve(candidates_.size());
  for (const CandidateSlot& slot : candidates_) {
    RuntimeLexemeInfo info;
    if (!resources_.lexeme_info(slot.handle.runtime_ref, &info)) {
      continue;
    }
    CandidateView view;
    view.handle = slot.handle;
    view.stable_key = info.stable_key;
    view.kind = info.kind;
    view.text_utf8 = slot.text_utf8;
    view.selection_count = slot.selection_count;
    view.score = slot.score;
    view.output_status = slot.output_status;
    view.learning_available = learning_available_;
    views.push_back(std::move(view));
  }
  return views;
}

void ImeSession::invalidate_snapshot() {
  candidates_.clear();
  raw_code_.clear();
  snapshot_state_ = CandidateSnapshotState::Invalid;
}

}  // namespace hanime
