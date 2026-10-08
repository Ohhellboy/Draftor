#include "hanime/core.hpp"
#include "hanime/paged_shared_resource.hpp"
#include "hanime/verified_resource.hpp"

#include "paged_scheme_index_internal.hpp"
#include "hanime/base_order.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace hanime {

struct QueryIndexIdentity {};

namespace {

constexpr std::uint32_t kPagedSchemeMagic = 0x324d4353U;
constexpr std::uint32_t kPagedSchemeVersion = 2U;
constexpr std::uint32_t kSuperblockMagic = 0x32584749U;
constexpr std::uint32_t kPageMagic = 0x32475049U;
constexpr std::size_t kCommonHeaderBytes = 40U;
constexpr std::size_t kSuperblockBytes = 64U;
constexpr std::size_t kPageHeaderBytes = 16U;
constexpr std::uint32_t kNoPage = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr std::uint32_t kMinimumPageBytes = 64U;
constexpr std::uint32_t kMaximumPageBytes = 64U * 1024U;
constexpr std::uint32_t kMaximumSchemeIdBytes = 1024U;
constexpr std::uint32_t kMaximumPages = 1'000'000U;
constexpr std::uint32_t kMaximumMappings = 1'000'000U;

void hash_bytes(std::uint64_t* hash, const std::uint8_t* bytes,
                std::size_t size) {
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= kFnvPrime;
  }
}

void put_u16(std::vector<std::uint8_t>* bytes, std::uint16_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value & 0xffU));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void put_u32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32U; shift += 8U) {
    bytes->push_back(
        static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

void put_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
  put_u32(bytes, static_cast<std::uint32_t>(value));
}

void put_u64(std::vector<std::uint8_t>* bytes, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64U; shift += 8U) {
    bytes->push_back(
        static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

bool get_u16(const std::uint8_t* bytes, std::size_t size,
             std::size_t offset, std::uint16_t* output) {
  if (offset > size || size - offset < 2U) {
    return false;
  }
  *output = static_cast<std::uint16_t>(bytes[offset]) |
            static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U;
  return true;
}

bool get_u32(const std::uint8_t* bytes, std::size_t size,
             std::size_t offset, std::uint32_t* output) {
  if (offset > size || size - offset < 4U) {
    return false;
  }
  *output = static_cast<std::uint32_t>(bytes[offset]) |
            static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U |
            static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U |
            static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U;
  return true;
}

bool get_i32(const std::uint8_t* bytes, std::size_t size,
             std::size_t offset, std::int32_t* output) {
  std::uint32_t value = 0;
  if (!get_u32(bytes, size, offset, &value)) {
    return false;
  }
  *output = static_cast<std::int32_t>(value);
  return true;
}

bool get_u64(const std::uint8_t* bytes, std::size_t size,
             std::size_t offset, std::uint64_t* output) {
  if (offset > size || size - offset < 8U) {
    return false;
  }
  std::uint64_t value = 0;
  for (unsigned index = 0; index < 8U; ++index) {
    value |= static_cast<std::uint64_t>(bytes[offset + index])
             << (index * 8U);
  }
  *output = value;
  return true;
}

int compare_code(std::string_view left, std::string_view right) {
  const std::size_t common = std::min(left.size(), right.size());
  for (std::size_t index = 0; index < common; ++index) {
    const auto lhs = static_cast<std::uint8_t>(left[index]);
    const auto rhs = static_cast<std::uint8_t>(right[index]);
    if (lhs < rhs) {
      return -1;
    }
    if (lhs > rhs) {
      return 1;
    }
  }
  if (left.size() < right.size()) {
    return -1;
  }
  if (left.size() > right.size()) {
    return 1;
  }
  return 0;
}

std::size_t align_up(std::size_t value, std::size_t alignment) {
  const std::size_t remainder = value % alignment;
  return remainder == 0U ? value : value + (alignment - remainder);
}

bool checked_add(std::size_t left, std::size_t right, std::size_t* output) {
  if (right > std::numeric_limits<std::size_t>::max() - left) {
    return false;
  }
  *output = left + right;
  return true;
}

bool checked_multiply(std::size_t left, std::size_t right,
                      std::size_t* output) {
  if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
    return false;
  }
  *output = left * right;
  return true;
}

bool valid_budget(const WorkBudget& budget) {
  return budget.max_read_calls != 0U && budget.max_read_bytes != 0U &&
         budget.max_work_items != 0U;
}

struct CommonHeader {
  RegistryNamespaceId registry_namespace{0};
  SharedBundleId shared_bundle_id{0};
  std::uint64_t body_hash{0};
  std::size_t body_bytes{0};
};

bool parse_common_header(const std::uint8_t* bytes, std::size_t size,
                         CommonHeader* output, std::string* error) {
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint64_t body_bytes = 0;
  if (size != kCommonHeaderBytes || !get_u32(bytes, size, 0U, &magic) ||
      !get_u32(bytes, size, 4U, &version) ||
      !get_u64(bytes, size, 8U, &output->registry_namespace) ||
      !get_u64(bytes, size, 16U, &output->shared_bundle_id) ||
      !get_u64(bytes, size, 24U, &output->body_hash) ||
      !get_u64(bytes, size, 32U, &body_bytes)) {
    *error = "paged scheme header is truncated";
    return false;
  }
  if (magic != kPagedSchemeMagic || version != kPagedSchemeVersion) {
    *error = "paged scheme magic or version mismatch";
    return false;
  }
  if (body_bytes > std::numeric_limits<std::size_t>::max()) {
    *error = "paged scheme body is too large";
    return false;
  }
  output->body_bytes = static_cast<std::size_t>(body_bytes);
  return true;
}

struct Superblock {
  std::uint32_t page_bytes{0};
  std::uint32_t page_count{0};
  std::uint32_t root_page{0};
  std::uint32_t tree_depth{0};
  std::uint32_t mapping_count{0};
  std::uint32_t leaf_page_count{0};
  std::uint32_t scheme_id_bytes{0};
  std::size_t pages_offset{0};
};

bool parse_superblock(const std::uint8_t* bytes, std::size_t size,
                      Superblock* output, std::string* error) {
  std::uint32_t magic = 0;
  std::uint64_t pages_offset = 0;
  if (size != kSuperblockBytes || !get_u32(bytes, size, 0U, &magic) ||
      !get_u32(bytes, size, 4U, &output->page_bytes) ||
      !get_u32(bytes, size, 8U, &output->page_count) ||
      !get_u32(bytes, size, 12U, &output->root_page) ||
      !get_u32(bytes, size, 16U, &output->tree_depth) ||
      !get_u32(bytes, size, 20U, &output->mapping_count) ||
      !get_u32(bytes, size, 24U, &output->leaf_page_count) ||
      !get_u32(bytes, size, 28U, &output->scheme_id_bytes) ||
      !get_u64(bytes, size, 32U, &pages_offset)) {
    *error = "paged scheme superblock is truncated";
    return false;
  }
  if (magic != kSuperblockMagic || output->page_bytes < kMinimumPageBytes ||
      output->page_bytes > kMaximumPageBytes || output->page_count == 0U ||
      output->page_count > kMaximumPages ||
      output->root_page >= output->page_count ||
      output->tree_depth == 0U || output->mapping_count == 0U ||
      output->mapping_count > kMaximumMappings ||
      output->leaf_page_count == 0U ||
      output->leaf_page_count > output->page_count ||
      output->scheme_id_bytes == 0U ||
      output->scheme_id_bytes > kMaximumSchemeIdBytes ||
      pages_offset > std::numeric_limits<std::size_t>::max()) {
    *error = "paged scheme superblock is invalid";
    return false;
  }
  output->pages_offset = static_cast<std::size_t>(pages_offset);
  return true;
}

enum class PageKind : std::uint8_t { Leaf = 1U, Internal = 2U };

struct PageHeader {
  PageKind kind{PageKind::Leaf};
  std::uint8_t level{0};
  std::uint16_t record_count{0};
  std::uint32_t next_leaf{kNoPage};
  std::uint32_t used_bytes{0};
};

bool parse_page_header(const std::vector<std::uint8_t>& page,
                       PageHeader* output, std::string* error) {
  std::uint32_t magic = 0;
  std::uint16_t record_count = 0;
  std::uint32_t next_leaf = 0;
  std::uint32_t used_bytes = 0;
  if (page.size() < kPageHeaderBytes ||
      !get_u32(page.data(), page.size(), 0U, &magic) ||
      !get_u16(page.data(), page.size(), 6U, &record_count) ||
      !get_u32(page.data(), page.size(), 8U, &next_leaf) ||
      !get_u32(page.data(), page.size(), 12U, &used_bytes)) {
    *error = "index page header is truncated";
    return false;
  }
  const std::uint8_t kind = page[4U];
  if (magic != kPageMagic ||
      (kind != static_cast<std::uint8_t>(PageKind::Leaf) &&
       kind != static_cast<std::uint8_t>(PageKind::Internal)) ||
      record_count == 0U || used_bytes < kPageHeaderBytes ||
      used_bytes > page.size()) {
    *error = "index page header is invalid";
    return false;
  }
  output->kind = static_cast<PageKind>(kind);
  output->level = page[5U];
  output->record_count = record_count;
  output->next_leaf = next_leaf;
  output->used_bytes = used_bytes;
  if ((output->kind == PageKind::Leaf && output->level != 0U) ||
      (output->kind == PageKind::Internal && output->level == 0U)) {
    *error = "index page kind and level disagree";
    return false;
  }
  return true;
}

bool read_code(const std::vector<std::uint8_t>& page, std::size_t used_bytes,
               std::size_t* offset, std::string_view* code,
               std::string* error) {
  std::uint16_t length = 0;
  if (!get_u16(page.data(), used_bytes, *offset, &length)) {
    *error = "index record code length is truncated";
    return false;
  }
  *offset += 2U;
  if (length == 0U || *offset > used_bytes ||
      length > used_bytes - *offset) {
    *error = "index record code is invalid";
    return false;
  }
  *code = std::string_view(
      reinterpret_cast<const char*>(page.data() + *offset), length);
  if (!is_valid_utf8(*code)) {
    *error = "index record code is invalid UTF-8";
    return false;
  }
  *offset += length;
  return true;
}

bool read_leaf_record(const std::vector<std::uint8_t>& page,
                      std::size_t used_bytes, std::size_t* offset,
                      std::string_view* code, RawQueryHit* hit,
                      std::string* error) {
  if (!read_code(page, used_bytes, offset, code, error) ||
      !get_u32(page.data(), used_bytes, *offset, &hit->runtime_ref) ||
      !get_i32(page.data(), used_bytes, *offset + 4U,
               &hit->scheme_weight) ||
      !get_u32(page.data(), used_bytes, *offset + 8U,
               &hit->mapping_ordinal)) {
    if (error->empty()) {
      *error = "leaf mapping record is truncated";
    }
    return false;
  }
  *offset += 12U;
  return true;
}

bool read_internal_record(const std::vector<std::uint8_t>& page,
                          std::size_t used_bytes, std::size_t* offset,
                          std::string_view* maximum_code,
                          std::uint32_t* child_page, std::string* error) {
  if (!read_code(page, used_bytes, offset, maximum_code, error) ||
      !get_u32(page.data(), used_bytes, *offset, child_page)) {
    if (error->empty()) {
      *error = "internal index record is truncated";
    }
    return false;
  }
  *offset += 4U;
  return true;
}

bool read_chunked(const ResourceReader& reader, std::size_t offset,
                  void* destination, std::size_t byte_count,
                  std::size_t max_single_read_bytes, WorkCost* cost) {
  auto* bytes = static_cast<std::uint8_t*>(destination);
  std::size_t done = 0;
  while (done < byte_count) {
    const std::size_t amount =
        std::min(byte_count - done, max_single_read_bytes);
    ++cost->read_calls;
    cost->read_bytes += amount;
    if (!reader.read(offset + done, bytes + done, amount)) {
      return false;
    }
    done += amount;
  }
  return true;
}

struct BuildPage {
  PageKind kind{PageKind::Leaf};
  std::uint8_t level{0};
  std::uint32_t next_leaf{kNoPage};
  std::uint16_t record_count{0};
  std::vector<std::uint8_t> records;
  std::string maximum_code;
};

std::vector<std::uint8_t> encode_page(const BuildPage& page,
                                      std::uint32_t page_bytes) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(page_bytes);
  put_u32(&bytes, kPageMagic);
  bytes.push_back(static_cast<std::uint8_t>(page.kind));
  bytes.push_back(page.level);
  put_u16(&bytes, page.record_count);
  put_u32(&bytes, page.next_leaf);
  put_u32(&bytes,
          static_cast<std::uint32_t>(kPageHeaderBytes + page.records.size()));
  bytes.insert(bytes.end(), page.records.begin(), page.records.end());
  bytes.resize(page_bytes, 0U);
  return bytes;
}

}  // namespace

