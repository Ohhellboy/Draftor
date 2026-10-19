#include "paged_shared_internal.hpp"
#include "output_reuse.hpp"
#include "shared_structural.hpp"
#include "hanime/verified_resource.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <new>
#include <utility>

#if HANIME_STRUCTURAL_HOST_OBSERVE
#define SHARED_STRUCTURAL_DIAGNOSTIC(...) do { __VA_ARGS__; } while (false)
#else
#define SHARED_STRUCTURAL_DIAGNOSTIC(...) do {} while (false)
#endif

namespace hanime {
struct SharedActivationIdentity {};
namespace {
using namespace detail;
constexpr std::array<std::uint32_t, 5> kWidths{{52, 16, 8, 8, 4}};
constexpr std::uint32_t kNo = kNoSharedString;
constexpr std::uint32_t kCoLocatedMagic = 0x33485348U;
constexpr std::uint32_t kCoLocatedLayoutMagic = 0x33444853U;
constexpr std::uint32_t kCoLocatedPageMagic = 0x33504753U;
constexpr std::uint32_t kShortTextMagic = 0x34485348U;
constexpr std::uint32_t kShortTextLayoutMagic = 0x34444853U;
constexpr std::uint32_t kShortTextPageMagic = 0x34504753U;
constexpr std::size_t kShortTextCapacity = 10;
bool short_text(const PagedSharedInfo& info) { return info.sections[0].record_bytes == 64; }
bool colocated(const PagedSharedInfo& info) { return info.sections[0].record_bytes >= 60; }
std::uint32_t records_per_page(const PagedSharedInfo& info, std::size_t s) {
  return short_text(info) && s == 0 ? 63U : (info.page_bytes - 16) / info.sections[s].record_bytes;
}
std::size_t record_position(const PagedSharedInfo& info, std::size_t s, std::uint32_t i) {
  const auto local = i % records_per_page(info,s);
  if (short_text(info) && s == 0) return local < 7 ? 16 + local*64 : 512 + (local-7)*64;
  return 16 + std::size_t(local)*info.sections[s].record_bytes;
}

std::uint32_t u32(const std::uint8_t* p) {
  return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8U) |
         (std::uint32_t(p[2]) << 16U) | (std::uint32_t(p[3]) << 24U);
}
std::uint64_t u64(const std::uint8_t* p) {
  return std::uint64_t(u32(p)) | (std::uint64_t(u32(p + 4)) << 32U);
}
void put32(std::uint8_t* p, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (i * 8U));
}
void put64(std::uint8_t* p, std::uint64_t v) {
  put32(p, static_cast<std::uint32_t>(v)); put32(p + 4, static_cast<std::uint32_t>(v >> 32U));
}
void hash_byte(std::uint64_t& h, std::uint8_t b) { h = (h ^ b) * kSharedFnvPrime; }
std::uint64_t namespace_hash(RegistryNamespaceId ns) {
  auto h = kSharedFnvOffset;
  for (unsigned i = 0; i < 8; ++i) hash_byte(h, static_cast<std::uint8_t>(ns >> (i * 8U)));
  return h;
}
bool valid_budget(const WorkBudget& b) {
  return b.max_read_calls && b.max_read_bytes && b.max_work_items;
}
bool parse_layout(const std::uint8_t* h, std::size_t file_size,
                  PagedSharedInfo* info, std::string* error) {
  auto fail = [&](const char* s) { *error = s; return false; };
  const bool layout4 = u32(h) == kShortTextMagic && u32(h + 4) == 4 &&
                       u32(h + 40) == kShortTextLayoutMagic;
  const bool layout3 = u32(h) == kCoLocatedMagic && u32(h + 4) == 3 &&
                       u32(h + 40) == kCoLocatedLayoutMagic;
  const bool layout2 = u32(h) == kSharedMagic && u32(h + 4) == 2 &&
                       u32(h + 40) == kSharedLayoutMagic;
  if (file_size < kSharedHeaderBytes || (!layout2 && !layout3 && !layout4) || u32(h + 44) != 160 ||
      u64(h + 32) != file_size - 40) return fail("shared header/version/size mismatch");
  if (!u64(h + 8) || !u64(h + 16) || u32(h + 48) != kStableKeyFormat ||
      u32(h + 52) != 32 || u32(h + 60) != 5) return fail("shared namespace/key format mismatch");
  info->registry_namespace = u64(h + 8); info->shared_bundle_id = u64(h + 16);
  info->key_format = u32(h + 48); info->page_bytes = u32(h + 56);
  info->file_bytes = file_size;
  if (info->page_bytes < 68 || info->page_bytes > 65536) return fail("invalid shared page width");
  if (layout4 && info->page_bytes != 4096) return fail("short-text layout requires original 4096-byte logical pages");
  std::uint64_t expected_offset = layout4 ? 512 : kSharedHeaderBytes;
  for (std::size_t s = 0; s < 5; ++s) {
    auto& section = info->sections[s]; const auto* d = h + 64 + 24 * s;
    section = {u64(d), u32(d + 8), u32(d + 12), u32(d + 16)};
    if (section.record_bytes != (s == 0 && layout4 ? 64U : (layout3 && s == 0 ? 60U : kWidths[s])) || u32(d + 20) ||
        section.offset != expected_offset) return fail("shared section layout mismatch");
    const std::uint32_t per_page = records_per_page(*info,s);
    if (!per_page) return fail("shared record does not fit physical page");
    const std::uint64_t pages = (std::uint64_t(section.count) + per_page - 1) / per_page;
    if (pages != section.page_count) return fail("shared section page count mismatch");
    // uint32 page count * bounded uint32 page width fits uint64; check before add.
    const std::uint64_t span = pages * info->page_bytes;
    if (expected_offset > file_size || span > file_size - expected_offset)
      return fail("shared section out of bounds");
    expected_offset += span;
  }
  info->payload_offset = u64(h + 184); info->payload_bytes = u64(h + 192);
  if (!info->sections[0].count || info->sections[4].count != info->sections[0].count ||
      info->payload_offset != expected_offset ||
      info->payload_bytes > std::numeric_limits<std::uint32_t>::max() ||
      info->payload_bytes != file_size - expected_offset)
    return fail("shared payload or uniqueness coverage length mismatch");
  return true;
}
std::uint32_t page_records(const PagedSharedInfo& info, std::size_t s, std::uint32_t page) {
  const auto& sec = info.sections[s];
  const auto per = records_per_page(info,s);
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(per, sec.count - std::uint64_t(page) * per));
}
bool valid_page(const std::uint8_t* p, const PagedSharedInfo& info,
                std::size_t s, std::uint32_t page) {
  return u32(p) == (short_text(info) ? kShortTextPageMagic : (colocated(info) ? kCoLocatedPageMagic : kSharedPageMagic)) && u32(p + 4) == s && u32(p + 8) == page &&
         u32(p + 12) == page_records(info, s, page);
}
bool string_ref(std::uint32_t id, const PagedSharedInfo& info, bool optional = false) {
  return (optional && id == kNo) || id < info.sections[3].count;
}
bool lexeme_record(const std::uint8_t* p, const PagedSharedInfo& info, SharedLexemeRecord* out) {
  std::copy_n(p, 32, out->identity.stable_key.bytes.begin());
  const auto weight = u32(p + 32);
  out->identity.base_weight = weight <= 0x7fffffffU ? static_cast<std::int32_t>(weight)
      : static_cast<std::int32_t>(std::int64_t(weight) - 0x100000000LL);
  out->default_payload = u32(p + 36); out->relation_start = u32(p + 40);
  out->relation_count = u32(p + 44); out->identity.kind = static_cast<LexemeKind>(p[48]);
  return !out->identity.stable_key.empty() && (p[48] == 1 || p[48] == 2) &&
      (short_text(info) || (!p[49] && !p[50] && !p[51])) && string_ref(out->default_payload, info) &&
      std::uint64_t(out->relation_start) + out->relation_count <= info.sections[1].count;
}
bool relation_record(const std::uint8_t* p, const PagedSharedInfo& info, SharedOutputRecord* out) {
  out->selector = u32(p); out->target_payload = u32(p + 4);
  out->condition_descriptor = u32(p + 8); out->condition_kind = static_cast<ConditionKind>(p[12]);
  out->availability = p[13]; out->admission = static_cast<OutputAdmission>(p[14]); out->preferred = p[15] != 0;
  return out->selector < info.sections[2].count && p[12] <= 1 && p[14] <= 1 && p[15] <= 1 &&
      p[13] && !(p[13] & ~7U) && string_ref(out->target_payload, info, p[14] == 1) &&
      (p[12] == 0 ? out->condition_descriptor == kNo : string_ref(out->condition_descriptor, info));
}
bool selector_record(const std::uint8_t* p, const PagedSharedInfo& info, SharedSelectorRecord* out) {
  *out = {u32(p), u32(p + 4)};
  return string_ref(out->name, info) && string_ref(out->selection_basis, info, true);
}
bool string_record(const std::uint8_t* p, const PagedSharedInfo& info, SharedStringDescriptor* out) {
  *out = {u32(p), u32(p + 4)};
  return out->length && std::uint64_t(out->offset) + out->length <= info.payload_bytes;
}
const std::uint8_t* memory_record(const std::vector<std::uint8_t>& bytes,
                                  const PagedSharedInfo& info, std::size_t s, std::uint32_t index) {
  const auto& sec = info.sections[s]; const auto per = records_per_page(info,s);
  return bytes.data() + static_cast<std::size_t>(sec.offset) +
      std::size_t(index / per) * info.page_bytes + record_position(info,s,index);
}

