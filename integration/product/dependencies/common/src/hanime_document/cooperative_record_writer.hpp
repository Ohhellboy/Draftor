#include "../../core_memory.hpp"
#pragma once

#include "record_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace hanime::documents {

// Reads and authenticates one slot header without retaining a File operation
// across the public service call.  Each service() performs at most one
// cooperative storage primitive or one bounded CPU transition.
class CooperativeHeaderReader {
 public:
  void start(StoragePort& storage, draftor::memory::String path, RecordKind kind,
             std::size_t max_payload, MediumSession session);
  bool service();
  bool active() const noexcept;
  Status status() const noexcept { return status_; }
  Status readStatus() const noexcept { return read_status_; }
  bool lastStepWasStorage() const noexcept { return last_step_storage_; }
  const RecordHeader& header() const noexcept { return header_; }

 private:
  enum class State : std::uint8_t {
    Idle, Open, OpenFailedExists, IsDirectory, Size, Read, Close, Decode, Done
  };
  void finish(Status status) noexcept;

  StoragePort* storage_ = nullptr;
  draftor::memory::String path_;
  RecordKind kind_ = RecordKind::Snapshot;
  std::size_t max_payload_ = 0U;
  MediumSession session_{};
  State state_ = State::Idle;
  Status status_ = Status::NotFound;
  Status read_status_ = Status::NotFound;
  Status after_close_status_ = Status::Ok;
  RecordHeader header_{};
  std::array<std::uint8_t, RecordStore::kHeaderBytes> bytes_{};
  std::size_t wanted_ = 0U;
  std::size_t at_ = 0U;
  bool last_step_storage_ = false;
};

class CooperativeRecordWriter {
 public:
  inline static constexpr std::size_t kChunkBytes = 512U;
  inline static constexpr std::size_t kMaxPayloadParts = 3U;

  // requested_generation == 0 selects max(valid A/B)+1.  Parts remain
  // caller-owned and must stay immutable until the writer finishes.
  Status start(draftor::memory::String base, RecordKind kind,
               std::uint64_t requested_generation,
               const std::string_view* payload_parts,
               std::size_t part_count, MediumSession session,
               std::uint64_t trusted_generation = 0U);
  OperationResult service(StoragePort& storage);
  bool active() const noexcept;
  bool complete() const noexcept { return state_ == State::Done; }
  Status status() const noexcept { return status_; }
  std::uint64_t generation() const noexcept { return generation_; }
  std::uint32_t payloadCrc() const noexcept { return payload_crc_; }
  std::size_t storageSteps() const noexcept { return storage_steps_; }
  std::size_t cpuSteps() const noexcept { return cpu_steps_; }
  std::size_t writeChunks() const noexcept { return write_chunks_; }
  std::size_t maxIoRequest() const noexcept { return max_io_request_; }
  const draftor::memory::String& targetPath() const noexcept { return target_path_; }

 private:
  enum class State : std::uint8_t {
    Idle,
    PayloadCrc,
    BuildHeader,
    PeekAStart,
    PeekA,
    PeekBStart,
    PeekB,
    SelectSlot,
    DirectoryPlan,
    DirectoryExists,
    DirectoryOpen,
    DirectoryIsDirectory,
    DirectoryClose,
    DirectoryMkdir,
    TargetExists,
    TargetRemove,
    WriteOpen,
    Write,
    Flush,
    WriteClose,
    VerifyOpen,
    VerifyOpenFailedExists,
    VerifyIsDirectory,
    VerifySize,
    VerifyRead,
    VerifyEof,
    VerifyClose,
    Finish,
    FailureClose,
    Done,
  };

  static void crcUpdate(std::uint32_t* crc, const std::uint8_t* bytes,
                        std::size_t count) noexcept;
  bool sessionMatches(const StoragePort& storage) const noexcept;
  void fail(Status status, bool file_open) noexcept;
  void advancePart(std::size_t* part, std::size_t* offset) const noexcept;
  draftor::memory::String parentPath() const;
  bool selectNextDirectory();
  OperationResult working(const char* stage) const noexcept;

  State state_ = State::Idle;
  Status status_ = Status::Ok;
  Status deferred_failure_ = Status::Ok;
  draftor::memory::String base_;
  draftor::memory::String target_path_;
  draftor::memory::String parent_path_;
  draftor::memory::String current_directory_;
  draftor::memory::String header_;
  RecordKind kind_ = RecordKind::Snapshot;
  MediumSession session_{};
  std::uint64_t requested_generation_ = 0U;
  std::uint64_t trusted_generation_ = 0U;
  std::uint64_t generation_ = 0U;
  std::uint32_t running_crc_ = 0xFFFFFFFFU;
  std::uint32_t payload_crc_ = 0U;
  std::size_t payload_size_ = 0U;
  std::array<std::string_view, kMaxPayloadParts> payload_parts_{};
  std::size_t payload_part_count_ = 0U;
  std::array<std::string_view, kMaxPayloadParts + 1U> record_parts_{};
  std::size_t crc_part_ = 0U;
  std::size_t crc_offset_ = 0U;
  std::size_t io_part_ = 0U;
  std::size_t io_offset_ = 0U;
  std::size_t directory_scan_ = 1U;
  bool directory_planned_=false;
  std::size_t expected_size_ = 0U;
  CooperativeHeaderReader header_reader_{};
  Status slot_status_[2] = {Status::NotFound, Status::NotFound};
  Status slot_read_status_[2] = {Status::NotFound, Status::NotFound};
  RecordHeader slot_header_[2]{};
  std::array<std::uint8_t, kChunkBytes> verify_buffer_{};
  std::size_t storage_steps_ = 0U;
  std::size_t cpu_steps_ = 0U;
  std::size_t write_chunks_ = 0U;
  std::size_t max_io_request_ = 0U;
};

}  // namespace hanime::documents
