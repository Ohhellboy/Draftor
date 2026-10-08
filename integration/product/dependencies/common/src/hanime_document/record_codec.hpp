#include "../../core_memory.hpp"
#pragma once

#include "types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace hanime::documents {

struct CollectionState {
  DocumentId collection_id{};
  std::uint64_t id_counter = 0;
  std::uint64_t temp_name_counter = 0;
  std::uint64_t opened_order_counter = 0;
  std::uint64_t generation = 0;
};

struct CurrentRecord {
  bool has_current = false;
  DocumentId document_id{};
  std::uint64_t generation = 0;
};

struct PositionRecord {
  DocumentId document_id{};
  std::uint64_t body_generation = 0;
  std::uint32_t text_crc32 = 0;
  std::size_t cursor_byte = 0;
  VisualAnchor viewport{};
};

struct CreateIntent {
  bool active = false;
  DocumentMetadata metadata{};
  // Zero retains the ordinary blank-create contract. External creates bind a
  // durable caller operation and source before any body can become visible.
  std::uint64_t external_operation = 0;
  std::uint64_t source_session = 0, source_entry = 0, source_size = 0;
};

struct MigrationRecord {
  enum class State : std::uint8_t { None = 0, InProgress = 1, Complete = 2 };
  State state = State::None;
  DocumentId target_id{};
};

draftor::memory::String encodeCollection(const CollectionState& value);
Status decodeCollection(std::string_view bytes, CollectionState* out) noexcept;
draftor::memory::String encodeMetadata(const DocumentMetadata& value);
// S3: allocation failures propagate to the application error boundary.
Status decodeMetadata(std::string_view bytes, DocumentMetadata* out);
draftor::memory::String encodeSnapshot(const Snapshot& value);
Status decodeSnapshot(std::string_view bytes, Snapshot* out);
draftor::memory::String encodeSnapshotPrefix(const Snapshot& value);
draftor::memory::String encodeSnapshotPrefix(std::string_view text, std::size_t cursor_byte,
                                 VisualAnchor viewport);
draftor::memory::String encodeSnapshotPrefix(std::size_t text_size,
                                 std::uint32_t text_crc32,
                                 std::size_t cursor_byte,
                                 VisualAnchor viewport);
Status decodeSnapshotOwned(draftor::memory::String bytes, Snapshot* out) noexcept;
draftor::memory::String encodeCurrent(const CurrentRecord& value);
Status decodeCurrent(std::string_view bytes, CurrentRecord* out) noexcept;
draftor::memory::String encodePosition(const PositionRecord& value);
Status decodePosition(std::string_view bytes, PositionRecord* out) noexcept;
draftor::memory::String encodeCreateIntent(const CreateIntent& value);
Status decodeCreateIntent(std::string_view bytes, CreateIntent* out);
draftor::memory::String encodeMigration(const MigrationRecord& value);
Status decodeMigration(std::string_view bytes, MigrationRecord* out) noexcept;

}  // namespace hanime::documents
