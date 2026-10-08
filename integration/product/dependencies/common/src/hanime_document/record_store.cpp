#include "../../core_memory.hpp"
#include "record_store.hpp"
#include "save_diagnostic.hpp"

#include <array>
#include <limits>

namespace hanime::documents {
namespace {

constexpr std::array<char, 8> kMagic{{'H','D','O','C','R','E','C','1'}};
constexpr std::size_t kHeaderBytes = RecordStore::kHeaderBytes;

void put16(draftor::memory::String& out, const std::size_t at, const std::uint16_t value) {
  out[at] = static_cast<char>(value & 0xFFU);
  out[at + 1U] = static_cast<char>((value >> 8U) & 0xFFU);
}
void put32(draftor::memory::String& out, const std::size_t at, const std::uint32_t value) {
  for (std::size_t i = 0; i < 4U; ++i) out[at + i] = static_cast<char>((value >> (8U * i)) & 0xFFU);
}
void put64(draftor::memory::String& out, const std::size_t at, const std::uint64_t value) {
  for (std::size_t i = 0; i < 8U; ++i) out[at + i] = static_cast<char>((value >> (8U * i)) & 0xFFU);
}
std::uint16_t get16(const std::string_view in, const std::size_t at) {
  return static_cast<std::uint16_t>(static_cast<unsigned char>(in[at])) |
         (static_cast<std::uint16_t>(static_cast<unsigned char>(in[at + 1U])) << 8U);
}
std::uint32_t get32(const std::string_view in, const std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < 4U; ++i) value |= static_cast<std::uint32_t>(static_cast<unsigned char>(in[at + i])) << (8U * i);
  return value;
}
std::uint64_t get64(const std::string_view in, const std::size_t at) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8U; ++i) value |= static_cast<std::uint64_t>(static_cast<unsigned char>(in[at + i])) << (8U * i);
  return value;
}

Status decodeHeaderImpl(const std::string_view bytes, const RecordKind expected_kind,
                        const std::size_t max_payload, RecordHeader* out) {
  if (!out || bytes.size() != kHeaderBytes) return Status::Corrupt;
  for (std::size_t i = 0; i < kMagic.size(); ++i) if (bytes[i] != kMagic[i]) return Status::Corrupt;
  if (get16(bytes, 10U) != 1U) return Status::UnsupportedVersion;
  if (get16(bytes, 8U) != static_cast<std::uint16_t>(expected_kind)) return Status::Corrupt;
  RecordHeader value{};
  value.generation = get64(bytes, 12U);
  value.payload_size = get32(bytes, 20U);
  value.payload_crc = get32(bytes, 24U);
  const std::uint32_t header_crc = get32(bytes, 28U);
  if (value.generation == 0U) return Status::Corrupt;
  if (value.payload_size > max_payload) return Status::TooLarge;
  std::array<char, kHeaderBytes> copy{};
  std::copy(bytes.begin(), bytes.end(), copy.begin());
  std::fill(copy.begin() + 28U, copy.end(), '\0');
  if (save_diag::measured(save_diag::Op::HeaderCrc, copy.size(), [&] { return crc32(std::string_view(copy.data(), copy.size())); }, [](auto) { return 1; }) != header_crc) return Status::Corrupt;
  *out = value;
  return Status::Ok;
}

draftor::memory::String encodeHeaderImpl(const RecordKind kind, const std::uint64_t generation,
                             const std::size_t payload_size,
                             const std::uint32_t payload_crc) {
  if (generation == 0U || payload_size > std::numeric_limits<std::uint32_t>::max()) return {};
  draftor::memory::String header(kHeaderBytes, '\0');
  for (std::size_t i = 0; i < kMagic.size(); ++i) header[i] = kMagic[i];
  put16(header, 8U, static_cast<std::uint16_t>(kind)); put16(header, 10U, 1U);
  put64(header, 12U, generation); put32(header, 20U, static_cast<std::uint32_t>(payload_size));
  put32(header, 24U, payload_crc); put32(header, 28U, 0U); put32(header, 28U, save_diag::measured(save_diag::Op::HeaderCrc, header.size(), [&] { return crc32(header); }, [](auto) { return 1; }));
  return header;
}

Status decodeOwned(draftor::memory::String bytes, const RecordKind kind,
                   const std::size_t max_payload, Record* out) {
  if (!out || bytes.size() < kHeaderBytes) return Status::Corrupt;
  RecordHeader header{};
  const Status header_status = decodeHeaderImpl(std::string_view(bytes).substr(0U, kHeaderBytes),
                                                kind, max_payload, &header);
  if (header_status != Status::Ok) return header_status;
  if (bytes.size() != kHeaderBytes + header.payload_size) return Status::Corrupt;
  const std::string_view payload(bytes.data() + kHeaderBytes, header.payload_size);
  if (save_diag::measured(save_diag::Op::PayloadCrc, payload.size(), [&] { return crc32(payload); }, [](auto) { return 1; }) != header.payload_crc) return Status::Corrupt;
  bytes.erase(0U, kHeaderBytes);
  out->generation = header.generation;
  out->payload = std::move(bytes);
  return Status::Ok;
}

