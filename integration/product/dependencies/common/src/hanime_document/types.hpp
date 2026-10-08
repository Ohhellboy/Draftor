#include "../../core_memory.hpp"
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hanime::documents {

constexpr std::size_t kMaxDocumentBytes = 64U * 1024U;
constexpr std::size_t kMaxNameBytes = 96U;
constexpr std::size_t kMaxDocuments = 512U;
constexpr std::size_t kMaxListPage = 16U;

enum class Status : std::uint8_t {
  Ok,
  OkDegraded,
  Partial,
  NotFound,
  MediaUnavailable,
  SessionChanged,
  IoError,
  Corrupt,
  UnsupportedVersion,
  TooLarge,
  InvalidUtf8,
  InvalidCursor,
  InvalidName,
  Conflict,
  Busy,
  CompositionActive,
  NoCurrentDocument,
  NoManualVersion,
  ConfirmationRequired,
  StaleToken,
  CapacityReached,
  InstallFailed,
  FirstPage,
  LastPage,
  NameExists,
  ResourceExhausted,
};

const char* statusMessage(Status status) noexcept;

struct DocumentId {
  std::array<std::uint8_t, 16> bytes{};

  bool empty() const noexcept;
  draftor::memory::String hex() const;
  draftor::memory::String shortHex() const;
  static bool fromHex(std::string_view value, DocumentId* out) noexcept;

  friend bool operator==(const DocumentId& a, const DocumentId& b) noexcept {
    return a.bytes == b.bytes;
  }
  friend bool operator!=(const DocumentId& a, const DocumentId& b) noexcept {
    return !(a == b);
  }
  friend bool operator<(const DocumentId& a, const DocumentId& b) noexcept {
    return a.bytes < b.bytes;
  }
};

struct MediumSession {
  bool available = false;
  std::uint64_t mount_epoch = 0;
};

struct BoundSession {
  DocumentId collection_id{};
  std::uint64_t mount_epoch = 0;

  bool valid() const noexcept {
    return !collection_id.empty() && mount_epoch != 0U;
  }
};

enum class Affinity : std::uint8_t { Before = 0, After = 1 };

struct VisualAnchor {
  std::size_t byte_offset = 0;
  Affinity affinity = Affinity::Before;
  std::uint32_t layout_revision = 0;
};

struct Snapshot {
  draftor::memory::String text;
  std::size_t cursor_byte = 0;
  VisualAnchor viewport{};
  std::uint64_t generation = 0;
  std::uint32_t text_crc32 = 0;
};

struct EditorSnapshot : Snapshot {
  std::uint64_t revision = 0;
};

enum class DocumentArea : std::uint8_t { Normal = 0, Trash = 1 };

struct DocumentMetadata {
  DocumentId id{};
  draftor::memory::String name;
  DocumentArea area = DocumentArea::Normal;
  std::uint64_t opened_order = 0;
  std::uint64_t metadata_generation = 0;
};

struct ListPage {
  draftor::memory::Vector<DocumentMetadata> items;
  std::size_t offset = 0;
  bool has_more = false;
  bool degraded = false;
};

enum class SaveLane : std::uint8_t { Auto = 1, Manual = 2, Protection = 3 };
enum class RestoreSource : std::uint8_t {
  None,
  Auto,
  Manual,
  Protection,
  LegacyShared,
  LegacyV1,
};

struct LoadResult {
  Status status = Status::NotFound;
  Snapshot snapshot{};
  RestoreSource source = RestoreSource::None;
  bool used_older_slot = false;
  // Highest authenticated AUTO/MANUAL record-header generation observed by
  // loadLatest(). It can exceed snapshot.generation when payload recovery
  // falls back to an older valid slot.
  std::uint64_t latest_body_generation = 0U;
};

struct OperationResult {
  Status status = Status::Ok;
  const char* stage = "none";
  bool body_committed = false;
  bool current_record_committed = false;

  explicit operator bool() const noexcept {
    return status == Status::Ok || status == Status::OkDegraded;
  }
};

bool isValidUtf8(std::string_view text) noexcept;
bool isUtf8Boundary(std::string_view text, std::size_t offset) noexcept;
std::uint32_t crc32(std::string_view bytes) noexcept;
std::uint32_t crc32Parts(const std::string_view* parts,
                         std::size_t part_count) noexcept;

}  // namespace hanime::documents