struct Utf8State {
  std::uint32_t code{0}, minimum{0}; unsigned remaining{0};
  bool consume(std::uint8_t b) {
    if (!remaining) {
      if (b < 0x80) return true;
      if (b >= 0xc2 && b <= 0xdf) { code = b & 31; minimum = 0x80; remaining = 1; }
      else if (b >= 0xe0 && b <= 0xef) { code = b & 15; minimum = 0x800; remaining = 2; }
      else if (b >= 0xf0 && b <= 0xf4) { code = b & 7; minimum = 0x10000; remaining = 3; }
      else return false;
    } else {
      if ((b & 0xc0) != 0x80) return false;
      code = (code << 6) | (b & 63);
      if (--remaining == 0 && (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))) return false;
    }
    return true;
  }
};
struct UniquenessCostScope {
  WorkCost& total;
  const WorkCost& current;
  WorkCost before;
  ~UniquenessCostScope() {
    total.read_calls += current.read_calls - before.read_calls;
    total.read_bytes += current.read_bytes - before.read_bytes;
    total.work_items += current.work_items - before.work_items;
  }
};
}  // namespace

namespace detail {
bool build_shared_bundle(RegistryNamespaceId ns, const SharedBuildTables& tables,
                         std::uint32_t page_bytes, std::vector<std::uint8_t>* output,
                         PagedSharedInfo* info, std::string* error) {
  if (page_bytes < 68 || page_bytes > 65536) { *error = "invalid shared page width"; return false; }
  const std::array<std::size_t, 5> counts{{tables.lexemes.size(), tables.relations.size(),
      tables.selectors.size(), tables.strings.size(), tables.lexemes.size()}};
  std::uint64_t pos = kSharedHeaderBytes;
  info->page_bytes = page_bytes;
  for (std::size_t s = 0; s < 5; ++s) {
    if (counts[s] > std::numeric_limits<std::uint32_t>::max()) { *error = "shared count overflow"; return false; }
    const auto per = (page_bytes - 16) / kWidths[s];
    const auto pages = (counts[s] + per - 1) / per;
    info->sections[s] = {pos, static_cast<std::uint32_t>(counts[s]), kWidths[s], static_cast<std::uint32_t>(pages)};
    pos += std::uint64_t(pages) * page_bytes;
  }
  info->payload_offset = pos;
  std::uint64_t payload = 0;
  for (const auto& text : tables.strings) payload += text.size();
  if (payload > std::numeric_limits<std::uint32_t>::max() || pos > std::numeric_limits<std::size_t>::max() - payload) {
    *error = "shared payload size overflow"; return false;
  }
  info->payload_bytes = payload; info->file_bytes = static_cast<std::size_t>(pos + payload);
  info->registry_namespace = ns;
  output->assign(info->file_bytes, 0);
  auto* h = output->data();
  put32(h, kSharedMagic); put32(h + 4, 2); put64(h + 8, ns); put64(h + 32, output->size() - 40);
  put32(h + 40, kSharedLayoutMagic); put32(h + 44, 160); put32(h + 48, kStableKeyFormat);
  put32(h + 52, 32); put32(h + 56, page_bytes); put32(h + 60, 5);
  for (std::size_t s = 0; s < 5; ++s) {
    const auto& sec = info->sections[s]; auto* d = h + 64 + s * 24;
    put64(d, sec.offset); put32(d + 8, sec.count); put32(d + 12, sec.record_bytes); put32(d + 16, sec.page_count);
    for (std::uint32_t page = 0; page < sec.page_count; ++page) {
      auto* p = h + static_cast<std::size_t>(sec.offset) + std::size_t(page) * page_bytes;
      put32(p, kSharedPageMagic); put32(p + 4, static_cast<std::uint32_t>(s)); put32(p + 8, page); put32(p + 12, page_records(*info, s, page));
    }
  }
  put64(h + 184, pos); put64(h + 192, payload);
  auto record = [&](std::size_t s, std::uint32_t i) { return const_cast<std::uint8_t*>(memory_record(*output, *info, s, i)); };
  for (std::uint32_t i = 0; i < tables.lexemes.size(); ++i) {
    auto* p = record(0, i); const auto& l = tables.lexemes[i];
    std::copy(l.identity.stable_key.bytes.begin(), l.identity.stable_key.bytes.end(), p);
    put32(p + 32, static_cast<std::uint32_t>(l.identity.base_weight)); put32(p + 36, l.default_payload);
    put32(p + 40, l.relation_start); put32(p + 44, l.relation_count); p[48] = static_cast<std::uint8_t>(l.identity.kind);
  }
  for (std::uint32_t i = 0; i < tables.relations.size(); ++i) {
    auto* p = record(1, i); const auto& r = tables.relations[i];
    put32(p, r.selector); put32(p + 4, r.target_payload); put32(p + 8, r.condition_descriptor);
    p[12] = static_cast<std::uint8_t>(r.condition_kind); p[13] = r.availability;
    p[14] = static_cast<std::uint8_t>(r.admission); p[15] = r.preferred ? 1 : 0;
  }
  for (std::uint32_t i = 0; i < tables.selectors.size(); ++i) {
    auto* p = record(2, i); put32(p, tables.selectors[i].name); put32(p + 4, tables.selectors[i].selection_basis);
  }
  std::uint32_t offset = 0;
  for (std::uint32_t i = 0; i < tables.strings.size(); ++i) {
    const auto& text = tables.strings[i]; auto* p = record(3, i);
    put32(p, offset); put32(p + 4, static_cast<std::uint32_t>(text.size()));
    std::copy(text.begin(), text.end(), h + static_cast<std::size_t>(pos) + offset);
    offset += static_cast<std::uint32_t>(text.size());
  }
  std::vector<std::uint32_t> order(tables.lexemes.size()); std::iota(order.begin(), order.end(), 1U);
  std::sort(order.begin(), order.end(), [&](auto a, auto b) { return tables.lexemes[a - 1].identity.stable_key < tables.lexemes[b - 1].identity.stable_key; });
  for (std::uint32_t i = 0; i < order.size(); ++i) put32(record(4, i), order[i]);
  auto hash = kSharedFnvOffset, identity = namespace_hash(ns);
  for (std::size_t i = 40; i < output->size(); ++i) { hash_byte(hash, h[i]); hash_byte(identity, h[i]); }
  if (!identity) { *error = "zero shared content identity"; return false; }
  put64(h + 16, identity); put64(h + 24, hash); info->shared_bundle_id = identity;
  return true;
}

bool decode_shared_host(const ResourceReader& reader, DecodedSharedBundle* out, std::string* error) {
  // Legacy Host-only materialization, never called from PagedSharedResource.
  std::vector<std::uint8_t> bytes(reader.size());
  if (bytes.size() < kSharedHeaderBytes || !reader.read(0, bytes.data(), bytes.size()) ||
      !parse_layout(bytes.data(), bytes.size(), &out->info, error)) return false;
  auto hash = kSharedFnvOffset, identity = namespace_hash(out->info.registry_namespace);
  for (std::size_t i = 40; i < bytes.size(); ++i) { hash_byte(hash, bytes[i]); hash_byte(identity, bytes[i]); }
  if (hash != u64(bytes.data() + 24) || identity != out->info.shared_bundle_id) { *error = "shared checksum or identity mismatch"; return false; }
  auto fail = [&]() { *error = "shared record/coverage is invalid"; return false; };
  const auto& info = out->info;
  for (std::size_t s = 0; s < 5; ++s)
    for (std::uint32_t p = 0; p < info.sections[s].page_count; ++p)
      if (!valid_page(bytes.data() + static_cast<std::size_t>(info.sections[s].offset) + std::size_t(p) * info.page_bytes, info, s, p)) return fail();
  auto get = [&](std::size_t s, std::uint32_t i) { return memory_record(bytes, info, s, i); };
  std::uint64_t next_relation = 0;
  for (std::uint32_t i = 0; i < info.sections[0].count; ++i) {
    SharedLexemeRecord r; if (!lexeme_record(get(0, i), info, &r) || r.relation_start != next_relation) return fail();
    if (colocated(info)) {
      SharedStringDescriptor embedded, original;
      const auto* p=get(0,i);
      if (short_text(info)) {
        embedded={u32(p+50),p[49] ? std::uint32_t(p[49]) : u32(p+54)};
        if (p[49]>kShortTextCapacity || !embedded.length ||
            std::uint64_t(embedded.offset)+embedded.length>info.payload_bytes ||
            (!p[49] && embedded.length<=kShortTextCapacity)) return fail();
        const auto padding=p[49] ? p[49] : 4U;
        for (std::size_t j=padding;j<10;++j) if(p[54+j]) return fail();
        if (p[49] && !std::equal(p+54,p+54+p[49],bytes.data()+info.payload_offset+embedded.offset)) return fail();
      } else if (!string_record(p+52,info,&embedded)) return fail();
      if (!string_record(get(3, r.default_payload), info, &original) ||
          embedded.offset != original.offset || embedded.length != original.length) return fail();
    }
    next_relation += r.relation_count; out->tables.lexemes.push_back(r);
  }
  if (next_relation != info.sections[1].count) return fail();
  for (std::uint32_t i = 0; i < info.sections[1].count; ++i) {
    SharedOutputRecord r; if (!relation_record(get(1, i), info, &r)) return fail(); out->tables.relations.push_back(r);
  }
  for (std::uint32_t i = 0; i < info.sections[2].count; ++i) {
    SharedSelectorRecord r; if (!selector_record(get(2, i), info, &r)) return fail(); out->tables.selectors.push_back(r);
  }
  std::uint64_t next_string = 0;
  for (std::uint32_t i = 0; i < info.sections[3].count; ++i) {
    SharedStringDescriptor r; if (!string_record(get(3, i), info, &r) || r.offset != next_string) return fail();
    std::string text(reinterpret_cast<const char*>(bytes.data() + info.payload_offset + r.offset), r.length);
    if (!is_valid_utf8(text)) return fail();
    next_string += r.length;
    out->tables.strings.push_back(std::move(text)); out->descriptors.push_back(r);
  }
  if (next_string != info.payload_bytes) return fail();
  StableLexemeKey previous;
  for (std::uint32_t i = 0; i < info.sections[4].count; ++i) {
    const auto ref = u32(get(4, i)); if (!ref || ref > info.sections[0].count) return fail();
    const auto& key = out->tables.lexemes[ref - 1].identity.stable_key;
    if (i && !(previous < key)) return fail();
    previous = key;
  }
  return true;
}
}  // namespace detail

