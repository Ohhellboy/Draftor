#include "../../core_memory.hpp"
#pragma once

#include "storage_port.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace hanime::documents {

enum class RecordKind : std::uint16_t {
  Collection = 1,
  Metadata = 2,
  Snapshot = 3,
  Position = 4,
  Current = 5,
  CreateIntent = 6,
  Migration = 7,
  ExternalCreateReceipt = 8,
};

struct Record {
  std::uint64_t generation = 0;
  draftor::memory::String payload;
};

struct PairLoad {
  Status status = Status::NotFound;
  Record record{};
  bool used_older_slot = false;
  bool had_invalid_slot = false;
};

struct PairPeek {
  Status status = Status::NotFound;
  std::uint64_t generation = 0;
  std::uint8_t selected_slot = 0;
  bool other_header_valid = false;
  std::uint32_t payload_size = 0;
};

struct RecordHeader {
  std::uint64_t generation = 0;
  std::uint32_t payload_size = 0;
  std::uint32_t payload_crc = 0;
};

class RecordStore {
 public:
  inline static constexpr std::size_t kHeaderBytes = 32U;
  explicit RecordStore(StoragePort& storage) noexcept : storage_(storage) {}

  PairLoad loadPair(std::string_view base,
                    RecordKind kind,
                    std::size_t max_payload,
                    std::size_t admitted_payload = static_cast<std::size_t>(-1));
  PairPeek peekPair(std::string_view base,
                    RecordKind kind,
                    std::size_t max_payload);
  Status commitPair(std::string_view base,
                    RecordKind kind,
                    std::uint64_t generation,
                    std::string_view payload);
  Status commitPairParts(std::string_view base,
                         RecordKind kind,
                         std::uint64_t generation,
                         const std::string_view* payload_parts,
                         std::size_t part_count);
  // Only for a caller-owned unfinished create/migration target whose ID was
  // collision-checked before publication. Never use this to repair an
  // established document: a corrupt record must remain diagnosable there.
  Status resetUnrecoverablePair(std::string_view base,
                                RecordKind kind,
                                std::size_t max_payload);

  static draftor::memory::String encode(RecordKind kind,
                            std::uint64_t generation,
                            std::string_view payload);
  static Status decode(std::string_view bytes,
                       RecordKind expected_kind,
                       std::size_t max_payload,
                       Record* out);
  static draftor::memory::String encodeHeader(RecordKind kind,
                                  std::uint64_t generation,
                                  std::size_t payload_size,
                                  std::uint32_t payload_crc);
  static Status decodeHeader(std::string_view bytes,
                             RecordKind expected_kind,
                             std::size_t max_payload,
                             RecordHeader* out) noexcept;

 private:
  StoragePort& storage_;
};

}  // namespace hanime::documents