namespace detail {

bool build_paged_scheme_bundle(
    std::string_view scheme_id, RegistryNamespaceId registry_namespace,
    SharedBundleId shared_bundle_id,
    const std::vector<PagedSchemeBuildMapping>& mappings,
    std::uint32_t page_bytes, std::vector<std::uint8_t>* output,
    PagedSchemeBuildStats* stats, std::string* error) {
  if (!output || !stats || !error || scheme_id.empty() ||
      !is_valid_utf8(scheme_id) ||
      scheme_id.size() > kMaximumSchemeIdBytes ||
      registry_namespace == 0U || shared_bundle_id == 0U ||
      mappings.empty() || mappings.size() > kMaximumMappings ||
      page_bytes < kMinimumPageBytes || page_bytes > kMaximumPageBytes) {
    if (error) {
      *error = "paged scheme build input is invalid";
    }
    return false;
  }

  std::vector<PagedSchemeBuildMapping> ordered = mappings;
  for (const PagedSchemeBuildMapping& mapping : ordered) {
    if (mapping.code.empty() || !is_valid_utf8(mapping.code) ||
        mapping.code.size() > std::numeric_limits<std::uint16_t>::max() ||
        mapping.runtime_ref == 0U) {
      *error = "paged scheme mapping is invalid";
      return false;
    }
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const PagedSchemeBuildMapping& left,
               const PagedSchemeBuildMapping& right) {
              const int code_order = compare_code(left.code, right.code);
              if (code_order != 0) {
                return code_order < 0;
              }
              if (left.runtime_ref != right.runtime_ref) {
                return left.runtime_ref < right.runtime_ref;
              }
              if (left.scheme_weight != right.scheme_weight) {
                return left.scheme_weight > right.scheme_weight;
              }
              return left.mapping_ordinal < right.mapping_ordinal;
            });

  std::vector<BuildPage> pages;
  std::size_t mapping_index = 0;
  while (mapping_index < ordered.size()) {
    BuildPage page;
    page.kind = PageKind::Leaf;
    while (mapping_index < ordered.size()) {
      const PagedSchemeBuildMapping& mapping = ordered[mapping_index];
      const std::size_t record_bytes = 2U + mapping.code.size() + 12U;
      if (kPageHeaderBytes + page.records.size() + record_bytes > page_bytes) {
        if (page.record_count == 0U) {
          *error = "scheme mapping cannot fit in the configured page size";
          return false;
        }
        break;
      }
      if (page.record_count == std::numeric_limits<std::uint16_t>::max()) {
        break;
      }
      put_u16(&page.records,
              static_cast<std::uint16_t>(mapping.code.size()));
      page.records.insert(page.records.end(), mapping.code.begin(),
                          mapping.code.end());
      put_u32(&page.records, mapping.runtime_ref);
      put_i32(&page.records, mapping.scheme_weight);
      put_u32(&page.records, mapping.mapping_ordinal);
      ++page.record_count;
      page.maximum_code = mapping.code;
      ++mapping_index;
    }
    pages.push_back(std::move(page));
  }

  const std::uint32_t leaf_page_count =
      static_cast<std::uint32_t>(pages.size());
  for (std::uint32_t index = 0; index < leaf_page_count; ++index) {
    pages[index].next_leaf =
        index + 1U < leaf_page_count ? index + 1U : kNoPage;
  }