struct PagedSharedResource::Impl {
  enum class Phase { Closed, Hash, Pages, Lexemes, Relations, Selectors, Strings, Unique, Resident, Ready, Error };
  struct ExternalRelease {
    detail::SharedStructuralAccess::Release release{nullptr};
    void operator()(std::uint8_t* p) const { if (p && release) release(p); }
  };
  struct CacheSlot { std::uint64_t offset{0}; std::size_t filled{0}; bool assigned{false}, checked{false}; };
  std::shared_ptr<const ResourceReader> reader;
  std::shared_ptr<const SharedActivationIdentity> identity;
  const VerifiedResourceReader* verified{nullptr}; // Owned through reader.
  PagedSharedInfo info;
  PagedSharedConfig config;
  MountEpoch epoch{0}; std::uint64_t revision{0};
  std::unique_ptr<std::uint8_t[]> cache;
  std::unique_ptr<CacheSlot[]> slots;
  std::unique_ptr<detail::OutputReuseCache> output_reuse;
  std::size_t replace_slot{0};
  Phase phase{Phase::Closed}; std::string error;
  std::uint64_t expected_hash{0}, body_hash{kSharedFnvOffset}, identity_hash{0};
  std::size_t section{0}; std::uint32_t index{0};
  std::uint64_t next_relation{0}, next_string{0};
  bool secondary{false}; std::uint32_t unique_ref{0}; StableLexemeKey previous_key;
  std::array<std::uint8_t, 256> stream_buffer{};
  std::uint64_t stream_start{0}, stream_size{0}, stream_done{0};
  std::size_t stream_pos{0}, stream_filled{0}; Utf8State utf8;
  SharedRecordKind read_kind{SharedRecordKind::Lexeme}; std::uint32_t read_index{0};
  bool read_second{false}, read_complete{false}, read_active{false}, io_failed{false};
  SharedLexemeRecord read_lexeme;
#if HANIME_TEXT_CHAIN
  SharedStringDescriptor read_default_span; // Valid only with this completed Lexeme revision.
#endif
  std::size_t pending_read_bytes{0}; // Freeze a budgeted request across small/varying ticks.
  std::size_t slice_section{0}, slice_filled{0};
  std::uint32_t slice_index{0};
  bool slice_assigned{false};
  detail::SharedStructuralStats structural;
  std::unique_ptr<std::uint8_t, ExternalRelease> resident{nullptr, ExternalRelease{}};
  bool resident_started{false}, resident_complete{false};
  std::size_t resident_index{0}, resident_stage{0};
  const std::uint8_t* resident_source{nullptr};
  SharedStringDescriptor resident_span;
  std::uint64_t resident_next_offset{0};
  // One serial record slot: this pointer is consumed before any next record or
  // reader operation can overwrite the authenticated slice. Cancellation/new
  // begin clears it. RAM projection uses the same byte-metered parser.
  const std::uint8_t* locator_source{nullptr};
  std::size_t locator_position{0};
  SharedStringDescriptor locator_span;
  SharedLexemeRecord validation_lexeme;
  SharedStringDescriptor validation_span;
  // Pointer refers only to this owner's page allocation, never verifier cache.
  // A serial read owns the allocation until begin/cancel/retirement replaces it.
  const std::uint8_t* short_source{nullptr};
  std::uint8_t short_length{0}, short_position{0};
  bool short_checked{false}, short_payload_active{false};

