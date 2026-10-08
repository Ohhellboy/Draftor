#include "../../core_memory.hpp"
#include "record_codec.hpp"
#include "save_diagnostic.hpp"

#include <cstring>
#include <limits>

namespace hanime::documents {
namespace {

class Writer {
 public:
  void u8(const std::uint8_t v) { out_.push_back(static_cast<char>(v)); }
  void u32(const std::uint32_t v) {
    for (std::size_t i = 0; i < 4U; ++i) u8(static_cast<std::uint8_t>(v >> (8U * i)));
  }
  void u64(const std::uint64_t v) {
    for (std::size_t i = 0; i < 8U; ++i) u8(static_cast<std::uint8_t>(v >> (8U * i)));
  }
  void id(const DocumentId& value) {
    out_.append(reinterpret_cast<const char*>(value.bytes.data()), value.bytes.size());
  }
  void string(const std::string_view value) {
    u32(static_cast<std::uint32_t>(value.size()));
    out_.append(value.data(), value.size());
  }
  void length(const std::size_t value) { u32(static_cast<std::uint32_t>(value)); }
  draftor::memory::String finish() { return std::move(out_); }
 private:
  draftor::memory::String out_;
};

class Reader {
 public:
  explicit Reader(const std::string_view bytes) : bytes_(bytes) {}
  bool u8(std::uint8_t* out) {
    if (out == nullptr || at_ + 1U > bytes_.size()) return false;
    *out = static_cast<unsigned char>(bytes_[at_++]); return true;
  }
  bool u32(std::uint32_t* out) {
    if (out == nullptr || at_ + 4U > bytes_.size()) return false;
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4U; ++i) value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[at_++])) << (8U * i);
    *out = value; return true;
  }
  bool u64(std::uint64_t* out) {
    if (out == nullptr || at_ + 8U > bytes_.size()) return false;
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i) value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes_[at_++])) << (8U * i);
    *out = value; return true;
  }
  bool size(std::size_t* out) {
    std::uint64_t value = 0; if (!u64(&value) || value > std::numeric_limits<std::size_t>::max()) return false;
    *out = static_cast<std::size_t>(value); return true;
  }
  bool id(DocumentId* out) {
    if (out == nullptr || at_ + out->bytes.size() > bytes_.size()) return false;
    std::memcpy(out->bytes.data(), bytes_.data() + at_, out->bytes.size()); at_ += out->bytes.size();
    return !out->empty();
  }
  bool string(draftor::memory::String* out, const std::size_t max) {
    std::uint32_t size = 0; if (out == nullptr || !u32(&size) || size > max || at_ + size > bytes_.size()) return false;
    out->assign(bytes_.data() + at_, size); at_ += size; return true;
  }
  bool done() const { return at_ == bytes_.size(); }
 private:
  std::string_view bytes_; std::size_t at_ = 0;
};

bool validSnapshot(const Snapshot& value) {
  return value.text.size() <= kMaxDocumentBytes && isValidUtf8(value.text) &&
         isUtf8Boundary(value.text, value.cursor_byte) &&
         isUtf8Boundary(value.text, value.viewport.byte_offset);
}

}  // namespace

draftor::memory::String encodeCollection(const CollectionState& v) {
  Writer w; w.u8(1U); w.id(v.collection_id); w.u64(v.id_counter); w.u64(v.temp_name_counter); w.u64(v.opened_order_counter); return w.finish();
}
Status decodeCollection(const std::string_view b, CollectionState* out) noexcept {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0; CollectionState v{};
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.id(&v.collection_id) || !r.u64(&v.id_counter) || !r.u64(&v.temp_name_counter) || !r.u64(&v.opened_order_counter) || !r.done()) return Status::Corrupt;
  *out = v; return Status::Ok;
}

draftor::memory::String encodeMetadata(const DocumentMetadata& v) {
  Writer w; w.u8(1U); w.id(v.id); w.u8(static_cast<std::uint8_t>(v.area)); w.u64(v.opened_order); w.string(v.name); return w.finish();
}
// S3: allocation failures propagate to the application error boundary.
Status decodeMetadata(const std::string_view b, DocumentMetadata* out) {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, area = 0; DocumentMetadata v{};
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.id(&v.id) || !r.u8(&area) || area > 1U || !r.u64(&v.opened_order) || !r.string(&v.name, kMaxNameBytes) || !r.done()) return Status::Corrupt;
  if (!isValidUtf8(v.name) || v.name.empty()) return Status::Corrupt;
  v.area = static_cast<DocumentArea>(area); *out = v; return Status::Ok;
}

draftor::memory::String encodeSnapshot(const Snapshot& v) {
  draftor::memory::String out = encodeSnapshotPrefix(v);
  out.append(v.text);
  return out;
}
draftor::memory::String encodeSnapshotPrefix(const Snapshot& v) {
  return encodeSnapshotPrefix(v.text, v.cursor_byte, v.viewport);
}
draftor::memory::String encodeSnapshotPrefix(const std::string_view text,
                                 const std::size_t cursor_byte,
                                 const VisualAnchor viewport) {
  return encodeSnapshotPrefix(
      text.size(),
      save_diag::measured(save_diag::Op::BodyCrc, text.size(),
                          [&] { return crc32(text); }, [](auto) { return 1; }),
      cursor_byte, viewport);
}
draftor::memory::String encodeSnapshotPrefix(const std::size_t text_size,
                                 const std::uint32_t text_crc32,
                                 const std::size_t cursor_byte,
                                 const VisualAnchor viewport) {
  Writer w; w.u8(1U); w.u64(static_cast<std::uint64_t>(cursor_byte)); w.u64(static_cast<std::uint64_t>(viewport.byte_offset));
  w.u8(static_cast<std::uint8_t>(viewport.affinity)); w.u32(viewport.layout_revision); w.u32(text_crc32); w.length(text_size); return w.finish();
}
Status decodeSnapshot(const std::string_view b, Snapshot* out) {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, affinity = 0; Snapshot v{}; std::uint32_t stored_crc = 0;
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.size(&v.cursor_byte) || !r.size(&v.viewport.byte_offset) || !r.u8(&affinity) || affinity > 1U ||
      !r.u32(&v.viewport.layout_revision) || !r.u32(&stored_crc) || !r.string(&v.text, kMaxDocumentBytes) || !r.done()) return Status::Corrupt;
  v.viewport.affinity = static_cast<Affinity>(affinity); v.text_crc32 = crc32(v.text);
  if (stored_crc != v.text_crc32 || !validSnapshot(v)) return Status::Corrupt;
  *out = std::move(v); return Status::Ok;
}