  struct ChildDescriptor {
    std::string maximum_code;
    std::uint32_t page_id{0};
  };
  std::vector<ChildDescriptor> level_nodes;
  level_nodes.reserve(pages.size());
  for (std::uint32_t index = 0; index < leaf_page_count; ++index) {
    level_nodes.push_back({pages[index].maximum_code, index});
  }

  std::uint8_t level = 0U;
  while (level_nodes.size() > 1U) {
    if (level == std::numeric_limits<std::uint8_t>::max()) {
      *error = "paged scheme tree is too deep";
      return false;
    }
    ++level;
    std::vector<ChildDescriptor> parents;
    std::size_t child_index = 0;
    while (child_index < level_nodes.size()) {
      BuildPage page;
      page.kind = PageKind::Internal;
      page.level = level;
      while (child_index < level_nodes.size()) {
        const ChildDescriptor& child = level_nodes[child_index];
        const std::size_t record_bytes = 2U + child.maximum_code.size() + 4U;
        if (kPageHeaderBytes + page.records.size() + record_bytes >
            page_bytes) {
          if (page.record_count == 0U) {
            *error = "internal descriptor cannot fit in the page size";
            return false;
          }
          break;
        }
        if (page.record_count == std::numeric_limits<std::uint16_t>::max()) {
          break;
        }
        put_u16(&page.records,
                static_cast<std::uint16_t>(child.maximum_code.size()));
        page.records.insert(page.records.end(), child.maximum_code.begin(),
                            child.maximum_code.end());
        put_u32(&page.records, child.page_id);
        ++page.record_count;
        page.maximum_code = child.maximum_code;
        ++child_index;
      }
      if (pages.size() >= kMaximumPages) {
        *error = "paged scheme exceeds the page-count bound";
        return false;
      }
      const auto page_id = static_cast<std::uint32_t>(pages.size());
      pages.push_back(std::move(page));
      parents.push_back({pages.back().maximum_code, page_id});
    }
    level_nodes = std::move(parents);
  }

  const std::uint32_t root_page = level_nodes.front().page_id;
  const std::uint32_t tree_depth = static_cast<std::uint32_t>(level) + 1U;
  const std::size_t pages_offset =
      align_up(kSuperblockBytes + scheme_id.size(), page_bytes);
  std::size_t page_region_bytes = 0;
  std::size_t body_bytes = 0;
  if (!checked_multiply(pages.size(), page_bytes, &page_region_bytes) ||
      !checked_add(pages_offset, page_region_bytes, &body_bytes)) {
    *error = "paged scheme size overflows the host size type";
    return false;
  }

  std::vector<std::uint8_t> body;
  body.reserve(body_bytes);
  put_u32(&body, kSuperblockMagic);
  put_u32(&body, page_bytes);
  put_u32(&body, static_cast<std::uint32_t>(pages.size()));
  put_u32(&body, root_page);
  put_u32(&body, tree_depth);
  put_u32(&body, static_cast<std::uint32_t>(ordered.size()));
  put_u32(&body, leaf_page_count);
  put_u32(&body, static_cast<std::uint32_t>(scheme_id.size()));
  put_u64(&body, static_cast<std::uint64_t>(pages_offset));
  body.resize(kSuperblockBytes, 0U);
  body.insert(body.end(), scheme_id.begin(), scheme_id.end());
  body.resize(pages_offset, 0U);
  for (const BuildPage& page : pages) {
    const std::vector<std::uint8_t> encoded = encode_page(page, page_bytes);
    body.insert(body.end(), encoded.begin(), encoded.end());
  }

  std::uint64_t body_hash = kFnvOffset;
  hash_bytes(&body_hash, body.data(), body.size());
  std::vector<std::uint8_t> bundle;
  bundle.reserve(kCommonHeaderBytes + body.size());
  put_u32(&bundle, kPagedSchemeMagic);
  put_u32(&bundle, kPagedSchemeVersion);
  put_u64(&bundle, registry_namespace);
  put_u64(&bundle, shared_bundle_id);
  put_u64(&bundle, body_hash);
  put_u64(&bundle, static_cast<std::uint64_t>(body.size()));
  bundle.insert(bundle.end(), body.begin(), body.end());

  stats->page_bytes = page_bytes;
  stats->page_count = static_cast<std::uint32_t>(pages.size());
  stats->leaf_page_count = leaf_page_count;
  stats->tree_depth = tree_depth;
  *output = std::move(bundle);
  error->clear();
  return true;
}

bool decode_paged_scheme_bundle(const ResourceReader& reader,
                                DecodedPagedScheme* output,
                                std::string* error) {
  if (!output || !error || reader.size() < kCommonHeaderBytes) {
    if (error) {
      *error = "paged scheme compatibility decode input is invalid";
    }
    return false;
  }
  std::array<std::uint8_t, kCommonHeaderBytes> common_bytes{};
  if (!reader.read(0U, common_bytes.data(), common_bytes.size())) {
    *error = "paged scheme header read failed";
    return false;
  }
  CommonHeader common;
  if (!parse_common_header(common_bytes.data(), common_bytes.size(), &common,
                           error) ||
      common.body_bytes != reader.size() - kCommonHeaderBytes) {
    if (error->empty()) {
      *error = "paged scheme body size mismatch";
    }
    return false;
  }
  std::vector<std::uint8_t> body(common.body_bytes);
  if (!reader.read(kCommonHeaderBytes, body.data(), body.size())) {
    *error = "paged scheme body read failed";
    return false;
  }
  std::uint64_t actual_hash = kFnvOffset;
  hash_bytes(&actual_hash, body.data(), body.size());
  if (actual_hash != common.body_hash) {
    *error = "paged scheme body checksum mismatch";
    return false;
  }
  if (body.size() < kSuperblockBytes) {
    *error = "paged scheme superblock is truncated";
    return false;
  }
  Superblock superblock;
  if (!parse_superblock(body.data(), kSuperblockBytes, &superblock, error)) {
    return false;
  }
  std::size_t page_region_bytes = 0;
  std::size_t expected_body_bytes = 0;
  if (!checked_multiply(superblock.page_count, superblock.page_bytes,
                        &page_region_bytes) ||
      !checked_add(superblock.pages_offset, page_region_bytes,
                   &expected_body_bytes) ||
      expected_body_bytes != body.size() ||
      kSuperblockBytes + superblock.scheme_id_bytes >
          superblock.pages_offset) {
    *error = "paged scheme layout is invalid";
    return false;
  }
  output->scheme_id.assign(
      reinterpret_cast<const char*>(body.data() + kSuperblockBytes),
      superblock.scheme_id_bytes);
  if (!is_valid_utf8(output->scheme_id)) {
    *error = "paged scheme id is invalid UTF-8";
    return false;
  }
  output->mappings.clear();
  output->mappings.reserve(superblock.mapping_count);
  std::string previous_code;
  for (std::uint32_t page_id = 0; page_id < superblock.page_count;
       ++page_id) {
    const std::size_t offset =
        superblock.pages_offset +
        static_cast<std::size_t>(page_id) * superblock.page_bytes;
    std::vector<std::uint8_t> page(
        body.begin() + static_cast<std::ptrdiff_t>(offset),
        body.begin() + static_cast<std::ptrdiff_t>(offset +
                                                  superblock.page_bytes));
    PageHeader header;
    if (!parse_page_header(page, &header, error)) {
      return false;
    }
    if (page_id < superblock.leaf_page_count) {
      if (header.kind != PageKind::Leaf ||
          header.next_leaf !=
              (page_id + 1U < superblock.leaf_page_count ? page_id + 1U
                                                         : kNoPage)) {
        *error = "paged scheme leaf chain is invalid";
        return false;
      }
      std::size_t record_offset = kPageHeaderBytes;
      for (std::uint16_t record = 0; record < header.record_count; ++record) {
        std::string_view code;
        RawQueryHit hit;
        if (!read_leaf_record(page, header.used_bytes, &record_offset, &code,
                              &hit, error) ||
            (!previous_code.empty() &&
             compare_code(previous_code, code) > 0)) {
          if (error->empty()) {
            *error = "paged scheme mappings are not ordered";
          }
          return false;
        }
        previous_code.assign(code);
        output->mappings.push_back(
            {std::string(code), hit.runtime_ref, hit.scheme_weight,
             hit.mapping_ordinal});
      }
      if (record_offset != header.used_bytes) {
        *error = "paged scheme leaf has trailing record bytes";
        return false;
      }
    }
  }
  if (output->mappings.size() != superblock.mapping_count) {
    *error = "paged scheme mapping count disagrees with leaves";
    return false;
  }
  output->registry_namespace = common.registry_namespace;
  output->required_shared_bundle_id = common.shared_bundle_id;
  error->clear();
  return true;
}

}  // namespace detail