  bool matches(const SharedReadContext& c) const {
    return phase == Phase::Ready && reader && reader->healthy() && identity && c.identity == identity && c.mount_epoch == epoch &&
        c.operation_revision == revision && revision != 0;
  }
  void fail(const char* message) { phase = Phase::Error; error = message; }
  std::size_t read_amount(std::size_t left, const WorkBudget& b, const WorkCost& c) const {
    if (pending_read_bytes) return pending_read_bytes;
    if (c.read_calls == b.max_read_calls || c.read_bytes == b.max_read_bytes) return 0;
    return std::min({left, config.max_single_read_bytes, b.max_read_bytes - c.read_bytes});
  }
  bool read_bytes(std::uint64_t offset, void* p, std::size_t n, const WorkBudget& b, WorkCost& c) {
    ResourceReadStep step;
    if (offset <= info.file_bytes && n <= info.file_bytes-offset) {
      step=reader->read_budgeted(static_cast<std::size_t>(offset),p,n,
          {b.max_read_calls-c.read_calls,b.max_read_bytes-c.read_bytes,b.max_work_items-c.work_items},
          {identity,revision});
      c.read_calls+=step.cost.read_calls; c.read_bytes+=step.cost.read_bytes;
      c.work_items+=step.cost.work_items;
    }
    if (step.status==ResourceReadStatus::Pending) { pending_read_bytes=n; return false; }
    pending_read_bytes=0;
    if (step.status==ResourceReadStatus::Error) {
      // A transient runtime I/O failure terminates this operation, not the
      // validated immutable activation. Validation/format failures stay fatal.
      if (phase == Phase::Ready && reader->healthy()) { io_failed = true; error = "shared resource read failed"; }
      else fail("shared resource read failed");
      return false;
    }
    return true;
  }
  const std::uint8_t* page(std::size_t s, std::uint32_t number, const WorkBudget& b, WorkCost& c) {
    const auto offset = info.sections[s].offset + std::uint64_t(number) * info.page_bytes;
    std::size_t slot = config.page_cache_capacity;
    for (std::size_t i = 0; i < config.page_cache_capacity; ++i)
      if (slots[i].assigned && slots[i].offset == offset) { slot = i; break; }
    if (slot == config.page_cache_capacity) {
      slot = replace_slot; replace_slot = (replace_slot + 1) % config.page_cache_capacity;
      slots[slot] = {offset, 0, true, false};
    }
    auto& meta = slots[slot]; auto* p = cache.get() + slot * info.page_bytes;
    while (meta.filled < info.page_bytes) {
      const auto n = read_amount(info.page_bytes - meta.filled, b, c); if (!n) return nullptr;
      if (!read_bytes(offset + meta.filled, p + meta.filled, n, b, c)) return nullptr;
      meta.filled += n;
    }
    if (!meta.checked) {
      if (c.work_items == b.max_work_items) return nullptr;
      ++c.work_items;
      if (!valid_page(p, info, s, number)) { fail("shared page header mismatch"); return nullptr; }
      meta.checked = true;
    }
    return p;
  }
  const std::uint8_t* verified_record_slice(std::size_t section_index,
      std::uint32_t record_index, const WorkBudget& budget, WorkCost& cost) {
    const auto& section_info = info.sections[section_index];
    const auto per_page = records_per_page(info,section_index);
    const auto page_number = record_index / per_page;

    // parse_layout checked section spans, record widths and page counts using
    // authenticated global metadata. These products fit within that span.
    // The physical page header contains only redundant type/number/count, not
    // pointers used here. Its static semantics were checked before publication;
    // do not fetch an unrelated 1 KiB block merely to recompute those values.
    const auto record_offset = section_info.offset +
        std::uint64_t(page_number) * info.page_bytes + record_position(info,section_index,record_index);
    if (page_number >= section_info.page_count || record_offset > info.file_bytes ||
        section_info.record_bytes > info.file_bytes - record_offset) {
      fail("authenticated shared record bounds mismatch");
      return nullptr;
    }

    // One immutable record, in the existing page allocation, survives a small
    // budget or the immediately repeated descriptor request. This is not a
    // candidate/selection cache. Reopening replaces the entire activation.
    if (!slice_assigned || slice_section != section_index || slice_index != record_index) {
      slice_section = section_index;
      slice_index = record_index;
      slice_filled = 0;
      slice_assigned = true;
    }
    auto* bytes = cache.get();
    while (slice_filled < section_info.record_bytes) {
      const auto amount = read_amount(section_info.record_bytes - slice_filled, budget, cost);
      if (!amount) return nullptr;
      // read_bytes does not advance slice_filled until the authenticated request
      // is complete. Partial/cancelled/unverified bytes never reach the parser.
      if (!read_bytes(record_offset + slice_filled, bytes + slice_filled, amount, budget, cost)) {
        return nullptr;
      }
      slice_filled += amount;
    }
    if (cost.work_items == budget.max_work_items) return nullptr;
    ++cost.work_items;
    return bytes;
  }

  const std::uint8_t* record(std::size_t s, std::uint32_t i, const WorkBudget& b, WorkCost& c) {
    const auto& sec = info.sections[s];
    if (i >= sec.count) { fail("shared record reference out of range"); return nullptr; }
    const auto per = records_per_page(info,s);
    if (verified && config.verified_record_slices && (phase == Phase::Ready || phase == Phase::Resident)) {
      return verified_record_slice(s, i, b, c);
    }
    const auto* p = page(s, i / per, b, c);
    if (!p || c.work_items == b.max_work_items) return nullptr;
    ++c.work_items;
    return p + record_position(info,s,i);
  }
  void start_short(const std::uint8_t* record_bytes) {
    short_source=record_bytes+49; short_position=0; short_length=0;
    short_checked=short_payload_active=false; utf8={}; locator_span={};
  }
  bool parse_short(const WorkBudget& b, WorkCost& c) {
    // All 15 bytes of control, locator and inline/padding are consumed only
    // after the complete 64-byte owner record was authenticated. Each byte is
    // charged so tiny/changing work budgets preserve bounded forward progress.
    while(short_position<15 && c.work_items<b.max_work_items) {
      const auto pos=short_position++;const auto byte=short_source[pos];++c.work_items;
      SHARED_STRUCTURAL_DIAGNOSTIC(++structural.runtime_parse_bytes; ++structural.short_control_parse_bytes);
      if(pos==0) {
        short_length=byte;
        if(short_length>kShortTextCapacity) {fail("invalid short default length");return false;}
        if(short_length) locator_span.length=short_length;
      } else if(pos<5) locator_span.offset|=std::uint32_t(byte)<<((pos-1)*8);
      else {
        const auto i=pos-5;
        if(short_length) {
          if(i<short_length) {if(!utf8.consume(byte)){fail("invalid inline default UTF-8");return false;}}
          else if(byte) {fail("nonzero inline default padding");return false;}
        } else if(i<4) locator_span.length|=std::uint32_t(byte)<<(i*8);
        else if(byte) {fail("nonzero external default padding");return false;}
      }
    }
    if(short_position<15 || c.work_items==b.max_work_items) return false;
    if(!short_checked) {
      ++c.work_items;
      if(!locator_span.length || std::uint64_t(locator_span.offset)+locator_span.length>info.payload_bytes ||
          (!short_length && locator_span.length<=kShortTextCapacity) || utf8.remaining) {
        fail("invalid short default span or truncated UTF-8");return false;
      }
      short_checked=true;
      SHARED_STRUCTURAL_DIAGNOSTIC(if(short_length) ++structural.short_inline_records; else ++structural.short_external_records);
    }
    return true;
  }
  void start_stream(std::uint64_t start, std::uint64_t length) {
    stream_start = start; stream_size = length; stream_done = 0;
    stream_pos = stream_filled = 0; utf8 = {};
  }
  bool stream(bool checksum, const WorkBudget& b, WorkCost& c) {
    while (stream_done < stream_size) {
      if (c.work_items == b.max_work_items) return false;
      if (stream_pos == stream_filled) {
        const auto n = read_amount(static_cast<std::size_t>(std::min<std::uint64_t>(stream_buffer.size(), stream_size - stream_done)), b, c);
        if (!n) return false;
        if (!read_bytes(stream_start + stream_done, stream_buffer.data(), n, b, c)) return false;
        stream_pos = 0; stream_filled = n;
      }
      if (c.work_items==b.max_work_items) return false;
      const auto byte = stream_buffer[stream_pos++]; ++stream_done; ++c.work_items;
      if (checksum) { hash_byte(body_hash, byte); hash_byte(identity_hash, byte); }
      else if (!utf8.consume(byte)) { fail("invalid shared UTF-8 payload"); return false; }
    }
    if (!checksum && utf8.remaining) { fail("truncated shared UTF-8 sequence"); return false; }
    return true;
  }
};

