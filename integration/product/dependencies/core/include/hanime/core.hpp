#pragma once

#include "hanime/stable_key.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hanime {

using RuntimeLexemeRef = std::uint32_t;
using RegistryNamespaceId = std::uint64_t;
using SharedBundleId = std::uint64_t;
using MountEpoch = std::uint64_t;

enum class LexemeKind : std::uint8_t {
  Character = 1,
  Word = 2,
};

enum class RegistryStatus : std::uint8_t {
  Approved = 1,
  PendingMapping = 2,
};

enum class ConditionKind : std::uint8_t { None = 0, WordOrSense = 1 };
enum class OutputAdmission : std::uint8_t { Ready = 0, Blocked = 1 };
enum class OutputInvocation : std::uint8_t { Profile = 1, Explicit = 2, Expanded = 4 };

struct OutputRelationInput {
  std::string form_id;
  std::string text_utf8;
  std::string condition;
  std::string selection_basis;
  bool preferred{false};
  ConditionKind condition_kind{ConditionKind::None};
  std::uint8_t availability{3};  // Profile | Explicit
  OutputAdmission admission{OutputAdmission::Ready};
};

struct LexemeSourceRecord {
  StableLexemeKey stable_key{};
  RegistryStatus status{RegistryStatus::PendingMapping};
  LexemeKind kind{LexemeKind::Word};
  std::string identity_definition;
  std::string provenance;
  std::int32_t base_weight{0};
  std::string default_output_utf8;
  std::vector<OutputRelationInput> output_relations;
};

struct SchemeMappingInput {
  std::string scheme_id;
  std::string code;
  StableLexemeKey stable_key{};
  std::int32_t scheme_weight{0};
};

struct ResourceBuildInput {
  RegistryNamespaceId registry_namespace{0};
  std::string source_name;
  std::string source_version;
  std::string relation_basis;
  std::vector<LexemeSourceRecord> lexemes;
  std::vector<SchemeMappingInput> mappings;
};

struct ResourceBuildOptions {
  std::uint32_t scheme_page_bytes{256};
  std::uint32_t shared_page_bytes{256};
};

struct IdentityTraceRecord {
  StableLexemeKey stable_key{};
  LexemeKind kind{LexemeKind::Word};
  std::string provenance;
  std::size_t output_relation_count{0};
};

struct BuildManifest {
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId shared_bundle_id{0};
  std::string source_name;
  std::string source_version;
  std::string relation_basis;
  std::size_t approved_lexeme_count{0};
  std::size_t pending_mapping_count{0};
  std::size_t string_pool_count{0};
  std::size_t output_relation_count{0};
  std::size_t shared_bundle_bytes{0};
  std::uint32_t shared_page_bytes{0};
  std::size_t uniqueness_sequence_bytes{0};
  std::vector<IdentityTraceRecord> identity_trace;
  std::vector<std::string> pending_mapping_records;
  std::map<std::string, std::size_t> scheme_mapping_counts;
  std::map<std::string, std::size_t> scheme_bundle_bytes;
  std::map<std::string, std::uint32_t> scheme_page_bytes;
  std::map<std::string, std::uint32_t> scheme_tree_depths;

  std::string to_json() const;
};

struct BuiltResources {
  std::vector<std::uint8_t> shared_bundle;
  std::map<std::string, std::vector<std::uint8_t>> scheme_bundles;
  BuildManifest manifest;
};

struct BuildResult {
  bool ok{false};
  std::string error;
  BuiltResources resources;
};

class ResourceBuilder {
 public:
  BuildResult build(const ResourceBuildInput& input) const;
  BuildResult build(const ResourceBuildInput& input,
                    const ResourceBuildOptions& options) const;
};

struct WorkBudget {
  std::size_t max_read_calls{1};
  std::size_t max_read_bytes{256};
  std::size_t max_work_items{32};
};

struct WorkCost {
  std::size_t read_calls{0};
  std::size_t read_bytes{0};
  std::size_t work_items{0};
};