struct ExactQueryCursor::Impl {
  enum class Phase : std::uint8_t { Locate, Scan, Done, NoResults, Error };
  Impl()=default;
  Impl(Impl&&)=default;
  Impl& operator=(Impl&&)=default;
  std::weak_ptr<const ResourceReader> read_owner;
  ~Impl() {
    if (auto reader=read_owner.lock())
      reader->cancel_read_budgeted({context.index_identity,context.query_revision});
  }

  bool initialized{false};
  bool cancelled{false};
  QueryBatchContext context;
  std::string code;
  Phase phase{Phase::Error};
  std::uint32_t page_id{0};
  std::vector<std::uint8_t> page;
  std::size_t page_fill{0};
  std::size_t pending_read_amount{0};
  bool header_parsed{false};
  PageHeader header;
  std::uint16_t record_index{0};
  std::size_t record_offset{kPageHeaderBytes};
  bool found_any{false};
  std::string error;
};

ExactQueryMemoryUsage ExactQueryCursor::memory_usage() const {
  if (!impl_) return {};
  const auto address=reinterpret_cast<std::uintptr_t>(impl_->code.data());
  const auto object=reinterpret_cast<std::uintptr_t>(&impl_->code);
  const bool in_object=address>=object && address<object+sizeof(impl_->code);
  return {sizeof(Impl),impl_->code.capacity()+1,in_object ? 0 : impl_->code.capacity()+1,impl_->page.capacity()};
}

struct PagedSchemeIndex::Impl {
  enum class State : std::uint8_t { Closed, Validating, Ready, Error };
  enum class ValidationPhase : std::uint8_t { Hash, Pages };

  struct CacheEntry {
    std::uint32_t page_id{0};
    std::vector<std::uint8_t> bytes;
    std::uint64_t last_used{0};
  };

  State state{State::Closed};
  std::shared_ptr<const ResourceReader> reader;
  PagedSchemeIndexConfig config;
  PagedSchemeIndexInfo info;
  CommonHeader common;
  Superblock superblock;
  RuntimeLexemeRef maximum_runtime_ref{0};
  std::size_t absolute_pages_offset{0};
  std::uint64_t index_epoch{0};
  std::uint64_t query_revision{0};
  MountEpoch active_query_mount_epoch{0};
  std::shared_ptr<const QueryIndexIdentity> identity;
  const PagedSharedResource* verified_shared{nullptr};
  SharedActivationHandle verified_activation;
  std::shared_ptr<const VerifiedResourceReader> verified_reader;
  std::shared_ptr<BaseOrderIndex> base_order;

  ValidationPhase validation_phase{ValidationPhase::Hash};
  std::size_t validation_hash_offset{0};
  std::uint64_t validation_hash{kFnvOffset};
  std::uint32_t validation_page_id{0};
  std::vector<std::uint8_t> validation_page;
  std::size_t validation_page_fill{0};
  bool validation_header_parsed{false};
  PageHeader validation_header;
  std::uint16_t validation_record_index{0};
  std::size_t validation_record_offset{kPageHeaderBytes};
  std::uint64_t validated_leaf_records{0};
  std::string previous_leaf_code;
  RuntimeLexemeRef previous_leaf_ref{0};

  // Bottom-up consecutive levels certify coverage without a visited-page table.
  std::uint32_t validation_level{0};
  std::uint32_t level_start{0};
  std::uint32_t children_end{0};
  std::uint32_t next_child{0};
  std::string previous_fence;
  bool child_pending{false};
  std::string_view pending_fence; // view into the suspended parent page
  std::vector<std::uint8_t> child_page;
  std::size_t child_fill{0};
  bool child_header_parsed{false};
  PageHeader child_header;
  std::uint16_t child_record_index{0};
  std::size_t child_record_offset{kPageHeaderBytes};
  std::string_view child_maximum; // view into child_page, never escapes validation

  std::vector<CacheEntry> cache;
  std::uint64_t cache_clock{0};
  std::string error;

  void release_validation_buffers() {
    std::vector<std::uint8_t>().swap(validation_page);
    std::vector<std::uint8_t>().swap(child_page);
    std::string().swap(previous_leaf_code);
    std::string().swap(previous_fence);
    pending_fence = {};
    child_maximum = {};
  }

  bool resource_current() const {
    return state==State::Ready && reader && reader->healthy() &&
        (!verified_shared || verified_shared->activation_is_current(verified_activation));
  }
  bool context_is_current(const QueryBatchContext& context) const {
    return resource_current() && identity &&
           context.index_identity == identity &&
           context.index_epoch == index_epoch &&
           context.query_revision == query_revision &&
           context.mount_epoch == active_query_mount_epoch;
  }

  void fail(std::string message) {
    state = State::Error;
    error = std::move(message);
    release_validation_buffers();
    reader.reset();
  }

  const std::vector<std::uint8_t>* find_cached(std::uint32_t page_id) {
    for (CacheEntry& entry : cache) {
      if (entry.page_id == page_id) {
        entry.last_used = ++cache_clock;
        return &entry.bytes;
      }
    }
    return nullptr;
  }

  void insert_cache(std::uint32_t page_id,
                    const std::vector<std::uint8_t>& bytes) {
    if (config.page_cache_capacity == 0U) {
      return;
    }
    if (cache.size() == config.page_cache_capacity) {
      const auto oldest = std::min_element(
          cache.begin(), cache.end(),
          [](const CacheEntry& left, const CacheEntry& right) {
            return left.last_used < right.last_used;
          });
      *oldest = {page_id, bytes, ++cache_clock};
      return;
    }
    cache.push_back({page_id, bytes, ++cache_clock});
  }
};

ExactQueryCursor::ExactQueryCursor() : impl_(std::make_unique<Impl>()) {}
ExactQueryCursor::~ExactQueryCursor() = default;
ExactQueryCursor::ExactQueryCursor(ExactQueryCursor&&) noexcept = default;
ExactQueryCursor& ExactQueryCursor::operator=(ExactQueryCursor&&) noexcept =
    default;

PagedSchemeIndex::PagedSchemeIndex() : impl_(std::make_unique<Impl>()) {}
PagedSchemeIndex::~PagedSchemeIndex() { deactivate(); }