PagedSharedResource::PagedSharedResource() : impl_(std::make_unique<Impl>()) {}
PagedSharedResource::~PagedSharedResource() { deactivate(); }
bool PagedSharedResource::open(std::shared_ptr<const ResourceReader> reader,
    RegistryNamespaceId ns, SharedBundleId bundle, MountEpoch epoch,
    const PagedSharedConfig& config, std::string* error) {
  deactivate();
  std::string local; if (!error) error = &local;
  error->clear();
  if (!reader || !ns || !epoch || !config.max_single_read_bytes ||
      !config.page_cache_capacity || config.page_cache_capacity > 64 ||
      !detail::OutputReuseCache::valid_config(config.output_reuse) || reader->size() < detail::kSharedHeaderBytes) {
    *error = "invalid shared open arguments"; return false;
  }
  auto& x = *impl_; x.config = config; x.reader = std::move(reader);
  std::array<std::uint8_t, detail::kSharedHeaderBytes> header{};
  for (std::size_t pos = 0; pos < header.size();) {
    const auto n = std::min(header.size() - pos, config.max_single_read_bytes);
    ++x.info.open_cost.read_calls; x.info.open_cost.read_bytes += n;
    if (!x.reader->read(pos, header.data() + pos, n)) { *error = "shared header read failed"; x.reader.reset(); return false; }
    pos += n;
  }
  x.info.open_cost.work_items = 1;
  if (!parse_layout(header.data(), x.reader->size(), &x.info, error) ||
      x.info.registry_namespace != ns || (bundle && x.info.shared_bundle_id != bundle)) {
    if (error->empty()) *error = "shared namespace/bundle binding mismatch";
    x.reader.reset(); return false;
  }
  x.cache = std::make_unique<std::uint8_t[]>(config.page_cache_capacity * x.info.page_bytes);
  x.slots = std::make_unique<Impl::CacheSlot[]>(config.page_cache_capacity);
  if (config.output_reuse.entries) x.output_reuse=std::make_unique<detail::OutputReuseCache>(config.output_reuse);
  x.identity = std::make_shared<SharedActivationIdentity>(); x.epoch = epoch;
  x.expected_hash = u64(header.data() + 24); x.identity_hash = namespace_hash(ns);
  x.start_stream(40, x.info.file_bytes - 40); x.phase = Impl::Phase::Hash;
  error->clear(); return true;
}

