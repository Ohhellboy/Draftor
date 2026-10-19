#pragma once

#include "hanime/core.hpp"
#include <array>

#define HANIME_OUTPUT_REUSE_API 1
#ifndef HANIME_TEXT_CHAIN
#define HANIME_TEXT_CHAIN 0
#endif

namespace hanime {
class VerifiedResourceReader;
class OutputReader;
namespace detail { class OutputReuseCache; struct SharedStructuralAccess; }

// Optional completed-output reuse, owned by one shared activation. Zero entries
// disables it. Longer results bypass this pool without limiting candidates.
struct OutputReuseConfig {
  std::size_t entries{0}, text_bytes{64}, form_bytes{64};
};
struct OutputReuseStats {
  std::size_t entries{0}, resident{0};
  std::size_t fixed_bytes{0}, metadata_bytes{0}, text_bytes{0}, form_bytes{0};
  std::uint64_t lookups{0}, hits{0}, misses{0}, inserts{0}, evictions{0};
  std::uint64_t copied_bytes{0}, stored_bytes{0};
};

constexpr std::uint32_t kNoSharedString = 0xffffffffU;
enum class SharedRecordKind : std::uint8_t { Lexeme, Relation, Selector, StringDescriptor };

struct SharedLexemeRecord {
  RuntimeLexemeInfo identity;
  std::uint32_t default_payload{kNoSharedString};
  std::uint32_t relation_start{0};
  std::uint32_t relation_count{0};
};
struct SharedOutputRecord {
  std::uint32_t selector{0};
  std::uint32_t target_payload{kNoSharedString};
  std::uint32_t condition_descriptor{kNoSharedString};
  ConditionKind condition_kind{ConditionKind::None};
  std::uint8_t availability{0};
  OutputAdmission admission{OutputAdmission::Ready};
  bool preferred{false};
};
struct SharedSelectorRecord {
  std::uint32_t name{0};
  std::uint32_t selection_basis{kNoSharedString};
};
struct SharedStringDescriptor {
  std::uint32_t offset{0};  // Relative to the immutable payload section.
  std::uint32_t length{0};
};
struct SharedSectionInfo {
  std::uint64_t offset{0};
  std::uint32_t count{0};
  std::uint32_t record_bytes{0};
  std::uint32_t page_count{0};
};
struct PagedSharedInfo {
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId shared_bundle_id{0};
  std::uint32_t key_format{kStableKeyFormat};
  std::uint32_t page_bytes{0};
  std::array<SharedSectionInfo, 5> sections{};
  std::uint64_t payload_offset{0}, payload_bytes{0};
  std::size_t file_bytes{0};
  WorkCost open_cost;
  WorkCost uniqueness_cost;  // Actual I/O/work in the auxiliary-reference phase.
};
struct PagedSharedConfig {
  std::size_t page_cache_capacity{2};
  std::size_t max_single_read_bytes{256};
  // Opt-in for an authenticated release ONLY: use the authenticated global
  // layout to locate the needed fixed record, and authenticate its bytes.
  // Redundant physical page headers are not used for this address calculation.
  // The complete validator still checks every page header. Same allocation;
  // unused bytes are not claimed as checked, and no text gains selection rights.
  bool verified_record_slices{false};
  OutputReuseConfig output_reuse{};
};

struct SharedActivationIdentity;
// Read-only snapshot of one ready activation. Copies share the existing token;
// no reader/cache ownership or new identity allocation. Retain once per query,
// not per hit. A default handle is never current.
class SharedActivationHandle {
 public:
  SharedActivationHandle() = default;
  MountEpoch mount_epoch() const noexcept { return mount_epoch_; }
  RegistryNamespaceId registry_namespace() const noexcept { return registry_namespace_; }
  SharedBundleId shared_bundle_id() const noexcept { return shared_bundle_id_; }

