#pragma once
#include "hanime/core.hpp"
#include <array>

#define HANIME_VERIFICATION_FORMAT_MAX 2

namespace hanime {

using Sha256Digest = std::array<std::uint8_t, 32>;
// Portable FIPS 180-4 SHA-256. final() returns a digest without changing *this.
class Sha256 {
 public:
  Sha256();
  void update(const void*, std::size_t);
  Sha256Digest final() const;
 private:
  void compress(const std::uint8_t*);
  std::array<std::uint32_t, 8> state_;
  std::array<std::uint8_t, 64> buffer_{};
  std::uint64_t bytes_{0};
  std::size_t filled_{0};
};
Sha256Digest sha256(const void*, std::size_t);
Sha256Digest merkle_leaf_hash(std::uint64_t block, const void*, std::size_t bytes);
Sha256Digest merkle_page_hash(std::uint32_t level, std::uint32_t page, const void* bytes1024);

constexpr std::size_t kVerifiedBlockBytes = 1024;
constexpr std::size_t kVerifiedIndexPageBytes = 1024;
constexpr std::size_t kVerifiedFanout = 32;
constexpr std::size_t kVerifiedMaxLevels = 6;
constexpr std::uint32_t kResourceVerificationRule = 1;
enum class ResourceReleaseRole : std::uint8_t { Shared = 1, Scheme = 2, BaseOrder = 3 };

// TRUSTED INPUT, copied into the reader. The host publisher emits this record
// only after validating the final serialized files. The adapter pins this
// entire value in trusted firmware, NOT by loading an untrusted same-card file.
// Changing a release pin requires an explicit firmware update in this version.
struct ResourceReleaseSpec {
  std::uint32_t rule_version{kResourceVerificationRule};
  std::uint32_t resource_format{2};
  std::uint32_t key_format{kStableKeyFormat};
  ResourceReleaseRole role{ResourceReleaseRole::Shared};
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId shared_bundle_id{0};
  std::uint64_t file_bytes{0};
  Sha256Digest file_sha256{};
  Sha256Digest merkle_root{};
  Sha256Digest required_shared_sha256{}; // Scheme exact dependency; Shared zero.
  RuntimeLexemeRef runtime_ref_count{0};
  std::uint32_t text_capacity{64}, name_capacity{128};
  std::array<char, 32> scheme_id{}; // NUL-terminated; Shared uses all zeroes.
  std::uint64_t bootstrap_root_offset{0};
  std::uint32_t bootstrap_root_bytes{0};
  Sha256Digest required_scheme_sha256{}; // BaseOrder only; exact V2 source.
  std::uint64_t ranking_rule_version{0}; // BaseOrder format 1 uses BaseOnly rule 1.
  std::uint32_t verification_format{1}; // 1: separate Merkle pages; 2: PackedAuth.
};
// Format 2 is a physical transport of the SAME logical resource bytes. Its
// trusted pin still binds the full logical SHA and all semantic dependencies.
constexpr std::size_t kPackedDataBytes = 512;
constexpr std::uint32_t kPackedMaxDepth = 32;
struct PackedMerkleLayout {
  std::uint64_t data_blocks{0}, padded_leaves{0}, file_bytes{0};
  std::uint32_t depth{0};
  std::size_t packet_bytes{0};
};
bool packed_merkle_layout(std::uint64_t logical_bytes, PackedMerkleLayout*);
Sha256Digest packed_leaf_hash(std::uint64_t block, const void*, std::size_t);
Sha256Digest packed_parent_hash(std::uint32_t level, std::uint64_t node,
                                const Sha256Digest& left, const Sha256Digest& right);
struct MerkleLayout {
  std::uint64_t data_blocks{0};
  std::uint32_t levels{0};
  std::array<std::uint32_t, kVerifiedMaxLevels> page_counts{};
  std::array<std::uint64_t, kVerifiedMaxLevels> offsets{};
  std::uint64_t index_bytes{0};
};
bool merkle_layout(std::uint64_t file_bytes, MerkleLayout*);
struct VerifiedResourceConfig {
  // 1 KiB payload units in both formats; format 2 uses twice as many 512 B slots.
  std::size_t data_cache_blocks{16};
  // Format 1 LRU pages. Format 2 reserves only its required packet scratch
  // within this KiB limit, with no proof-page cache or whole-file directory.
  std::size_t index_cache_pages{8};
  std::size_t max_single_read_bytes{1024};
  // Optional format-2 direct-mapped cache of authenticated level-8 subtree
  // roots (<=64 slots). No new trust source or wire bytes; zero preserves the
  // original complete proof path. Shorter trees need no allocation.
  std::size_t packed_anchor_slots{0};
};
enum class VerifiedResourceStatus : std::uint8_t { Pending, Ready, Error, Closed };
struct VerifiedResourceSnapshot {
  VerifiedResourceStatus status{VerifiedResourceStatus::Pending};
  std::uint64_t authenticated_loads{0}; // Includes re-loads; not unique coverage.
  std::uint64_t authenticated_index_loads{0};
  std::uint64_t total_blocks{0};
  std::uint64_t logical_requests{0}, logical_bytes{0}; // Started budgeted requests, not Pending ticks.
  std::uint64_t cache_only_requests{0}, cache_only_bytes{0}; // Fixed-open read(), no I/O.
  std::uint64_t data_cache_hits{0}, index_cache_hits{0}; // Block/page lookups, not byte counts.
  std::size_t resident_verified{0}, resident_verified_index_pages{0};
  bool whole_file_checked{false}; // No visited bitset/full-file claim in this reader.
  WorkCost cost;
  std::uint64_t authenticated_packet_loads{0}; // Format 2; not index-page loads.
  std::size_t data_block_bytes{kVerifiedBlockBytes};
  std::uint64_t anchor_hits{0}, anchor_misses{0}, anchor_inserts{0}, proof_bytes_saved{0};
};
struct VerifiedResourceMemory {
  std::size_t owner_fixed_bytes{0};
  std::size_t data_payload_bytes{0}, index_payload_bytes{0};
  std::size_t data_metadata_bytes{0}, index_metadata_bytes{0};
  std::size_t hash_state_bytes{0}; // Included in owner_fixed_bytes.
  std::size_t anchor_bytes{0};
};

// Single serial read owner. Borrowed destinations remain exclusive until Ready
// or replacement/cancellation. Pending may have written AUTHENTICATED partial
// bytes, which must not be consumed until Ready. A different opaque owner token,
// revision or offset/count/destination replaces the old operation. Tokens are
// retained until Ready/cancel/failure, preventing reused-address resurrection.
// Data and index files must remain immutable until this reader is deactivated.
// Failure is latched, including underlying unhealthiness. New activation uses
// a NEW reader; deactivation cannot be reversed.
// With verification_format=2, index is the packed authenticated transport;
// data may be null. No logical byte is read from data in this mode.
class VerifiedResourceReader final : public ResourceReader {
 public:
  VerifiedResourceReader(std::shared_ptr<const ResourceReader> data,
                         std::shared_ptr<const ResourceReader> index,
                         const ResourceReleaseSpec&, const VerifiedResourceConfig& = {});
  ~VerifiedResourceReader() override;
  VerifiedResourceReader(const VerifiedResourceReader&) = delete;
  VerifiedResourceReader& operator=(const VerifiedResourceReader&) = delete;
  std::size_t size() const override;
  bool healthy() const override;
  // CACHE ONLY: zero I/O, succeeds only when every requested byte is resident
  // and authenticated. Used by existing fixed-header/root open after bootstrap.
  bool read(std::size_t offset, void*, std::size_t bytes) const override;
  ResourceReadStep read_budgeted(std::size_t offset, void*, std::size_t bytes,
                                const WorkBudget&, const ResourceReadContext& = {}) const override;
  void cancel_read_budgeted(const ResourceReadContext&) const override;
  bool prepare_bootstrap(); // No I/O, fixed work, idempotent after ready.
  ResourceReadStep advance_bootstrap(const WorkBudget&);
  bool bootstrap_ready() const;
  const ResourceReleaseSpec& spec() const;
  // Legacy format 1 only (zero layout for format 2). For format 2 use
  // packed_merkle_layout(spec().file_bytes, ...); this requires no I/O.
  const MerkleLayout& layout() const;
  VerifiedResourceSnapshot snapshot() const; // Read-only; no I/O or advancement.
  VerifiedResourceMemory memory_usage() const;
  const char* error() const;
  void deactivate();
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace hanime