bool PagedSchemeIndex::open(
    std::shared_ptr<const ResourceReader> scheme_bundle,
    RegistryNamespaceId expected_registry_namespace,
    SharedBundleId expected_shared_bundle_id,
    RuntimeLexemeRef maximum_runtime_ref,
    const PagedSchemeIndexConfig& config, std::string* error) {
  std::string local_error;
  if (!error) {
    error = &local_error;
  }
  if (impl_->reader) impl_->reader->cancel_read_budgeted({impl_->identity,impl_->query_revision});
  ++impl_->index_epoch;
  impl_->query_revision = 0U;
  impl_->identity = std::make_shared<const QueryIndexIdentity>();
  impl_->active_query_mount_epoch = 0U;
  impl_->state = Impl::State::Closed;
  impl_->reader.reset();
  impl_->verified_shared=nullptr; impl_->verified_activation={};
  impl_->base_order.reset(); impl_->verified_reader.reset();
  impl_->release_validation_buffers();
  impl_->cache.clear();
  impl_->error.clear();
  impl_->info = {};
  if (!scheme_bundle || expected_registry_namespace == 0U ||
      expected_shared_bundle_id == 0U || maximum_runtime_ref == 0U ||
      config.page_cache_capacity == 0U ||
      config.max_single_read_bytes == 0U ||
      scheme_bundle->size() < kCommonHeaderBytes + kSuperblockBytes) {
    *error = "paged scheme open input is invalid";
    return false;
  }

  WorkCost initial_cost;
  std::array<std::uint8_t, kCommonHeaderBytes> common_bytes{};
  if (!read_chunked(*scheme_bundle, 0U, common_bytes.data(),
                    common_bytes.size(), config.max_single_read_bytes,
                    &initial_cost) ||
      !parse_common_header(common_bytes.data(), common_bytes.size(),
                           &impl_->common, error)) {
    return false;
  }
  if (impl_->common.body_bytes !=
      scheme_bundle->size() - kCommonHeaderBytes) {
    *error = "paged scheme body size mismatch";
    return false;
  }
  if (impl_->common.registry_namespace != expected_registry_namespace ||
      impl_->common.shared_bundle_id != expected_shared_bundle_id) {
    *error = "paged scheme is bound to another registry or shared bundle";
    return false;
  }

  std::array<std::uint8_t, kSuperblockBytes> superblock_bytes{};
  if (!read_chunked(*scheme_bundle, kCommonHeaderBytes,
                    superblock_bytes.data(), superblock_bytes.size(),
                    config.max_single_read_bytes, &initial_cost) ||
      !parse_superblock(superblock_bytes.data(), superblock_bytes.size(),
                        &impl_->superblock, error)) {
    return false;
  }
  std::size_t page_region_bytes = 0;
  std::size_t expected_body_bytes = 0;
  if (!checked_multiply(impl_->superblock.page_count,
                        impl_->superblock.page_bytes, &page_region_bytes) ||
      !checked_add(impl_->superblock.pages_offset, page_region_bytes,
                   &expected_body_bytes) ||
      expected_body_bytes != impl_->common.body_bytes ||
      kSuperblockBytes + impl_->superblock.scheme_id_bytes >
          impl_->superblock.pages_offset ||
      !checked_add(kCommonHeaderBytes, impl_->superblock.pages_offset,
                   &impl_->absolute_pages_offset)) {
    *error = "paged scheme layout is invalid";
    return false;
  }

  std::string scheme_id(impl_->superblock.scheme_id_bytes, '\0');
  if (!read_chunked(*scheme_bundle, kCommonHeaderBytes + kSuperblockBytes,
                    scheme_id.data(), scheme_id.size(),
                    config.max_single_read_bytes, &initial_cost) ||
      !is_valid_utf8(scheme_id)) {
    *error = "paged scheme id read failed or is invalid UTF-8";
    return false;
  }

  std::vector<std::uint8_t> root_page(impl_->superblock.page_bytes);
  const std::size_t root_offset =
      impl_->absolute_pages_offset +
      static_cast<std::size_t>(impl_->superblock.root_page) *
          impl_->superblock.page_bytes;
  if (!read_chunked(*scheme_bundle, root_offset, root_page.data(),
                    root_page.size(), config.max_single_read_bytes,
                    &initial_cost)) {
    *error = "paged scheme root page read failed";
    return false;
  }
  PageHeader root_header;
  if (!parse_page_header(root_page, &root_header, error) ||
      static_cast<std::uint32_t>(root_header.level) + 1U !=
          impl_->superblock.tree_depth ||
      (impl_->superblock.tree_depth == 1U &&
       root_header.kind != PageKind::Leaf) ||
      (impl_->superblock.tree_depth > 1U &&
       root_header.kind != PageKind::Internal)) {
    if (error->empty()) {
      *error = "paged scheme root does not match the declared depth";
    }
    return false;
  }

  impl_->reader = std::move(scheme_bundle);
  impl_->config = config;
  impl_->maximum_runtime_ref = maximum_runtime_ref;
  impl_->info.scheme_id = std::move(scheme_id);
  impl_->info.registry_namespace = impl_->common.registry_namespace;
  impl_->info.required_shared_bundle_id = impl_->common.shared_bundle_id;
  impl_->info.file_bytes = impl_->reader->size();
  impl_->info.page_bytes = impl_->superblock.page_bytes;
  impl_->info.page_count = impl_->superblock.page_count;
  impl_->info.leaf_page_count = impl_->superblock.leaf_page_count;
  impl_->info.tree_depth = impl_->superblock.tree_depth;
  impl_->info.mapping_count = impl_->superblock.mapping_count;
  impl_->info.initial_read_calls = initial_cost.read_calls;
  impl_->info.initial_read_bytes = initial_cost.read_bytes;
  impl_->info.root_offset = root_offset;
  impl_->validation_phase = Impl::ValidationPhase::Hash;
  impl_->validation_hash_offset = 0U;
  impl_->validation_hash = kFnvOffset;
  impl_->validation_page_id = 0U;
  impl_->validation_page.clear();
  impl_->validation_page_fill = 0U;
  impl_->validation_header_parsed = false;
  impl_->validation_record_index = 0U;
  impl_->validation_record_offset = kPageHeaderBytes;
  impl_->validated_leaf_records = 0U;
  impl_->previous_leaf_code.clear();
  impl_->previous_leaf_ref=0;
  impl_->validation_level = 0U;
  impl_->level_start = 0U;
  impl_->children_end = 0U;
  impl_->next_child = 0U;
  impl_->child_pending = false;
  impl_->state = Impl::State::Validating;
  error->clear();
  return true;
}