Status decodeSnapshotOwned(draftor::memory::String b, Snapshot* out) noexcept {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, affinity = 0; Snapshot v{};
  std::uint32_t stored_crc = 0, text_size = 0;
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.size(&v.cursor_byte) || !r.size(&v.viewport.byte_offset) ||
      !r.u8(&affinity) || affinity > 1U ||
      !r.u32(&v.viewport.layout_revision) || !r.u32(&stored_crc) ||
      !r.u32(&text_size) || b.size() != 30U + text_size) return Status::Corrupt;
  v.viewport.affinity = static_cast<Affinity>(affinity);
  const std::string_view text(b.data() + 30U, text_size);
  v.text_crc32 = crc32(text);
  if (stored_crc != v.text_crc32 || text_size > kMaxDocumentBytes ||
      !isValidUtf8(text) || !isUtf8Boundary(text, v.cursor_byte) ||
      !isUtf8Boundary(text, v.viewport.byte_offset)) return Status::Corrupt;
  b.erase(0U, 30U);
  v.text = std::move(b);
  *out = std::move(v);
  return Status::Ok;
}

draftor::memory::String encodeCurrent(const CurrentRecord& v) { Writer w; w.u8(1U); w.u8(v.has_current ? 1U : 0U); if (v.has_current) w.id(v.document_id); return w.finish(); }
Status decodeCurrent(const std::string_view b, CurrentRecord* out) noexcept {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, has = 0; CurrentRecord v{};
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.u8(&has) || has > 1U || (has && !r.id(&v.document_id)) || !r.done()) return Status::Corrupt;
  v.has_current = has != 0; *out = v; return Status::Ok;
}

draftor::memory::String encodePosition(const PositionRecord& v) {
  Writer w; w.u8(1U); w.id(v.document_id); w.u64(v.body_generation); w.u32(v.text_crc32); w.u64(v.cursor_byte); w.u64(v.viewport.byte_offset); w.u8(static_cast<std::uint8_t>(v.viewport.affinity)); w.u32(v.viewport.layout_revision); return w.finish();
}
Status decodePosition(const std::string_view b, PositionRecord* out) noexcept {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, affinity = 0; PositionRecord v{};
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.id(&v.document_id) || !r.u64(&v.body_generation) || !r.u32(&v.text_crc32) || !r.size(&v.cursor_byte) || !r.size(&v.viewport.byte_offset) || !r.u8(&affinity) || affinity > 1U || !r.u32(&v.viewport.layout_revision) || !r.done()) return Status::Corrupt;
  v.viewport.affinity = static_cast<Affinity>(affinity); *out = v; return Status::Ok;
}

draftor::memory::String encodeCreateIntent(const CreateIntent& v) {
  Writer w; w.u8(v.external_operation ? 2U : 1U); w.u8(v.active ? 1U : 0U);
  if (v.active) w.string(encodeMetadata(v.metadata));
  if (v.external_operation) { w.u64(v.external_operation); w.u64(v.source_session); w.u64(v.source_entry); w.u64(v.source_size); }
  return w.finish();
}
Status decodeCreateIntent(const std::string_view b, CreateIntent* out) {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, active = 0; CreateIntent v{}; draftor::memory::String meta;
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U && version != 2U) return Status::UnsupportedVersion;
  if (!r.u8(&active) || active > 1U || (active && !r.string(&meta, 256U))) return Status::Corrupt;
  if (version == 2U && (!active || !r.u64(&v.external_operation) || !v.external_operation || !r.u64(&v.source_session) || !r.u64(&v.source_entry) || !r.u64(&v.source_size))) return Status::Corrupt;
  if (!r.done()) return Status::Corrupt;
  v.active = active != 0; if (v.active && decodeMetadata(meta, &v.metadata) != Status::Ok) return Status::Corrupt; *out = std::move(v); return Status::Ok;
}

draftor::memory::String encodeMigration(const MigrationRecord& v) { Writer w; w.u8(1U); w.u8(static_cast<std::uint8_t>(v.state)); if (v.state != MigrationRecord::State::None) w.id(v.target_id); return w.finish(); }
Status decodeMigration(const std::string_view b, MigrationRecord* out) noexcept {
  if (!out) return Status::Corrupt;
  Reader r(b); std::uint8_t version = 0, state = 0; MigrationRecord v{};
  if (!r.u8(&version)) return Status::Corrupt;
  if (version != 1U) return Status::UnsupportedVersion;
  if (!r.u8(&state) || state > 2U) return Status::Corrupt;
  v.state = static_cast<MigrationRecord::State>(state);
  if ((v.state != MigrationRecord::State::None && !r.id(&v.target_id)) || !r.done()) return Status::Corrupt;
  *out = v; return Status::Ok;
}

}  // namespace hanime::documents
