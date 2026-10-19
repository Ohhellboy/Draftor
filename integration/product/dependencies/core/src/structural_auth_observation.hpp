#pragma once
// Private Host-only counters; read-only evidence, no trust/budget decisions.
#include "hanime/verified_resource.hpp"
namespace hanime::detail {
struct StructuralAuthStats {
 std::uint64_t hash_update_calls{0},hash_update_bytes{0},hash_prefix_bytes{0},hash_data_bytes{0},hash_proof_bytes{0};
 std::uint64_t hash_final_calls{0},hash_compression_blocks{0};
 std::uint64_t lower_read_calls{0},lower_data_bytes{0},lower_proof_bytes{0};
 std::uint64_t packet_cache_copy_bytes{0},caller_copy_bytes{0},cache_only_copy_bytes{0},proof_digest_copy_bytes{0};
 std::uint64_t padding_checked_bytes{0};
};
StructuralAuthStats structural_auth_stats(ResourceReleaseRole);
}
