#include "wubi_auto_safe.hpp"

namespace ime_device {
bool sameRelease(const hanime::ResourceReleaseSpec& a, const hanime::ResourceReleaseSpec& b) {
    return a.rule_version == b.rule_version && a.resource_format == b.resource_format &&
        a.key_format == b.key_format && a.role == b.role &&
        a.registry_namespace == b.registry_namespace && a.shared_bundle_id == b.shared_bundle_id &&
        a.file_bytes == b.file_bytes && a.file_sha256 == b.file_sha256 && a.merkle_root == b.merkle_root &&
        a.required_shared_sha256 == b.required_shared_sha256 && a.runtime_ref_count == b.runtime_ref_count &&
        a.text_capacity == b.text_capacity && a.name_capacity == b.name_capacity && a.scheme_id == b.scheme_id &&
        a.bootstrap_root_offset == b.bootstrap_root_offset && a.bootstrap_root_bytes == b.bootstrap_root_bytes &&
        a.required_scheme_sha256 == b.required_scheme_sha256 && a.ranking_rule_version == b.ranking_rule_version &&
        a.verification_format == b.verification_format;
}

bool wubiAutoSafe(std::string_view code, const WubiAutoSafe* metadata,
                  const hanime::ResourceReleaseSpec* shared,
                  const hanime::ResourceReleaseSpec* scheme,
                  const hanime::ResourceReleaseSpec* order) {
    if (code.size() != 4 || !metadata || !metadata->bits || metadata->bytes != (390625u+7u)/8u ||
        !metadata->shared || !metadata->scheme || !metadata->order || !shared || !scheme || !order ||
        !sameRelease(*metadata->shared, *shared) || !sameRelease(*metadata->scheme, *scheme) ||
        !sameRelease(*metadata->order, *order)) return false;
    uint32_t index = 0;
    for (const char c : code) {
        if (c < 'a' || c > 'y') return false;
        index = index * 25 + uint32_t(c - 'a');
    }
    return (metadata->bits[index/8] & (1u << (index%8))) != 0;
}
} // namespace ime_device