bool PagedSharedResource::open_verified(std::shared_ptr<const VerifiedResourceReader> reader,
    MountEpoch epoch, const PagedSharedConfig& cfg, std::string* error) {
  if (!reader || !reader->bootstrap_ready() || reader->spec().role!=ResourceReleaseRole::Shared) {
    deactivate(); if (error) *error="trusted shared bootstrap required"; return false;
  }
  const auto& spec=reader->spec();
  if (!open(reader,spec.registry_namespace,spec.shared_bundle_id,epoch,cfg,error)) return false;
  if (info().sections[0].count!=spec.runtime_ref_count || info().file_bytes!=spec.file_bytes) {
    deactivate(); if (error) *error="trusted shared metadata mismatch"; return false;
  }
  if (spec.resource_format != (short_text(info()) ? 4U : (colocated(info()) ? 3U : 2U))) {
    deactivate(); if (error) *error="trusted shared physical format pin mismatch"; return false;
  }
  impl_->verified=reader.get(); impl_->phase=Impl::Phase::Ready; return true;
}
const ResourceReleaseSpec* PagedSharedResource::verified_release() const {
  return ready() && impl_->verified ? &impl_->verified->spec() : nullptr;
}
SharedValidationCursor PagedSharedResource::validation_cursor() const {
  SharedValidationCursor cursor;
  cursor.context_ = {impl_->identity, impl_->epoch, 0};
  return cursor;
}
void PagedSharedResource::cancel_validation(const SharedValidationCursor& cursor) {
  if (impl_->identity && cursor.context_.identity == impl_->identity &&
      cursor.context_.mount_epoch == impl_->epoch &&
      impl_->phase != Impl::Phase::Ready) deactivate();
}
ValidationStepResult PagedSharedResource::advance_validation(
    const SharedValidationCursor& cursor, const WorkBudget& budget) {
  ValidationStepResult result; auto& x = *impl_; auto& c = result.cost;
  if (!x.identity || cursor.context_.identity != x.identity || cursor.context_.mount_epoch != x.epoch) {
    result.status = ValidationStepStatus::Stale; return result;
  }
  if (!valid_budget(budget)) { result.status = ValidationStepStatus::InvalidBudget; return result; }
  if (x.reader && !x.reader->healthy()) x.fail("resource activation is no longer healthy");
  using Phase = Impl::Phase;
  // Only the private adapter owns projection initialization. Ordinary public
  // validation cannot accidentally drive or spin inside that private phase.
  if (x.phase==Phase::Resident) {c.work_items=1;result.status=ValidationStepStatus::InProgress;return result;}
  while (c.work_items < budget.max_work_items && x.phase != Phase::Closed && x.phase != Phase::Error && x.phase != Phase::Ready) {
    if (x.phase == Phase::Hash) {
      if (!x.stream(true, budget, c)) break;
      if (x.body_hash != x.expected_hash || x.identity_hash != x.info.shared_bundle_id) { x.fail("shared checksum or content identity mismatch"); break; }
      x.phase = Phase::Pages; x.section = x.index = 0;
    } else if (x.phase == Phase::Pages) {
      if (x.section == 5) { x.phase = Phase::Lexemes; x.index = 0; continue; }
      if (x.index == x.info.sections[x.section].page_count) { ++x.section; x.index = 0; continue; }
      if (!x.page(x.section, x.index, budget, c)) break;
      // Cached pages need a work charge too: every page visit is bounded.
      if (c.work_items == budget.max_work_items) break;
      ++c.work_items; ++x.index;
    } else if (x.phase == Phase::Lexemes) {
      if (x.index == x.info.sections[0].count) {
        if (x.next_relation != x.info.sections[1].count) { x.fail("shared relation coverage mismatch"); break; }
        x.phase = Phase::Relations; x.index = 0; continue;
      }
      if (!x.secondary) {
        const auto* p = x.record(0, x.index, budget, c); if (!p) break;
        if (!lexeme_record(p, x.info, &x.validation_lexeme) || x.validation_lexeme.relation_start != x.next_relation) {
          x.fail("invalid shared lexeme or relation ownership"); break;
        }
        if (colocated(x.info)) {
          // The full validator compares the compatibility descriptor, whereas
          // trusted runtime relies on the same comparison at publication.
          if(short_text(x.info)) {x.start_short(p);x.stream_done=x.stream_pos=x.stream_filled=0;}
          else {x.locator_source=p+52; x.locator_position=0; x.locator_span={};}
        }
        x.secondary=true;
      }
      if (colocated(x.info)) {
        if(short_text(x.info)) {
          if(!x.parse_short(budget,c)) break;
          // The full validator must compare inline content with its generic
          // source too; trusted runtime relies on that exact publisher proof.
          if(x.short_length) {
            while(c.work_items<budget.max_work_items) {
              while(x.stream_pos<x.stream_filled && c.work_items<budget.max_work_items) {
                const auto i=x.stream_pos++;++c.work_items;
                if(x.stream_buffer[i]!=x.short_source[5+x.stream_start+i]) {x.fail("inline/default pool mismatch");break;}
              }
              if(x.stream_pos<x.stream_filled || x.phase==Phase::Error || x.stream_done==x.short_length) break;
              const auto n=x.read_amount(x.short_length-static_cast<std::size_t>(x.stream_done),budget,c);
              if(!n) break;
              if(!x.read_bytes(x.info.payload_offset+x.locator_span.offset+x.stream_done,x.stream_buffer.data(),n,budget,c)) break;
              x.stream_start=x.stream_done; x.stream_pos=0; x.stream_filled=n;
              x.stream_done+=n;
            }
            if(x.phase==Phase::Error || x.stream_done<x.short_length || x.stream_pos<x.stream_filled) break;
          }
        } else while (x.locator_position < 8 && c.work_items < budget.max_work_items) {
          const auto pos=x.locator_position++;
          auto& field=pos<4 ? x.locator_span.offset : x.locator_span.length;
          field |= std::uint32_t(x.locator_source[pos]) << ((pos%4)*8);
          ++c.work_items;
        }
        if (!short_text(x.info) && x.locator_position<8) break;
        const auto* p=x.record(3,x.validation_lexeme.default_payload,budget,c); if (!p) break;
        SharedStringDescriptor original;
        if (!string_record(p,x.info,&original) || original.offset!=x.locator_span.offset ||
            original.length!=x.locator_span.length) {x.fail("co-located default descriptor mismatch");break;}
      }
      x.next_relation += x.validation_lexeme.relation_count; x.secondary=false; ++x.index;
    } else if (x.phase == Phase::Relations) {
      if (x.index == x.info.sections[1].count) { x.phase = Phase::Selectors; x.index = 0; continue; }
      const auto* p = x.record(1, x.index, budget, c); if (!p) break;
      SharedOutputRecord r; if (!relation_record(p, x.info, &r)) { x.fail("invalid shared output relation"); break; }
      ++x.index;
    } else if (x.phase == Phase::Selectors) {
      if (x.index == x.info.sections[2].count) { x.phase = Phase::Strings; x.index = 0; continue; }
      const auto* p = x.record(2, x.index, budget, c); if (!p) break;
      SharedSelectorRecord r; if (!selector_record(p, x.info, &r)) { x.fail("invalid shared selector"); break; }
      ++x.index;
    } else if (x.phase == Phase::Strings) {
      if (x.index == x.info.sections[3].count) {
        if (x.next_string != x.info.payload_bytes) { x.fail("shared payload coverage mismatch"); break; }
        x.phase = Phase::Unique; x.index = 0; continue;
      }
      if (!x.secondary) {
        const auto* p = x.record(3, x.index, budget, c); if (!p) break;
        SharedStringDescriptor r;
        if (!string_record(p, x.info, &r) || r.offset != x.next_string) { x.fail("invalid shared string span"); break; }
        x.next_string += r.length; x.start_stream(x.info.payload_offset + r.offset, r.length); x.secondary = true;
      }
      if (!x.stream(false, budget, c)) break;
      x.secondary = false; ++x.index;
    } else if (x.phase == Phase::Unique) {
      UniquenessCostScope measured{x.info.uniqueness_cost, c, c};
      if (x.index == x.info.sections[4].count) { ++c.work_items; x.phase = Phase::Ready; break; }
      if (!x.secondary) {
        const auto* p = x.record(4, x.index, budget, c); if (!p) break;
        x.unique_ref = u32(p);
        if (!x.unique_ref || x.unique_ref > x.info.sections[0].count) { x.fail("invalid uniqueness reference"); break; }
        x.secondary = true;
      }
      const auto* p = x.record(0, x.unique_ref - 1, budget, c); if (!p) break;
      SharedLexemeRecord r;
      if (!lexeme_record(p, x.info, &r) || (x.index && !(x.previous_key < r.identity.stable_key))) {
        x.fail("duplicate key or invalid uniqueness coverage/order"); break;
      }
      x.previous_key = r.identity.stable_key; x.secondary = false; ++x.index;
    }
  }
  result.status = x.phase == Phase::Ready ? ValidationStepStatus::Ready :
      (x.phase == Phase::Error || x.phase == Phase::Closed ? ValidationStepStatus::Error : ValidationStepStatus::InProgress);
  result.error = x.error;
  if (result.status == ValidationStepStatus::Error) { x.cache.reset(); x.slots.reset(); x.reader.reset(); }
  return result;
}
bool PagedSharedResource::ready() const {
  return impl_->phase==Impl::Phase::Ready && impl_->reader && impl_->reader->healthy();
}
SharedActivationHandle PagedSharedResource::activation_handle() const {
  SharedActivationHandle handle;
  if (ready()) {
    handle.identity_ = impl_->identity;
    handle.mount_epoch_ = impl_->epoch;
    handle.registry_namespace_ = impl_->info.registry_namespace;
    handle.shared_bundle_id_ = impl_->info.shared_bundle_id;
  }
  return handle;
}
bool PagedSharedResource::activation_is_current(const SharedActivationHandle& handle) const {
  return ready() && handle.identity_ && handle.identity_ == impl_->identity &&
      handle.mount_epoch_ == impl_->epoch &&
      handle.registry_namespace_ == impl_->info.registry_namespace &&
      handle.shared_bundle_id_ == impl_->info.shared_bundle_id;
}
const PagedSharedInfo& PagedSharedResource::info() const { return impl_->info; }
SharedMemoryUsage PagedSharedResource::memory_usage() const {
  const auto& x = *impl_; SharedMemoryUsage m;
  m.owner_fixed_bytes = sizeof(*this) + sizeof(Impl);
  if (x.cache) m.cache_payload_allocated_bytes = x.config.page_cache_capacity * x.info.page_bytes;
  if (x.slots) {
    m.cache_metadata_allocated_bytes = x.config.page_cache_capacity * sizeof(Impl::CacheSlot);
    for (std::size_t i = 0; i < x.config.page_cache_capacity; ++i) if (x.slots[i].checked) ++m.valid_cached_pages;
  }
  m.stream_scratch_bytes = x.stream_buffer.size(); return m;
}
detail::OutputReuseCache* PagedSharedResource::output_reuse_cache() const {
  // Transient legacy I/O failure must be retried by normal reading, not erased
  // by a cache hit. Authentication failures are already latched by ready().
  return ready() && !impl_->io_failed ? impl_->output_reuse.get() : nullptr;
}
OutputReuseStats PagedSharedResource::output_reuse_stats() const {
  return impl_->output_reuse ? impl_->output_reuse->snapshot() : OutputReuseStats{};
}
SharedReadStatus PagedSharedResource::begin_read(SharedRecordKind kind, std::uint32_t index, SharedReadCursor* cursor) {
  auto& x = *impl_; const auto section = static_cast<std::size_t>(kind);
  if (!ready()) return SharedReadStatus::NotReady;
  if (!cursor || section >= 4 || (section == 0 && !index)) return SharedReadStatus::InvalidReference;
  const auto slot = section == 0 ? index - 1 : index;
  if (slot >= x.info.sections[section].count) return SharedReadStatus::InvalidReference;
  if (x.revision == std::numeric_limits<std::uint64_t>::max()) return SharedReadStatus::Error;
  x.reader->cancel_read_budgeted({x.identity,x.revision});
  ++x.revision; cursor->context_ = {x.identity, x.epoch, x.revision}; cursor->cancelled_ = false;
  x.read_kind = kind; x.read_index = slot; x.read_active = true; x.read_complete = x.read_second = false;
  x.io_failed = false; x.error.clear(); x.pending_read_bytes=0;
  x.locator_source=nullptr; x.locator_position=0; x.locator_span={};
  x.short_source=nullptr; x.short_length=x.short_position=0;
  x.short_checked=x.short_payload_active=false;
  return SharedReadStatus::InProgress;
}
SharedReadResult PagedSharedResource::advance_read(SharedReadCursor* cursor, const WorkBudget& budget) {
  SharedReadResult r; auto& x = *impl_; auto& c = r.cost;
  if (!cursor || !x.matches(cursor->context_)) { r.status = SharedReadStatus::Stale; return r; }
  r.context = cursor->context_; r.kind = x.read_kind;
  if (cursor->cancelled_) { r.status = SharedReadStatus::Cancelled; return r; }
  if (!valid_budget(budget)) { r.status = SharedReadStatus::InvalidBudget; return r; }
  if (!x.read_active) { r.status = SharedReadStatus::Stale; return r; }
  r.status = SharedReadStatus::InProgress;
  if (!x.read_second) {
    const auto* p = x.record(static_cast<std::size_t>(x.read_kind), x.read_index, budget, c);
    if (p) {
      bool valid = false;
      SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.runtime_records;
        x.structural.runtime_parse_bytes += x.read_kind==SharedRecordKind::Lexeme && short_text(x.info) ? 49 : kWidths[static_cast<std::size_t>(x.read_kind)]);
      switch (x.read_kind) {
        case SharedRecordKind::Lexeme:
          valid = lexeme_record(p, x.info, &x.read_lexeme); x.read_second = valid;
          if (valid && short_text(x.info)) x.start_short(p);
          else if (valid && colocated(x.info)) x.locator_source=p+52;
          else if (valid && x.resident_complete) x.locator_source=x.resident.get()+std::size_t(x.read_lexeme.default_payload)*8;
          break;
        case SharedRecordKind::Relation: valid = relation_record(p, x.info, &r.relation); break;
        case SharedRecordKind::Selector: valid = selector_record(p, x.info, &r.selector); break;
        case SharedRecordKind::StringDescriptor: valid = string_record(p, x.info, &r.string_descriptor); break;
      }
      if (!valid) x.fail("invalid record during shared read");
      else if (x.read_kind != SharedRecordKind::Lexeme) r.status = SharedReadStatus::Ready;
    }
  }
  if (x.read_second && x.phase != Impl::Phase::Error && !x.io_failed) {
    bool have_span=false;
    if (x.short_source) {
      if(x.parse_short(budget,c)) {r.string_descriptor=x.locator_span;have_span=true;}
    } else if (x.locator_source) {
      // Added byte decode/copy is charged, including tiny budgets. Identity was
      // obtained from this operation's authenticated authoritative record.
      while (x.locator_position<8 && c.work_items<budget.max_work_items) {
        const auto pos=x.locator_position++;
        auto& field=pos<4 ? x.locator_span.offset : x.locator_span.length;
        field |= std::uint32_t(x.locator_source[pos]) << ((pos%4)*8);
        ++c.work_items;
        SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.runtime_parse_bytes; ++x.structural.runtime_locator_copy_bytes);
      }
      if (x.locator_position==8 && c.work_items<budget.max_work_items) {
        ++c.work_items;
        if (!x.locator_span.length || std::uint64_t(x.locator_span.offset)+x.locator_span.length>x.info.payload_bytes)
          x.fail("invalid trusted default locator");
        else {
          r.string_descriptor=x.locator_span; have_span=true;
          SHARED_STRUCTURAL_DIAGNOSTIC(if (!x.read_complete) {
            if (colocated(x.info)) ++x.structural.layout_locator_hits;
            else ++x.structural.resident_locator_hits;
          });
        }
      }
    } else {
      const auto* p = x.record(3, x.read_lexeme.default_payload, budget, c);
      if (p) {
        SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.runtime_records; x.structural.runtime_parse_bytes+=8);
        if (!string_record(p, x.info, &r.string_descriptor)) x.fail("invalid default payload descriptor");
        else have_span=true;
      }
    }
    if (have_span) {
#if HANIME_TEXT_CHAIN
      x.read_default_span = r.string_descriptor;
#endif
      r.lexeme = x.read_lexeme; r.status = SharedReadStatus::Ready;
    }
  }
  if (x.phase == Impl::Phase::Error) {
    r.status = SharedReadStatus::Error; r.error = x.error;
    x.cache.reset(); x.slots.reset(); x.reader.reset(); x.resident.reset(); x.resident_complete=false;
  }
  if (x.io_failed) { r.status = SharedReadStatus::Error; r.error = x.error; x.read_active = false; }
  if (r.status == SharedReadStatus::Ready) x.read_complete = true;
  return r;
}
void PagedSharedResource::cancel_read(SharedReadCursor* cursor) {
  if (!cursor || !impl_->matches(cursor->context_)) return;
  impl_->reader->cancel_read_budgeted({impl_->identity,impl_->revision});
  impl_->pending_read_bytes=0;
  cursor->cancelled_ = true; impl_->read_active = false;
  impl_->short_source=nullptr; impl_->short_payload_active=false;
}
bool PagedSharedResource::result_is_current(const SharedReadResult& r) const {
  return r.status == SharedReadStatus::Ready && impl_->matches(r.context) && impl_->read_active;
}
SharedReadStatus PagedSharedResource::begin_payload_read(const SharedActivationHandle& activation,
    std::uint32_t id, char* dest, std::size_t capacity, SharedTextCursor* cursor) {
  if (!activation_is_current(activation)) return SharedReadStatus::Stale;
  if (!cursor || !dest || !capacity || reinterpret_cast<std::uintptr_t>(dest) >
      std::numeric_limits<std::uintptr_t>::max() - capacity) return SharedReadStatus::InvalidReference;
  SharedTextCursor next;
  const auto status = begin_read(SharedRecordKind::StringDescriptor,id,&next.descriptor_);
  if (status != SharedReadStatus::InProgress) return status;
  next.destination_ = dest; next.capacity_ = capacity; *cursor = next;
  return status;
}
#if HANIME_TEXT_CHAIN
SharedReadStatus PagedSharedResource::begin_default_payload_read(const SharedActivationHandle& activation,
    const SharedReadCursor& record, RuntimeLexemeRef ref, std::uint32_t id,
    char* dest, std::size_t capacity, SharedTextCursor* cursor) {
  auto& x = *impl_;
  if (!activation_is_current(activation) || !x.matches(record.context_) || record.cancelled_ ||
      !x.read_active || !x.read_complete || x.io_failed || x.read_kind != SharedRecordKind::Lexeme ||
      !ref || x.read_index != ref - 1 || x.read_lexeme.default_payload != id)
    return SharedReadStatus::Stale;
  if (!cursor || !dest || !capacity || reinterpret_cast<std::uintptr_t>(dest) >
      std::numeric_limits<std::uintptr_t>::max() - capacity) return SharedReadStatus::InvalidReference;
  // The span belongs to the current completed owner operation. A new begin
  // still retires that operation, including its read cursor and cancellation.
  const auto span = x.read_default_span;
  const auto* inline_bytes=x.short_checked && x.short_length ? x.short_source+5 : nullptr;
  const auto inline_length=x.short_length;
  SharedTextCursor next;
  const auto status = begin_read(SharedRecordKind::StringDescriptor, id, &next.descriptor_);
  if (status != SharedReadStatus::InProgress) return status;
  next.destination_ = dest; next.capacity_ = capacity;
  next.span_ = span; next.described_ = true; *cursor = next;
  if(inline_bytes) {
    x.short_source=inline_bytes; x.short_length=inline_length; x.short_payload_active=true;
    SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.short_handoffs);
  }
  return status;
}
#endif
SharedTextResult PagedSharedResource::advance_payload_read(SharedTextCursor* cursor, const WorkBudget& budget) {
  SharedTextResult out; auto& x = *impl_;
  if (!cursor || !x.matches(cursor->descriptor_.context_)) return out;
  out.context = cursor->descriptor_.context_;
  if (cursor->descriptor_.cancelled_) { out.status = SharedReadStatus::Cancelled; return out; }
  if (!valid_budget(budget)) { out.status = SharedReadStatus::InvalidBudget; return out; }
  if (!x.read_active) return out;
  if (!cursor->described_) {
    const auto step = advance_read(&cursor->descriptor_,budget);
    out.status = step.status; out.cost = step.cost;
    if (step.status == SharedReadStatus::Ready) {
      if (!result_is_current(step)) { out.status = SharedReadStatus::Stale; return out; }
      cursor->span_ = step.string_descriptor; cursor->described_ = true;
      out.required_bytes = cursor->span_.length;
      out.status = out.required_bytes > cursor->capacity_ ? SharedReadStatus::CapacityExceeded : SharedReadStatus::InProgress;
    }
    return out; // No second lower-level advance or unmetered payload read.
  }
  out.required_bytes = cursor->span_.length;
  if (out.required_bytes > cursor->capacity_) { out.status = SharedReadStatus::CapacityExceeded; return out; }
  if (cursor->filled_ < cursor->span_.length && x.short_payload_active) {
    // Same-owner verified record survives this serial operation. A successor,
    // cancellation or resource retirement invalidates the ticket before any
    // read/cache write; no verifier pointer and no partial view escapes.
    if(!x.short_source || x.short_length!=cursor->span_.length) {
      x.fail("invalid inline default handoff");out.status=SharedReadStatus::Error;return out;
    }
    const auto n=std::min<std::size_t>(cursor->span_.length-cursor->filled_,budget.max_work_items);
    std::copy_n(x.short_source+cursor->filled_,n,cursor->destination_+cursor->filled_);
    cursor->filled_+=n;out.cost.work_items=n;out.copied_bytes=n;
    SHARED_STRUCTURAL_DIAGNOSTIC(x.structural.short_payload_copy_bytes+=n);
  } else if (cursor->filled_ < cursor->span_.length) {
    const auto n = x.pending_read_bytes ? x.pending_read_bytes :
        std::min({std::size_t(cursor->span_.length)-cursor->filled_,
        budget.max_read_bytes,budget.max_work_items,x.config.max_single_read_bytes});
    if (!x.read_bytes(x.info.payload_offset+cursor->span_.offset+cursor->filled_,
                      cursor->destination_+cursor->filled_,n,budget,out.cost)) {
      if (x.io_failed || x.phase==Impl::Phase::Error) {
        x.read_active=false; out.status=SharedReadStatus::Error;
      } else out.status=SharedReadStatus::InProgress;
      return out;
    }
    if (!out.cost.work_items) out.cost.work_items=n; // Legacy direct copy.
    cursor->filled_ += n;
    out.copied_bytes = n;
  }
  out.status = cursor->filled_ == cursor->span_.length ? SharedReadStatus::Ready : SharedReadStatus::InProgress;
  if (out.status == SharedReadStatus::Ready) out.text = {cursor->destination_,cursor->filled_};
  return out;
}
void PagedSharedResource::cancel_payload_read(SharedTextCursor* cursor) {
  if (cursor) cancel_read(&cursor->descriptor_);
}
bool PagedSharedResource::text_is_current(const SharedTextResult& out) const {
  return out.status == SharedReadStatus::Ready && impl_->matches(out.context) && impl_->read_active;
}
const void* detail::SharedStructuralAccess::allocation(const PagedSharedResource& owner) {
  return owner.impl_->resident.get();
}
detail::SharedStructuralStats detail::SharedStructuralAccess::stats(const PagedSharedResource& owner) {
  return owner.impl_->structural;
}
ValidationStepResult detail::SharedStructuralAccess::advance_resident(PagedSharedResource& owner,
    const WorkBudget& budget, Allocate allocate, Release release) {
  using Impl=PagedSharedResource::Impl;
  ValidationStepResult result; auto& x=*owner.impl_; auto& cost=result.cost;
  if (!valid_budget(budget)) {result.status=ValidationStepStatus::InvalidBudget;return result;}
  SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.initialization_advances);
  auto finish=[&]() {
    SHARED_STRUCTURAL_DIAGNOSTIC(
      x.structural.initialization_cost.read_calls+=cost.read_calls;
      x.structural.initialization_cost.read_bytes+=cost.read_bytes;
      x.structural.initialization_cost.work_items+=cost.work_items);
    result.status=x.phase==Impl::Phase::Ready && x.resident_complete ? ValidationStepStatus::Ready :
        x.phase==Impl::Phase::Resident ? ValidationStepStatus::InProgress : ValidationStepStatus::Error;
    result.error=x.error;
    if (result.status==ValidationStepStatus::Error && x.resident_started) {
      x.resident.reset(); x.resident_complete=false; x.cache.reset(); x.slots.reset(); x.reader.reset();
    }
    return result;
  };
  if (!x.resident_started) {
    if (!owner.ready() || !x.verified || colocated(x.info) || x.read_active || !allocate || !release) {
      result.status=ValidationStepStatus::Error; result.error="resident projection requires unused trusted original activation and explicit allocator";
      return result;
    }
    x.resident_started=true; x.phase=Impl::Phase::Resident;
    x.reader->cancel_read_budgeted({x.identity,x.revision});
    try { x.identity=std::make_shared<SharedActivationIdentity>(); }
    catch (const std::bad_alloc&) { x.fail("resident activation allocation failed");return finish(); }
    // VerifiedResourceReader requires a nonzero serial ticket even for the
    // private initialization owner. First runtime begin advances to revision 2.
    x.revision=1;
    x.pending_read_bytes=0; x.slice_assigned=false;
    x.structural.table_entries=x.info.sections[3].count;
    if (x.structural.table_entries>std::numeric_limits<std::size_t>::max()/8) {
      x.fail("resident projection size overflow");return finish();
    }
    x.structural.table_bytes=x.structural.table_entries*8;
    ++cost.work_items; SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.allocation_attempts);
    x.resident=decltype(x.resident)(static_cast<std::uint8_t*>(allocate(x.structural.table_bytes)),Impl::ExternalRelease{release});
    if (!x.resident) {SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.allocation_failures);x.fail("resident projection allocation failed; no fallback");return finish();}
  }
  if (x.phase!=Impl::Phase::Resident) return finish();
  if (!x.reader || !x.reader->healthy()) {x.fail("resident source no longer healthy");return finish();}
  while (cost.work_items<budget.max_work_items && x.resident_index<x.structural.table_entries) {
    if (!x.resident_source) {
      x.resident_source=x.record(3,static_cast<std::uint32_t>(x.resident_index),budget,cost);
      if (!x.resident_source) break;
      x.resident_stage=0; x.resident_span={};
    }
    // Parse then check then copy. No zero-fill or unbounded constructor copy.
    while (x.resident_stage<8 && cost.work_items<budget.max_work_items) {
      const auto pos=x.resident_stage++;
      auto& field=pos<4 ? x.resident_span.offset : x.resident_span.length;
      field |= std::uint32_t(x.resident_source[pos]) << ((pos%4)*8);
      ++cost.work_items; SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.initialization_parse_bytes);
    }
    if (x.resident_stage==8 && cost.work_items<budget.max_work_items) {
      ++cost.work_items; ++x.resident_stage;
      if (!x.resident_span.length || x.resident_span.offset!=x.resident_next_offset ||
          std::uint64_t(x.resident_span.offset)+x.resident_span.length>x.info.payload_bytes) {
        x.fail("resident descriptor bounds/coverage mismatch");break;
      }
    }
    while (x.resident_stage>=9 && x.resident_stage<17 && cost.work_items<budget.max_work_items) {
      const auto byte=x.resident_stage++-9;
      x.resident.get()[x.resident_index*8+byte]=x.resident_source[byte];
      ++cost.work_items; SHARED_STRUCTURAL_DIAGNOSTIC(++x.structural.initialization_store_bytes);
    }
    if (x.resident_stage<17) break;
    x.resident_next_offset+=x.resident_span.length;
    ++x.resident_index; x.structural.initialized_entries=x.resident_index; x.resident_source=nullptr;
  }
  if (x.phase==Impl::Phase::Resident && x.resident_index==x.structural.table_entries && cost.work_items<budget.max_work_items) {
    ++cost.work_items;
    if (x.resident_next_offset!=x.info.payload_bytes) x.fail("resident descriptor final coverage mismatch");
    else {x.resident_complete=true;x.phase=Impl::Phase::Ready;}
  }
  return finish();
}
void PagedSharedResource::deactivate() {
  if (impl_->reader) impl_->reader->cancel_read_budgeted({impl_->identity,impl_->revision});
  // Retire in the existing owner storage. Aggregate move-assignment creates a
  // full Impl temporary on the target stack; the projection's fixed cursors
  // must not silently grow that frame. Default construction cannot allocate
  // or fail, and each old owner is destroyed exactly once before replacement.
  static_assert(noexcept(Impl{}), "shared owner reset must not throw");
  auto* const storage=impl_.get();
  storage->~Impl();
  ::new (static_cast<void*>(storage)) Impl{};
}
}  // namespace hanime