enum class ResourceReadStatus : std::uint8_t { Pending, Ready, Error };
struct ResourceReadStep {
  ResourceReadStatus status{ResourceReadStatus::Error};
  WorkCost cost;
};
struct ResourceReadContext {
  std::shared_ptr<const void> owner; // Existing activation token; held while pending.
  std::uint64_t revision{0};
};
class ResourceReader {
 public:
  virtual ~ResourceReader() = default;
  virtual std::size_t size() const = 0;
  virtual bool read(std::size_t offset, void* destination,
                    std::size_t byte_count) const = 0;
  // Legacy readers remain synchronous. Authenticated readers may yield while
  // checking blocks; destination stays alive and unpublished until Ready.
  virtual ResourceReadStep read_budgeted(std::size_t offset, void* destination,
      std::size_t byte_count, const WorkBudget& budget,
      const ResourceReadContext& = {}) const {
    if (!healthy()) return {};
    if (!budget.max_read_calls || byte_count > budget.max_read_bytes)
      return {ResourceReadStatus::Pending,{}};
    const bool ok=read(offset,destination,byte_count);
    return {ok ? ResourceReadStatus::Ready : ResourceReadStatus::Error,{1,byte_count,0}};
  }
  // Fixed-cost no-I/O lifetime/fatal-error guard, checked before owner cache hits.
  // A legacy reader promises immutability for its lifetime unless overridden.
  virtual bool healthy() const { return true; }
  virtual void cancel_read_budgeted(const ResourceReadContext&) const {}
};

enum class ValidationStepStatus : std::uint8_t {
  InProgress,
  Ready,
  InvalidBudget,
  Error,
  Stale,
};

struct ValidationStepResult {
  ValidationStepStatus status{ValidationStepStatus::Error};
  WorkCost cost;
  std::string error;
};

enum class ExactQueryStepStatus : std::uint8_t {
  InProgress,
  HitsAvailable,
  Complete,
  NoResults,
  Cancelled,
  Stale,
  InvalidBudget,
  Error,
};

struct RawQueryHit {
  RuntimeLexemeRef runtime_ref{0};
  std::int32_t scheme_weight{0};
  std::uint32_t mapping_ordinal{0};
};

// Opaque lifetime token, shared by the index activation and outstanding work.
// Keeping the token alive prevents identity reuse even if an index address is reused.
struct QueryIndexIdentity;

struct QueryBatchContext {
  std::shared_ptr<const QueryIndexIdentity> index_identity;
  std::uint64_t query_revision{0};
  std::uint64_t index_epoch{0};
  MountEpoch mount_epoch{0};
};

struct RawQueryBatch {
  QueryBatchContext context;
  std::vector<RawQueryHit> hits;
};

struct ExactQueryStepResult {
  ExactQueryStepStatus status{ExactQueryStepStatus::Error};
  WorkCost cost;
  RawQueryBatch batch;
  std::string error;
};

struct PagedSchemeIndexConfig {
  std::size_t page_cache_capacity{2};
  std::size_t max_single_read_bytes{256};
};

struct PagedSchemeIndexInfo {
  std::string scheme_id;
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId required_shared_bundle_id{0};
  std::size_t file_bytes{0};
  std::uint32_t page_bytes{0};
  std::uint32_t page_count{0};
  std::uint32_t leaf_page_count{0};
  std::uint32_t tree_depth{0};
  std::uint32_t mapping_count{0};
  std::size_t initial_read_calls{0};
  std::size_t initial_read_bytes{0};
  std::uint64_t root_offset{0}; // Final serialized root span for Host publishing.
};

struct ExactQueryMemoryUsage {
  std::size_t impl_bytes{0}, code_capacity_bytes{0}, code_heap_bytes{0}, page_capacity_bytes{0};
};
class ExactQueryCursor {
 public:
  ExactQueryCursor();
  ~ExactQueryCursor();
  ExactQueryCursor(ExactQueryCursor&&) noexcept;
  ExactQueryCursor& operator=(ExactQueryCursor&&) noexcept;
  ExactQueryCursor(const ExactQueryCursor&) = delete;
  ExactQueryCursor& operator=(const ExactQueryCursor&) = delete;
  ExactQueryMemoryUsage memory_usage() const;

 private:
  friend class PagedSchemeIndex;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class VerifiedResourceReader;
struct ResourceReleaseSpec;
class BaseOrderIndex;
class PagedSharedResource;
class PagedSchemeIndex {
 public:
  PagedSchemeIndex();
  ~PagedSchemeIndex();
  PagedSchemeIndex(const PagedSchemeIndex&) = delete;
  PagedSchemeIndex& operator=(const PagedSchemeIndex&) = delete;