ValidationStepResult PagedSchemeIndex::advance_validation(
    const WorkBudget& budget) {
  ValidationStepResult result;
  if (!valid_budget(budget)) {
    result.status = ValidationStepStatus::InvalidBudget;
    result.error = "validation budget fields must all be non-zero";
    return result;
  }
  if (impl_->state == Impl::State::Ready) {
    result.status = ready() ? ValidationStepStatus::Ready : ValidationStepStatus::Error;
    if (!ready()) result.error="resource activation is no longer healthy";
    return result;
  }
  if (impl_->state != Impl::State::Validating || !impl_->reader) {
    result.status = ValidationStepStatus::Error;
    result.error = impl_->error.empty() ? "scheme index is not validating"
                                        : impl_->error;
    return result;
  }

  auto can_read = [&]() {
    return result.cost.read_calls < budget.max_read_calls &&
           result.cost.read_bytes < budget.max_read_bytes;
  };
  auto can_work = [&]() {
    return result.cost.work_items < budget.max_work_items;
  };

  while (true) {
    if (impl_->validation_phase == Impl::ValidationPhase::Hash) {
      if (impl_->validation_hash_offset == impl_->common.body_bytes) {
        if (impl_->validation_hash != impl_->common.body_hash) {
          impl_->fail("paged scheme body checksum mismatch");
          break;
        }
        impl_->validation_phase = Impl::ValidationPhase::Pages;
        continue;
      }
      if (!can_read() || !can_work()) {
        break;
      }
      const std::size_t remaining =
          impl_->common.body_bytes - impl_->validation_hash_offset;
      const std::size_t byte_budget =
          budget.max_read_bytes - result.cost.read_bytes;
      const std::size_t amount =
          std::min({remaining, byte_budget,
                    impl_->config.max_single_read_bytes,
                    static_cast<std::size_t>(impl_->superblock.page_bytes)});
      std::vector<std::uint8_t> bytes(amount);
      ++result.cost.read_calls;
      result.cost.read_bytes += amount;
      ++result.cost.work_items;
      if (!impl_->reader->read(kCommonHeaderBytes +
                                   impl_->validation_hash_offset,
                               bytes.data(), bytes.size())) {
        impl_->fail("paged scheme checksum read failed");
        break;
      }
      hash_bytes(&impl_->validation_hash, bytes.data(), bytes.size());
      impl_->validation_hash_offset += amount;
      continue;
    }

    // Each referenced child is read once more. Its own page was already
    // certified in the preceding level; its last fence is therefore the actual
    // subtree maximum. No recursion, tree-wide directory or visited set is needed.
    if (impl_->child_pending) {
      if (impl_->child_fill < impl_->child_page.size()) {
        if (!can_read()) {
          break;
        }
        const auto amount = std::min({
            impl_->child_page.size() - impl_->child_fill,
            budget.max_read_bytes - result.cost.read_bytes,
            impl_->config.max_single_read_bytes});
        ++result.cost.read_calls;
        result.cost.read_bytes += amount;
        if (!impl_->reader->read(
                impl_->absolute_pages_offset +
                    static_cast<std::size_t>(impl_->next_child) *
                        impl_->superblock.page_bytes + impl_->child_fill,
                impl_->child_page.data() + impl_->child_fill, amount)) {
          impl_->fail("paged scheme structural child read failed");
          break;
        }
        impl_->child_fill += amount;
        continue;
      }
      if (!can_work()) {
        break;
      }
      ++result.cost.work_items;
      std::string child_error;
      if (!impl_->child_header_parsed) {
        if (!parse_page_header(impl_->child_page, &impl_->child_header,
                               &child_error) ||
            static_cast<std::uint32_t>(impl_->child_header.level) + 1U !=
                impl_->validation_level) {
          impl_->fail(child_error.empty() ? "paged scheme child level is invalid"
                                           : std::move(child_error));
          break;
        }
        impl_->child_header_parsed = true;
        continue;
      }
      if (impl_->child_record_index < impl_->child_header.record_count) {
        std::string_view code;
        RawQueryHit hit;
        std::uint32_t child = 0U;
        const bool valid = impl_->child_header.kind == PageKind::Leaf
            ? read_leaf_record(impl_->child_page, impl_->child_header.used_bytes,
                               &impl_->child_record_offset, &code, &hit, &child_error)
            : read_internal_record(impl_->child_page, impl_->child_header.used_bytes,
                                   &impl_->child_record_offset, &code, &child, &child_error);
        if (!valid) {
          impl_->fail(std::move(child_error));
          break;
        }
        impl_->child_maximum = code;
        ++impl_->child_record_index;
        continue;
      }
      if (impl_->child_record_offset != impl_->child_header.used_bytes ||
          compare_code(impl_->pending_fence, impl_->child_maximum) != 0) {
        impl_->fail("paged scheme fence differs from child subtree maximum");
        break;
      }
      ++impl_->next_child;
      impl_->child_pending = false;
      continue;
    }

    if (impl_->validation_page_id == impl_->superblock.page_count) {
      if (!can_work()) {
        break;
      }
      ++result.cost.work_items;
      if (impl_->validated_leaf_records !=
          impl_->superblock.mapping_count) {
        impl_->fail("paged scheme leaf record total is invalid");
        break;
      }
      if (impl_->validation_level + 1U != impl_->superblock.tree_depth ||
          impl_->level_start != impl_->superblock.root_page ||
          impl_->level_start + 1U != impl_->superblock.page_count ||
          impl_->next_child != impl_->children_end) {
        impl_->fail("paged scheme root level or subtree coverage is incomplete");
        break;
      }
      impl_->state = Impl::State::Ready;
      impl_->release_validation_buffers();
      break;
    }

    if (impl_->validation_page.empty()) {
      impl_->validation_page.resize(impl_->superblock.page_bytes);
      impl_->validation_page_fill = 0U;
      impl_->validation_header_parsed = false;
      impl_->validation_record_index = 0U;
      impl_->validation_record_offset = kPageHeaderBytes;
    }
    if (impl_->validation_page_fill < impl_->validation_page.size()) {
      if (!can_read()) {
        break;
      }
      const std::size_t remaining =
          impl_->validation_page.size() - impl_->validation_page_fill;
      const std::size_t byte_budget =
          budget.max_read_bytes - result.cost.read_bytes;
      const std::size_t amount =
          std::min({remaining, byte_budget,
                    impl_->config.max_single_read_bytes});
      ++result.cost.read_calls;
      result.cost.read_bytes += amount;
      if (!impl_->reader->read(
              impl_->absolute_pages_offset +
                  static_cast<std::size_t>(impl_->validation_page_id) *
                      impl_->superblock.page_bytes +
                  impl_->validation_page_fill,
              impl_->validation_page.data() + impl_->validation_page_fill,
              amount)) {
        impl_->fail("paged scheme validation page read failed");
        break;
      }
      impl_->validation_page_fill += amount;
      continue;
    }
    if (!impl_->validation_header_parsed) {
      if (!can_work()) {
        break;
      }
      ++result.cost.work_items;
      std::string page_error;
      if (!parse_page_header(impl_->validation_page,
                             &impl_->validation_header, &page_error)) {
        impl_->fail(std::move(page_error));
        break;
      }
      const bool should_be_leaf =
          impl_->validation_page_id < impl_->superblock.leaf_page_count;
      if (should_be_leaf !=
          (impl_->validation_header.kind == PageKind::Leaf)) {
        impl_->fail("paged scheme page regions have invalid kinds");
        break;
      }
      if (should_be_leaf) {
        const std::uint32_t expected_next =
            impl_->validation_page_id + 1U <
                    impl_->superblock.leaf_page_count
                ? impl_->validation_page_id + 1U
                : kNoPage;
        if (impl_->validation_header.next_leaf != expected_next) {
          impl_->fail("paged scheme leaf chain is invalid");
          break;
        }
      } else {
        const auto level = static_cast<std::uint32_t>(impl_->validation_header.level);
        if (level != impl_->validation_level) {
          if (level != impl_->validation_level + 1U ||
              impl_->next_child != impl_->children_end) {
            impl_->fail("paged scheme level order or child coverage is invalid");
            break;
          }
          impl_->next_child = impl_->level_start;
          impl_->children_end = impl_->validation_page_id;
          impl_->level_start = impl_->validation_page_id;
          impl_->validation_level = level;
          impl_->previous_fence.clear();
        }
        if (impl_->validation_header.next_leaf != kNoPage) {
          impl_->fail("paged scheme internal page has a leaf link");
          break;
        }
      }
      impl_->validation_header_parsed = true;
      continue;
    }
    if (impl_->validation_record_index <
        impl_->validation_header.record_count) {
      if (!can_work()) {
        break;
      }
      ++result.cost.work_items;
      std::string record_error;
      if (impl_->validation_header.kind == PageKind::Leaf) {
        std::string_view code;
        RawQueryHit hit;
        if (!read_leaf_record(impl_->validation_page,
                              impl_->validation_header.used_bytes,
                              &impl_->validation_record_offset, &code, &hit,
                              &record_error) ||
            hit.runtime_ref == 0U ||
            hit.runtime_ref > impl_->maximum_runtime_ref ||
            (!impl_->previous_leaf_code.empty() &&
             (compare_code(impl_->previous_leaf_code, code) > 0 ||
              (compare_code(impl_->previous_leaf_code, code)==0 && hit.runtime_ref<impl_->previous_leaf_ref)))) {
          impl_->fail(record_error.empty()
                          ? "paged scheme leaf record is invalid"
                          : std::move(record_error));
          break;
        }
        impl_->previous_leaf_code.assign(code);
        impl_->previous_leaf_ref=hit.runtime_ref;
        ++impl_->validated_leaf_records;
      } else {
        std::string_view maximum_code;
        std::uint32_t child_page = 0;
        if (!read_internal_record(impl_->validation_page,
                                  impl_->validation_header.used_bytes,
                                  &impl_->validation_record_offset,
                                  &maximum_code, &child_page,
                                  &record_error) ||
            child_page != impl_->next_child ||
            child_page >= impl_->children_end ||
            (!impl_->previous_fence.empty() &&
             compare_code(impl_->previous_fence, maximum_code) > 0)) {
          impl_->fail(record_error.empty()
                          ? "paged scheme child coverage or fence order is invalid"
                          : std::move(record_error));
          break;
        }
        impl_->previous_fence.assign(maximum_code);
        impl_->pending_fence = maximum_code;
        impl_->child_page.resize(impl_->superblock.page_bytes);
        impl_->child_fill = 0U;
        impl_->child_header_parsed = false;
        impl_->child_record_index = 0U;
        impl_->child_record_offset = kPageHeaderBytes;
        impl_->child_maximum = {};
        impl_->child_pending = true;
      }
      ++impl_->validation_record_index;
      continue;
    }
    if (impl_->validation_record_offset !=
        impl_->validation_header.used_bytes) {
      impl_->fail("paged scheme page has trailing record bytes");
      break;
    }
    ++impl_->validation_page_id;
    impl_->validation_page.clear();
  }

  if (impl_->state == Impl::State::Ready) {
    result.status = ValidationStepStatus::Ready;
  } else if (impl_->state == Impl::State::Error) {
    result.status = ValidationStepStatus::Error;
    result.error = impl_->error;
  } else {
    result.status = ValidationStepStatus::InProgress;
  }
  return result;
}