struct HeaderSlot {
  Status read_status = Status::NotFound;
  Status status = Status::NotFound;
  RecordHeader header{};
};

}  // namespace

draftor::memory::String RecordStore::encode(const RecordKind kind,
                                const std::uint64_t generation,
                                const std::string_view payload) {
  draftor::memory::String out = encodeHeaderImpl(kind, generation, payload.size(), save_diag::measured(save_diag::Op::PayloadCrc, payload.size(), [&] { return crc32(payload); }, [](auto) { return 1; }));
  if (out.empty()) return {};
  out.append(payload);
  return out;
}

Status RecordStore::decode(const std::string_view bytes,
                           const RecordKind expected_kind,
                           const std::size_t max_payload,
                           Record* out) {
  if (!out || bytes.size() < kHeaderBytes) return Status::Corrupt;
  RecordHeader header{};
  const Status header_status = decodeHeaderImpl(bytes.substr(0U, kHeaderBytes), expected_kind,
                                                max_payload, &header);
  if (header_status != Status::Ok) return header_status;
  if (bytes.size() != kHeaderBytes + header.payload_size) return Status::Corrupt;
  const std::string_view payload = bytes.substr(kHeaderBytes);
  if (save_diag::measured(save_diag::Op::PayloadCrc, payload.size(), [&] { return crc32(payload); }, [](auto) { return 1; }) != header.payload_crc) return Status::Corrupt;
  out->generation = header.generation;
  out->payload.assign(payload.data(), payload.size());
  return Status::Ok;
}

draftor::memory::String RecordStore::encodeHeader(const RecordKind kind,
                                      const std::uint64_t generation,
                                      const std::size_t payload_size,
                                      const std::uint32_t payload_crc) {
  return encodeHeaderImpl(kind, generation, payload_size, payload_crc);
}

Status RecordStore::decodeHeader(const std::string_view bytes,
                                 const RecordKind expected_kind,
                                 const std::size_t max_payload,
                                 RecordHeader* out) noexcept {
  return decodeHeaderImpl(bytes, expected_kind, max_payload, out);
}

PairPeek RecordStore::peekPair(const std::string_view base,
                               const RecordKind kind,
                               const std::size_t max_payload) {
  return save_diag::measuredResult(save_diag::Op::PairPeek, [&]() -> PairPeek {
  const draftor::memory::String paths[2] = {draftor::memory::String(base) + ".A", draftor::memory::String(base) + ".B"};
  HeaderSlot slots[2];
  for (std::size_t i = 0; i < 2U; ++i) {
    const ReadResult read = storage_.readPrefix(paths[i], kHeaderBytes);
    slots[i].read_status = read.status;
    slots[i].status = read.status == Status::Ok
        ? decodeHeaderImpl(read.bytes, kind, max_payload, &slots[i].header)
        : read.status;
  }
  const bool valid[2] = {slots[0].status == Status::Ok, slots[1].status == Status::Ok};
  if (valid[0] || valid[1]) {
    const std::uint8_t chosen = valid[0] && valid[1]
        ? (slots[0].header.generation >= slots[1].header.generation ? 0U : 1U)
        : (valid[0] ? 0U : 1U);
    const bool invalid_other = slots[1U - chosen].status != Status::Ok &&
                               slots[1U - chosen].status != Status::NotFound;
    return {invalid_other ? Status::OkDegraded : Status::Ok,
            slots[chosen].header.generation, chosen, valid[1U - chosen], slots[chosen].header.payload_size};
  }
  if (slots[0].read_status == Status::MediaUnavailable || slots[1].read_status == Status::MediaUnavailable) return {Status::MediaUnavailable};
  if (slots[0].read_status == Status::IoError || slots[1].read_status == Status::IoError) return {Status::IoError};
  if (slots[0].status == Status::UnsupportedVersion || slots[1].status == Status::UnsupportedVersion) return {Status::UnsupportedVersion};
  if (slots[0].status == Status::TooLarge || slots[1].status == Status::TooLarge) return {Status::TooLarge};
  if (slots[0].read_status == Status::NotFound && slots[1].read_status == Status::NotFound) return {Status::NotFound};
  return {Status::Corrupt};

  });
}