  bool open(std::shared_ptr<const ResourceReader> scheme_bundle,
            RegistryNamespaceId expected_registry_namespace,
            SharedBundleId expected_shared_bundle_id,
            RuntimeLexemeRef maximum_runtime_ref,
            const PagedSchemeIndexConfig& config, std::string* error);
  ValidationStepResult advance_validation(const WorkBudget& budget);
  // The shared owner must outlive this fast-activated scheme owner.
  bool open_verified(std::shared_ptr<const VerifiedResourceReader>,
                     const PagedSharedResource&, const PagedSchemeIndexConfig&,
                     std::string* error);

  bool ready() const;
  const ResourceReleaseSpec* verified_release() const;
  QueryBatchContext activation_context() const; // Identity/epoch only, not a query.
  bool activation_is_current(const QueryBatchContext&) const;
  // Optional, serial attachment before starting input; replaces current query.
  // Shared/scheme owners outlive this attachment and all its borrowers.
  bool attach_base_order(std::shared_ptr<BaseOrderIndex>);
  std::shared_ptr<BaseOrderIndex> base_order() const;
  const PagedSchemeIndexInfo& info() const;
  std::uint64_t index_epoch() const;
  std::uint64_t current_query_revision() const;
  std::size_t cached_page_count() const;
  std::size_t resident_page_bytes() const;

  bool begin_exact_query(std::string code, MountEpoch mount_epoch,
                         ExactQueryCursor* cursor, std::string* error);
  QueryBatchContext query_context(const ExactQueryCursor& cursor) const;
  bool query_is_current(const QueryBatchContext& context) const;
  // Conditional SAME-code restart. A foreign/stale expected context or cursor
  // cannot replace current work. No I/O; code length remains caller bounded.
  bool restart_exact_query(const QueryBatchContext& expected,
                           ExactQueryCursor* cursor, std::string* error);
  ExactQueryStepResult advance_exact_query(ExactQueryCursor* cursor,
                                           const WorkBudget& budget,
                                           std::size_t batch_capacity);
  void cancel_query(ExactQueryCursor* cursor);
  // Releases only this cursor's pending reader operation, independently of
  // resource freshness. Does not cancel the query or change its revision;
  // later advance may re-read the unfinished span. Foreign ownership is a no-op.
  void release_query_read(const ExactQueryCursor& cursor);
  bool batch_is_current(const RawQueryBatch& batch) const;
  void deactivate();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct RuntimeLexemeInfo {
  StableLexemeKey stable_key{};
  LexemeKind kind{LexemeKind::Word};
  std::int32_t base_weight{0};
};

enum class OutputResolutionStatus : std::uint8_t {
  Exact,
  FallbackDefault,
  ConditionUndetermined,
  MultipleFormsUnsupported,
  InvalidReference,
  ReadError,
  OutputNotReady,
  NotAvailable,
};

struct OutputResolution {
  OutputResolutionStatus status{OutputResolutionStatus::ReadError};
  std::string text_utf8;
};

struct OutputRelationView {
  std::string form_id;
  std::string text_utf8;
  std::string condition;
  std::string selection_basis;
  bool preferred{false};
  ConditionKind condition_kind{ConditionKind::None};
  std::uint8_t availability{3};
  OutputAdmission admission{OutputAdmission::Ready};
};

struct QueryHit {
  RuntimeLexemeRef runtime_ref{0};
  std::int32_t scheme_weight{0};
};

class MountedResourceSet {
 public:
  MountedResourceSet();
  ~MountedResourceSet();
  MountedResourceSet(const MountedResourceSet&) = delete;
  MountedResourceSet& operator=(const MountedResourceSet&) = delete;

  bool mount(std::shared_ptr<const ResourceReader> shared_bundle,
             const std::vector<std::shared_ptr<const ResourceReader>>&
                 scheme_bundles,
             std::string* error);

