#include "../../core_memory.hpp"
#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hanime::documents {

struct ReadResult {
  Status status = Status::NotFound;
  draftor::memory::String bytes;
};

struct ListResult {
  Status status = Status::Ok;
  draftor::memory::Vector<draftor::memory::String> paths;
  bool has_more = false;
};

enum class CooperativeOpenMode : std::uint8_t { Read, Write };

struct CooperativeIoResult {
  Status status = Status::Conflict;
  std::size_t count = 0U;
  std::size_t value = 0U;
  bool flag = false;
};

// Synchronous, serialized filesystem boundary. Product integration owns the
// shared SPI/TF arbitration and must not call it from a BLE callback.
class StoragePort {
 public:
  virtual ~StoragePort() = default;
  virtual MediumSession mediumSession() const noexcept = 0;
  virtual ReadResult read(std::string_view path, std::size_t max_bytes) = 0;
  // Reads at most max_bytes from the start without rejecting a larger file.
  virtual ReadResult readPrefix(std::string_view path, std::size_t max_bytes) = 0;
  // True only when writeParts and cooperative Write both create/truncate the
  // actual target. Other adapters retain the donor's remove-before-write path.
  virtual bool writesTruncate() const noexcept { return false; }
  virtual Status write(std::string_view path, std::string_view bytes) = 0;
  virtual Status writeParts(std::string_view path, const std::string_view* parts,
                            std::size_t part_count) = 0;
  virtual Status verifyParts(std::string_view path, const std::string_view* parts,
                             std::size_t part_count) = 0;
  virtual Status remove(std::string_view path) = 0;
  // Returns paths in strict lexicographic order, exclusively after `after`.
  // This cursor contract lets the core enumerate metadata with bounded RAM.
  virtual ListResult list(std::string_view prefix, std::string_view after,
                          std::size_t max_paths) = 0;

  // Optional one-pass ID enumeration. Visitor may read metadata while the
  // directory handle is held; no body files or directory-wide ID array.
  virtual bool documentEnumerationSupported() const noexcept { return false; }
  virtual Status visitDocumentIds(void*, Status(*)(void*, const DocumentId&)) { return Status::UnsupportedVersion; }

  // Optional single-primitive interface used by the background save job.  A
  // successful call below performs at most the named filesystem/File API;
  // it never opens, probes, flushes or closes an additional handle.  The
  // default implementation keeps host/storage adapters source-compatible and
  // explicitly reports that cooperative I/O is unavailable.
  virtual bool cooperativeSupported() const noexcept { return false; }
  // Exists: flag means present; optional value 1=directory, 2=regular, 0=unknown.
  // Open must distinguish NotFound from IoError.
  virtual CooperativeIoResult cooperativeExists(std::string_view) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeMkdir(std::string_view) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeRemove(std::string_view) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeOpen(std::string_view,
                                               CooperativeOpenMode) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeIsDirectory() {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeSize() {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeRead(std::uint8_t*, std::size_t) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeWrite(const std::uint8_t*,
                                                std::size_t) {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeFlush() {
    return {Status::Conflict};
  }
  virtual CooperativeIoResult cooperativeClose() {
    return {Status::Conflict};
  }
};

class IdSource {
 public:
  virtual ~IdSource() = default;
  virtual DocumentId next(const DocumentId& collection_id,
                          std::uint64_t persisted_counter) = 0;
};

}  // namespace hanime::documents