bool PagedSchemeIndex::ready() const {
  return impl_->resource_current();
}

bool PagedSchemeIndex::open_verified(std::shared_ptr<const VerifiedResourceReader> reader,
    const PagedSharedResource& shared, const PagedSchemeIndexConfig& cfg, std::string* error) {
  const auto* dependency=shared.verified_release();
  if (!reader || !reader->bootstrap_ready() || !dependency ||
      reader->spec().role!=ResourceReleaseRole::Scheme ||
      reader->spec().required_shared_sha256!=dependency->file_sha256 ||
      reader->spec().registry_namespace!=dependency->registry_namespace ||
      reader->spec().shared_bundle_id!=dependency->shared_bundle_id ||
      reader->spec().runtime_ref_count!=dependency->runtime_ref_count) {
    deactivate(); if (error) *error="trusted scheme/shared dependency mismatch"; return false;
  }
  const auto& spec=reader->spec();
  if (!open(reader,spec.registry_namespace,spec.shared_bundle_id,spec.runtime_ref_count,cfg,error)) return false;
  if (info().scheme_id!=spec.scheme_id.data() || info().root_offset!=spec.bootstrap_root_offset ||
      info().page_bytes!=spec.bootstrap_root_bytes) {
    deactivate(); if (error) *error="trusted scheme metadata mismatch"; return false;
  }
  impl_->verified_shared=&shared; impl_->verified_activation=shared.activation_handle();
  impl_->verified_reader=reader;
  impl_->state=Impl::State::Ready; impl_->release_validation_buffers(); return true;
}

const PagedSchemeIndexInfo& PagedSchemeIndex::info() const {
  return impl_->info;
}

const ResourceReleaseSpec* PagedSchemeIndex::verified_release() const {
  return ready() && impl_->verified_reader ? &impl_->verified_reader->spec() : nullptr;
}
QueryBatchContext PagedSchemeIndex::activation_context() const {
  return {impl_->identity,0,impl_->index_epoch,0};
}
bool PagedSchemeIndex::activation_is_current(const QueryBatchContext& c) const {
  return ready() && c.index_identity && c.index_identity==impl_->identity && c.index_epoch==impl_->index_epoch;
}
bool PagedSchemeIndex::attach_base_order(std::shared_ptr<BaseOrderIndex> order) {
  if (order && !order->matches(*this)) return false;
  if (impl_->query_revision==std::numeric_limits<std::uint64_t>::max()) return false;
  if (impl_->reader) impl_->reader->cancel_read_budgeted({impl_->identity,impl_->query_revision});
  ++impl_->query_revision; impl_->base_order=std::move(order); return true;
}
std::shared_ptr<BaseOrderIndex> PagedSchemeIndex::base_order() const { return impl_->base_order; }

std::uint64_t PagedSchemeIndex::index_epoch() const {
  return impl_->index_epoch;
}

std::uint64_t PagedSchemeIndex::current_query_revision() const {
  return impl_->query_revision;
}

std::size_t PagedSchemeIndex::cached_page_count() const {
  return impl_->cache.size();
}

std::size_t PagedSchemeIndex::resident_page_bytes() const {
  return impl_->cache.size() * impl_->superblock.page_bytes;
}

bool PagedSchemeIndex::begin_exact_query(std::string code,
                                         MountEpoch mount_epoch,
                                         ExactQueryCursor* cursor,
                                         std::string* error) {
  std::string local_error;
  if (!error) {
    error = &local_error;
  }
  if (!cursor || !cursor->impl_ || !ready()) {
    *error = "paged scheme index is not ready";
    return false;
  }
  if (code.empty() || !is_valid_utf8(code) || mount_epoch == 0U) {
    *error = "complete code or mount epoch is invalid";
    return false;
  }
  if (impl_->verified_shared && mount_epoch!=impl_->verified_activation.mount_epoch()) {
    *error="query epoch does not match trusted shared activation"; return false;
  }
  if (impl_->query_revision == std::numeric_limits<std::uint64_t>::max()) {
    *error = "query revision exhausted; reopen the index";
    return false;
  }
  impl_->reader->cancel_read_budgeted({impl_->identity,impl_->query_revision});
  ++impl_->query_revision;
  impl_->active_query_mount_epoch = mount_epoch;
  ExactQueryCursor::Impl fresh;
  if (auto previous=cursor->impl_->read_owner.lock())
    previous->cancel_read_budgeted({cursor->impl_->context.index_identity,cursor->impl_->context.query_revision});
  fresh.read_owner=impl_->reader;
  fresh.initialized = true;
  fresh.context =
      {impl_->identity, impl_->query_revision, impl_->index_epoch, mount_epoch};
  fresh.code = std::move(code);
  fresh.phase = ExactQueryCursor::Impl::Phase::Locate;
  fresh.page_id = impl_->superblock.root_page;
  *cursor->impl_ = std::move(fresh);
  error->clear();
  return true;
}

QueryBatchContext PagedSchemeIndex::query_context(const ExactQueryCursor& cursor) const {
  return cursor.impl_ ? cursor.impl_->context : QueryBatchContext{};
}

bool PagedSchemeIndex::query_is_current(const QueryBatchContext& context) const {
  return impl_->context_is_current(context);
}

bool PagedSchemeIndex::restart_exact_query(const QueryBatchContext& expected,
                                          ExactQueryCursor* cursor, std::string* error) {
  if (!cursor || !cursor->impl_ || !impl_->context_is_current(expected) ||
      cursor->impl_->context.index_identity != expected.index_identity ||
      cursor->impl_->context.index_epoch != expected.index_epoch ||
      cursor->impl_->context.mount_epoch != expected.mount_epoch ||
      cursor->impl_->context.query_revision != expected.query_revision) {
    if (error) *error = "conditional query restart is stale or foreign";
    return false;
  }
  return begin_exact_query(cursor->impl_->code, expected.mount_epoch, cursor, error);
}