  bool mounted() const;
  RegistryNamespaceId registry_namespace() const;
  SharedBundleId shared_bundle_id() const;
  MountEpoch mount_epoch() const;
  bool has_scheme(std::string_view scheme_id) const;
  std::vector<QueryHit> query(std::string_view scheme_id,
                              std::string_view code) const;
  bool lexeme_info(RuntimeLexemeRef runtime_ref,
                   RuntimeLexemeInfo* output) const;
  OutputResolution resolve_output(RuntimeLexemeRef runtime_ref,
                                  std::string_view form_id,
                                  OutputInvocation invocation = OutputInvocation::Profile) const;
  bool output_relations(RuntimeLexemeRef runtime_ref,
                        std::vector<OutputRelationView>* output) const;
  std::size_t output_relation_count(RuntimeLexemeRef runtime_ref) const;
  std::uint32_t default_payload_index(RuntimeLexemeRef runtime_ref) const;
  std::size_t string_pool_count() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

enum class PersistenceLoadResult : std::uint8_t {
  Found,
  NotFound,
  Error,
  Incompatible,
};

class UserStatePersistence {
 public:
  virtual ~UserStatePersistence() = default;
  virtual PersistenceLoadResult load(RegistryNamespaceId registry_namespace,
                                     StableLexemeKey stable_key,
                                     std::uint32_t* selection_count) = 0;
  virtual bool write(RegistryNamespaceId registry_namespace,
                     StableLexemeKey stable_key,
                     std::uint32_t selection_count) = 0;
};

enum class StateUpdateResult : std::uint8_t {
  Persisted,
  Dirty,
  LoadError,
};

class UserStateStore {
 public:
  UserStateStore(RegistryNamespaceId registry_namespace,
                 UserStatePersistence& persistence);

  RegistryNamespaceId registry_namespace() const;
  bool selection_count(StableLexemeKey stable_key, std::uint32_t* output);
  StateUpdateResult increment_and_persist(StableLexemeKey stable_key);
  bool retry_dirty();
  std::size_t dirty_count() const;

 private:
  struct Entry {
    std::uint32_t count{0};
    bool loaded{false};
    bool dirty{false};
  };

  bool ensure_loaded(StableLexemeKey stable_key, Entry** output);

  RegistryNamespaceId registry_namespace_;
  UserStatePersistence& persistence_;
  std::unordered_map<StableLexemeKey, Entry> entries_;
};

struct CandidateHandle {
  std::uint64_t snapshot_id{0};
  std::uint64_t query_id{0};
  MountEpoch mount_epoch{0};
  RuntimeLexemeRef runtime_ref{0};

  bool operator==(const CandidateHandle& other) const;
};

struct CandidateView {
  CandidateHandle handle;
  StableLexemeKey stable_key{};
  LexemeKind kind{LexemeKind::Word};
  std::string text_utf8;
  std::uint32_t selection_count{0};
  std::int64_t score{0};
  OutputResolutionStatus output_status{OutputResolutionStatus::ReadError};
  bool learning_available{true};
};

class CandidatePresenter {
 public:
  virtual ~CandidatePresenter() = default;
  virtual bool present(std::uint64_t snapshot_id,
                       const std::vector<CandidateView>& candidates) = 0;
};

class TextCommitSink {
 public:
  virtual ~TextCommitSink() = default;
  virtual bool commit(std::string_view text_utf8) = 0;
};

enum class CandidateSnapshotState : std::uint8_t {
  None,
  Ready,
  Committing,
  Invalid,
};

enum class ConfirmResult : std::uint8_t {
  RejectedStale,
  FailedRetryable,
  CommittedPersisted,
  CommittedStateDirty,
  CommittedStateLoadError,
};

class ImeSession {
 public:
  ImeSession(MountedResourceSet& resources, UserStateStore& user_state,
             CandidatePresenter& presenter, TextCommitSink& commit_sink,
             std::string initial_scheme);

  bool query_complete_code(std::string code, std::string* error);
  ConfirmResult confirm(const CandidateHandle& handle);
  void cancel();
  bool switch_scheme_when_idle(std::string scheme_id);
  bool set_output_form_when_idle(std::string form_id);
  bool learning_available() const;

  bool idle() const;
  std::string_view active_scheme() const;
  std::string_view output_form() const;
  CandidateSnapshotState snapshot_state() const;
  std::vector<CandidateView> current_candidates() const;

 private:
  struct CandidateSlot {
    CandidateHandle handle;
    std::string text_utf8;
    std::uint32_t selection_count{0};
    std::int64_t score{0};
    OutputResolutionStatus output_status{OutputResolutionStatus::ReadError};
  };

  void invalidate_snapshot();

  MountedResourceSet& resources_;
  UserStateStore& user_state_;
  CandidatePresenter& presenter_;
  TextCommitSink& commit_sink_;
  std::string active_scheme_;
  std::string output_form_;
  std::string raw_code_;
  std::uint64_t next_query_id_{1};
  std::uint64_t next_snapshot_id_{1};
  CandidateSnapshotState snapshot_state_{CandidateSnapshotState::None};
  std::vector<CandidateSlot> candidates_;
  bool learning_available_{true};
};

bool is_valid_utf8(std::string_view text);

}  // namespace hanime