PairLoad RecordStore::loadPair(const std::string_view base,
                               const RecordKind kind,
                               const std::size_t max_payload,
                               const std::size_t admitted_payload) {
  return save_diag::measuredResult(save_diag::Op::PairLoad, [&]() -> PairLoad {
  const PairPeek peek = peekPair(base, kind, max_payload);
  if (peek.status != Status::Ok && peek.status != Status::OkDegraded) return {peek.status};
  // Capacity refusal is not corruption: never fall back to an older small body.
  if (peek.payload_size > admitted_payload) return {Status::TooLarge};
  const draftor::memory::String paths[2] = {draftor::memory::String(base) + ".A", draftor::memory::String(base) + ".B"};
  auto load = [&](const std::uint8_t slot, Record* record) {
    ReadResult read = storage_.read(paths[slot], kHeaderBytes + std::min(max_payload, admitted_payload));
    if (read.status != Status::Ok) return read.status;
    return decodeOwned(std::move(read.bytes), kind, max_payload, record);
  };
  Record chosen{};
  const Status chosen_status = load(peek.selected_slot, &chosen);
  if (chosen_status == Status::Ok) {
    return {peek.status, std::move(chosen), false, peek.status == Status::OkDegraded};
  }
  if (peek.other_header_valid) {
    Record fallback{};
    if (load(static_cast<std::uint8_t>(1U - peek.selected_slot), &fallback) == Status::Ok) {
      return {Status::OkDegraded, std::move(fallback), true, true};
    }
  }
  return {chosen_status};

  });
}

Status RecordStore::commitPair(const std::string_view base,
                               const RecordKind kind,
                               const std::uint64_t generation,
                               const std::string_view payload) {
  return commitPairParts(base, kind, generation, &payload, 1U);
}

Status RecordStore::commitPairParts(const std::string_view base,
                                    const RecordKind kind,
                                    const std::uint64_t generation,
                                    const std::string_view* payload_parts,
                                    const std::size_t part_count) {
  return save_diag::measuredStatus(save_diag::Op::PairCommit, [&]() -> Status {
  if (generation == 0U || (!payload_parts && part_count != 0U)) return Status::Conflict;
  std::size_t payload_size = 0U;
  for (std::size_t i = 0; i < part_count; ++i) {
    if (payload_parts[i].size() > std::numeric_limits<std::uint32_t>::max() - payload_size) return Status::TooLarge;
    payload_size += payload_parts[i].size();
  }
  const MediumSession before = storage_.mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const draftor::memory::String header = encodeHeaderImpl(kind, generation, payload_size,
                                              save_diag::measured(save_diag::Op::PayloadCrc, payload_size, [&] { return crc32Parts(payload_parts, part_count); }, [](auto) { return 1; }));
  if (header.empty()) return Status::TooLarge;
  // Slot selection only reads authenticated headers.  The previous payload may
  // legitimately be larger than its replacement (for example, clearing a
  // create intent), so the new payload size must not be used as the old limit.
  const PairPeek peek = peekPair(base, kind,
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()));
  std::uint8_t target = 0U;
  if (peek.status == Status::Ok || peek.status == Status::OkDegraded) {
    target = static_cast<std::uint8_t>(1U - peek.selected_slot);
  } else if (peek.status != Status::NotFound) {
    return peek.status;
  }
  const draftor::memory::String path = draftor::memory::String(base) + (target == 0U ? ".A" : ".B");
  if (!storage_.writesTruncate()) {
    const Status removed = storage_.remove(path);
    if (removed != Status::Ok && removed != Status::NotFound) return removed;
  }
  if (storage_.mediumSession().mount_epoch != before.mount_epoch) return Status::SessionChanged;
  std::array<std::string_view, 4> all_parts{};
  if (part_count + 1U > all_parts.size()) return Status::Conflict;
  all_parts[0] = header;
  for (std::size_t i = 0; i < part_count; ++i) all_parts[i + 1U] = payload_parts[i];
  const Status written = storage_.writeParts(path, all_parts.data(), part_count + 1U);
  if (written != Status::Ok) return written;
  if (storage_.mediumSession().mount_epoch != before.mount_epoch) return Status::SessionChanged;
  return storage_.verifyParts(path, all_parts.data(), part_count + 1U);

  });
}

Status RecordStore::resetUnrecoverablePair(const std::string_view base,
                                           const RecordKind kind,
                                           const std::size_t max_payload) {
  const MediumSession before = storage_.mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const PairPeek peek = peekPair(base, kind, max_payload);
  if (peek.status == Status::Ok || peek.status == Status::OkDegraded ||
      peek.status == Status::NotFound) return Status::Ok;
  if (peek.status == Status::MediaUnavailable || peek.status == Status::IoError) {
    return peek.status;
  }
  for (const char* suffix : {".A", ".B"}) {
    const Status removed = storage_.remove(draftor::memory::String(base) + suffix);
    if (removed != Status::Ok && removed != Status::NotFound) return removed;
  }
  return storage_.mediumSession().mount_epoch == before.mount_epoch
      ? Status::Ok : Status::SessionChanged;
}

}  // namespace hanime::documents