ExactQueryStepResult PagedSchemeIndex::advance_exact_query(
    ExactQueryCursor* cursor, const WorkBudget& budget,
    std::size_t batch_capacity) {
  ExactQueryStepResult result;
  if (!valid_budget(budget) || batch_capacity == 0U) {
    result.status = ExactQueryStepStatus::InvalidBudget;
    result.error = "query budget and batch capacity must be non-zero";
    return result;
  }
  if (!cursor || !cursor->impl_ || !cursor->impl_->initialized) {
    result.status = ExactQueryStepStatus::Error;
    result.error = "query cursor is not initialized";
    return result;
  }
  ExactQueryCursor::Impl& query = *cursor->impl_;
  result.batch.context = query.context;
  // Foreign work must not touch even the foreign cursor's cancellation state.
  if (query.context.index_identity != impl_->identity ||
      query.context.index_epoch != impl_->index_epoch) {
    result.status = ExactQueryStepStatus::Stale;
    return result;
  }
  if (query.cancelled) {
    result.status = ExactQueryStepStatus::Cancelled;
    return result;
  }
  if (!impl_->context_is_current(query.context)) {
    release_query_read(*cursor);
    result.status = ExactQueryStepStatus::Stale;
    return result;
  }
  result.batch.hits.reserve(batch_capacity);
  if (query.phase == ExactQueryCursor::Impl::Phase::Done) {
    result.status = ExactQueryStepStatus::Complete;
    return result;
  }
  if (query.phase == ExactQueryCursor::Impl::Phase::NoResults) {
    result.status = ExactQueryStepStatus::NoResults;
    return result;
  }
  if (query.phase == ExactQueryCursor::Impl::Phase::Error) {
    result.status = ExactQueryStepStatus::Error;
    result.error = query.error;
    return result;
  }

  auto can_read = [&]() {
    return result.cost.read_calls < budget.max_read_calls &&
           result.cost.read_bytes < budget.max_read_bytes;
  };
  auto can_work = [&]() {
    return result.cost.work_items < budget.max_work_items;
  };
  auto fail_query = [&](std::string message) {
    query.phase = ExactQueryCursor::Impl::Phase::Error;
    query.error = std::move(message);
  };
  auto reset_page = [&](std::uint32_t page_id) {
    query.page_id = page_id;
    query.page.clear();
    query.page_fill = 0U;
    query.pending_read_amount=0;
    query.header_parsed = false;
    query.record_index = 0U;
    query.record_offset = kPageHeaderBytes;
  };

  while (query.phase == ExactQueryCursor::Impl::Phase::Locate ||
         query.phase == ExactQueryCursor::Impl::Phase::Scan) {
    if (query.page_id >= impl_->superblock.page_count) {
      fail_query("query page id is outside the index");
      break;
    }
    if (query.page.empty()) {
      if (const auto* cached = impl_->find_cached(query.page_id)) {
        query.page = *cached;
        query.page_fill = query.page.size();
      } else {
        query.page.resize(impl_->superblock.page_bytes);
      }
    }
    if (query.page_fill < query.page.size()) {
      if (!can_read()) {
        break;
      }
      const std::size_t remaining = query.page.size() - query.page_fill;
      const std::size_t byte_budget =
          budget.max_read_bytes - result.cost.read_bytes;
      const std::size_t amount = query.pending_read_amount ? query.pending_read_amount :
          std::min({remaining, byte_budget, impl_->config.max_single_read_bytes});
      const auto step=impl_->reader->read_budgeted(
              impl_->absolute_pages_offset +
                  static_cast<std::size_t>(query.page_id) *
                      impl_->superblock.page_bytes +
                  query.page_fill,
              query.page.data()+query.page_fill,amount,
              {budget.max_read_calls-result.cost.read_calls,budget.max_read_bytes-result.cost.read_bytes,
               budget.max_work_items-result.cost.work_items},
              {query.context.index_identity,query.context.query_revision});
      result.cost.read_calls+=step.cost.read_calls; result.cost.read_bytes+=step.cost.read_bytes;
      result.cost.work_items+=step.cost.work_items;
      if (step.status==ResourceReadStatus::Pending) { query.pending_read_amount=amount; break; }
      query.pending_read_amount=0;
      if (step.status==ResourceReadStatus::Error) {
        fail_query("query page read failed");
        break;
      }
      query.page_fill += amount;
      if (query.page_fill == query.page.size()) {
        impl_->insert_cache(query.page_id, query.page);
      }
      continue;
    }
    if (!query.header_parsed) {
      if (!can_work()) {
        break;
      }
      std::string page_error;
      if (!parse_page_header(query.page, &query.header, &page_error)) {
        fail_query(std::move(page_error));
        break;
      }
      query.header_parsed = true;
      ++result.cost.work_items;
      if (query.phase == ExactQueryCursor::Impl::Phase::Locate &&
          query.header.kind == PageKind::Leaf) {
        query.phase = ExactQueryCursor::Impl::Phase::Scan;
      } else if (query.phase == ExactQueryCursor::Impl::Phase::Locate &&
                 query.header.kind != PageKind::Internal) {
        fail_query("query locate stage reached an invalid page kind");
        break;
      } else if (query.phase == ExactQueryCursor::Impl::Phase::Scan &&
                 query.header.kind != PageKind::Leaf) {
        fail_query("query scan stage reached an internal page");
        break;
      }
      continue;
    }

    if (query.record_index < query.header.record_count) {
      if (!can_work()) {
        break;
      }
      std::string record_error;
      if (query.phase == ExactQueryCursor::Impl::Phase::Locate) {
        std::string_view maximum_code;
        std::uint32_t child_page = 0;
        if (!read_internal_record(query.page, query.header.used_bytes,
                                  &query.record_offset, &maximum_code,
                                  &child_page, &record_error)) {
          fail_query(std::move(record_error));
          break;
        }
        ++query.record_index;
        ++result.cost.work_items;
        if (compare_code(query.code, maximum_code) <= 0) {
          reset_page(child_page);
        }
        continue;
      }

      std::string_view record_code;
      RawQueryHit hit;
      if (!read_leaf_record(query.page, query.header.used_bytes,
                            &query.record_offset, &record_code, &hit,
                            &record_error)) {
        fail_query(std::move(record_error));
        break;
      }
      ++query.record_index;
      ++result.cost.work_items;
      const int order = compare_code(record_code, query.code);
      if (order < 0) {
        continue;
      }
      if (order == 0) {
        query.found_any = true;
        result.batch.hits.push_back(hit);
        if (result.batch.hits.size() == batch_capacity) {
          break;
        }
        continue;
      }
      query.phase = query.found_any ? ExactQueryCursor::Impl::Phase::Done
                                    : ExactQueryCursor::Impl::Phase::NoResults;
      break;
    }

    if (query.record_offset != query.header.used_bytes) {
      fail_query("query page has trailing record bytes");
      break;
    }
    if (query.phase == ExactQueryCursor::Impl::Phase::Locate) {
      query.phase = ExactQueryCursor::Impl::Phase::NoResults;
      break;
    }
    if (query.header.next_leaf == kNoPage) {
      query.phase = query.found_any ? ExactQueryCursor::Impl::Phase::Done
                                    : ExactQueryCursor::Impl::Phase::NoResults;
      break;
    }
    reset_page(query.header.next_leaf);
  }

  if (!result.batch.hits.empty()) {
    result.status = ExactQueryStepStatus::HitsAvailable;
  } else if (query.phase == ExactQueryCursor::Impl::Phase::Done) {
    result.status = ExactQueryStepStatus::Complete;
  } else if (query.phase == ExactQueryCursor::Impl::Phase::NoResults) {
    result.status = ExactQueryStepStatus::NoResults;
  } else if (query.phase == ExactQueryCursor::Impl::Phase::Error) {
    result.status = ExactQueryStepStatus::Error;
    result.error = query.error;
  } else {
    result.status = ExactQueryStepStatus::InProgress;
  }
  return result;
}

void PagedSchemeIndex::release_query_read(const ExactQueryCursor& cursor) {
  if (!cursor.impl_ || !cursor.impl_->initialized || !impl_->reader) {
    return;
  }
  const auto& context=cursor.impl_->context;
  // Releasing a saved operation does not require its resources to remain
  // usable. Match this owner's original operation, not another caller's work.
  if (context.index_identity!=impl_->identity || context.index_epoch!=impl_->index_epoch ||
      context.query_revision!=impl_->query_revision || context.mount_epoch!=impl_->active_query_mount_epoch) {
    return;
  }
  impl_->reader->cancel_read_budgeted({context.index_identity,context.query_revision});
  cursor.impl_->pending_read_amount=0;
}

void PagedSchemeIndex::cancel_query(ExactQueryCursor* cursor) {
  if (!cursor || !cursor->impl_ || !cursor->impl_->initialized) {
    return;
  }
  const auto& context=cursor->impl_->context;
  if (context.index_identity!=impl_->identity || context.index_epoch!=impl_->index_epoch ||
      context.query_revision!=impl_->query_revision || context.mount_epoch!=impl_->active_query_mount_epoch) {
    return;
  }
  release_query_read(*cursor);
  if (impl_->query_revision != std::numeric_limits<std::uint64_t>::max()) {
    ++impl_->query_revision;
  }
  impl_->active_query_mount_epoch = 0U;
  cursor->impl_->cancelled = true;
}

bool PagedSchemeIndex::batch_is_current(const RawQueryBatch& batch) const {
  return impl_->context_is_current(batch.context);
}

void PagedSchemeIndex::deactivate() {
  if (impl_->reader) impl_->reader->cancel_read_budgeted({impl_->identity,impl_->query_revision});
  ++impl_->index_epoch;
  impl_->active_query_mount_epoch = 0U;
  impl_->state = Impl::State::Closed;
  impl_->reader.reset();
  impl_->verified_shared=nullptr; impl_->verified_activation={};
  impl_->base_order.reset(); impl_->verified_reader.reset();
  impl_->release_validation_buffers();
  impl_->cache.clear();
}

}  // namespace hanime
