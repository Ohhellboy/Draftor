#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "src/hanime_document/types.hpp"

namespace hanime::find {

inline constexpr std::uint16_t kFindProtocolVersion = 1;
inline constexpr std::size_t kHardMaximumQueryBytes = 256U;

enum class Direction : std::uint8_t { Forward, Backward };

enum class Status : std::uint8_t {
  Closed,
  Editing,
  Searching,
  Match,
  NotFound,
  EmptyQuery,
  QueryTooLong,
  CancelledOrStale,
  AdapterFailure,
};

struct SourceStamp {
  hanime::documents::DocumentId document_id{};
  std::uint64_t document_session = 0;
  std::uint64_t mount_epoch = 0;
  std::uint32_t text_revision = 0;

  bool operator==(const SourceStamp& other) const noexcept {
    return document_id == other.document_id &&
           document_session == other.document_session &&
           mount_epoch == other.mount_epoch &&
           text_revision == other.text_revision;
  }
  bool operator!=(const SourceStamp& other) const noexcept {
    return !(*this == other);
  }
};

struct MatchRange {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::uint32_t query_version = 0;
  bool valid = false;
};

struct ViewModel {
  Status status = Status::Closed;
  Direction direction = Direction::Forward;
  std::string_view query{};
  std::size_t query_cursor = 0;
  MatchRange match{};
  std::uint32_t query_version = 0;
  std::uint32_t highlight_version = 0;
  bool case_sensitive = false;
  bool wrapped = false;
  bool open = false;
};

struct MemoryBudget {
  std::size_t session_object_bytes = 0;
  std::size_t query_storage_bytes = 0;
  std::size_t ime_staging_bytes = 0;
  std::size_t unbounded_match_list_bytes = 0;
};

class DocumentPort {
 public:
  virtual ~DocumentPort() = default;
  virtual SourceStamp sourceStamp() const noexcept = 0;
  virtual std::string_view text() const noexcept = 0;
  virtual std::size_t cursorByte() const noexcept = 0;
  virtual bool beginFindSession() noexcept = 0;
  virtual bool revealMatch(std::size_t begin, std::size_t end) noexcept = 0;
  virtual bool placeCaretAtByte(std::size_t byte_offset) noexcept = 0;
  virtual void clearMatchHighlight() noexcept = 0;
};

const char* statusName(Status status) noexcept;

}  // namespace hanime::find