 private:
  friend class PagedSharedResource;
  std::shared_ptr<const SharedActivationIdentity> identity_;
  MountEpoch mount_epoch_{0};
  RegistryNamespaceId registry_namespace_{0};
  SharedBundleId shared_bundle_id_{0};
};
struct SharedReadContext {
  std::shared_ptr<const SharedActivationIdentity> identity;
  MountEpoch mount_epoch{0};
  std::uint64_t operation_revision{0};
};
class SharedValidationCursor {
 public:
  SharedValidationCursor() = default;
 private:
  friend class PagedSharedResource;
  SharedReadContext context_;
};
class SharedReadCursor {
 public:
  SharedReadCursor() = default;
 private:
  friend class PagedSharedResource;
  SharedReadContext context_;
  bool cancelled_{false};
};
enum class SharedReadStatus : std::uint8_t {
  InProgress, Ready, NotReady, InvalidReference, InvalidBudget,
  Cancelled, Stale, Error, CapacityExceeded
};
// Shares the owner's ONE operation slot with metadata reads. Destination must
// stay alive/exclusive until cancellation or completion; no partial text view.
class SharedTextCursor {
 public:
  SharedTextCursor() = default;
 private:
  friend class PagedSharedResource;
  SharedReadCursor descriptor_;
  char* destination_{nullptr};
  std::size_t capacity_{0}, filled_{0};
  SharedStringDescriptor span_;
  bool described_{false};
};
struct SharedTextResult {
  SharedReadStatus status{SharedReadStatus::Stale};
  SharedReadContext context;
  std::string_view text; // Ready only. Borrowed from caller's destination.
  std::size_t required_bytes{0};
  std::size_t copied_bytes{0}; // Payload bytes only, excludes descriptor/page I/O.
  WorkCost cost;
};
// Fixed-size owned values, not borrowed page/text views. Caller bounds retained
// results; the owner retains no result history. Lexeme includes its default span.
struct SharedReadResult {
  SharedReadStatus status{SharedReadStatus::Error};
  SharedReadContext context;
  SharedRecordKind kind{SharedRecordKind::Lexeme};
  SharedLexemeRecord lexeme;
  SharedOutputRecord relation;
  SharedSelectorRecord selector;
  SharedStringDescriptor string_descriptor;
  WorkCost cost;
  std::string error;
};
struct SharedMemoryUsage {
  std::size_t owner_fixed_bytes{0};
  std::size_t cache_payload_allocated_bytes{0};
  std::size_t cache_metadata_allocated_bytes{0};
  std::size_t valid_cached_pages{0};
  std::size_t stream_scratch_bytes{0};  // Included in owner_fixed_bytes.
};

class PagedSharedResource {
 public:
  PagedSharedResource();
  ~PagedSharedResource();
  PagedSharedResource(const PagedSharedResource&) = delete;
  PagedSharedResource& operator=(const PagedSharedResource&) = delete;
  // expected_bundle_id=0 permits discovering a new immutable shared package.
  // The returned identity must then bind every active scheme index.
  bool open(std::shared_ptr<const ResourceReader> reader,
            RegistryNamespaceId expected_namespace,
            SharedBundleId expected_bundle_id, MountEpoch mount_epoch,
            const PagedSharedConfig& config, std::string* error);
  bool open_verified(std::shared_ptr<const VerifiedResourceReader>, MountEpoch,
                     const PagedSharedConfig&, std::string* error);
  const struct ResourceReleaseSpec* verified_release() const;
  SharedValidationCursor validation_cursor() const;
  ValidationStepResult advance_validation(const SharedValidationCursor& cursor,
                                          const WorkBudget& budget);
  void cancel_validation(const SharedValidationCursor& cursor);
  bool ready() const;
  // Fixed-cost, no resource I/O, no query/validation/read state changes.
  // Serial host calls only, as with the other resource APIs.
  SharedActivationHandle activation_handle() const;
  bool activation_is_current(const SharedActivationHandle& handle) const;
  const PagedSharedInfo& info() const;
  SharedMemoryUsage memory_usage() const;
  OutputReuseStats output_reuse_stats() const; // Bounded inspection, no I/O.

  // Lexeme index is a 1-based RuntimeLexemeRef; other indexes are zero based.
  // Only one current operation; a successful new begin invalidates the old one.
  SharedReadStatus begin_read(SharedRecordKind kind, std::uint32_t index,
                             SharedReadCursor* cursor);
  SharedReadResult advance_read(SharedReadCursor* cursor, const WorkBudget& budget);
  void cancel_read(SharedReadCursor* cursor);
  bool result_is_current(const SharedReadResult& result) const;
  SharedReadStatus begin_payload_read(const SharedActivationHandle&, std::uint32_t string_id,
                                      char* destination, std::size_t capacity, SharedTextCursor*);
  SharedTextResult advance_payload_read(SharedTextCursor*, const WorkBudget&);
  void cancel_payload_read(SharedTextCursor*);
  bool text_is_current(const SharedTextResult&) const;
  void deactivate();

 private:
  struct Impl;
  friend class OutputReader;
  friend struct detail::SharedStructuralAccess;
  detail::OutputReuseCache* output_reuse_cache() const;
#if HANIME_TEXT_CHAIN
  // Only the resolver may transfer the owner's completed default descriptor.
  // No caller-supplied descriptor or public authenticated-span capability.
  SharedReadStatus begin_default_payload_read(const SharedActivationHandle&,
      const SharedReadCursor&, RuntimeLexemeRef, std::uint32_t string_id,
      char* destination, std::size_t capacity, SharedTextCursor*);
#endif
  std::unique_ptr<Impl> impl_;
};

}  // namespace hanime
