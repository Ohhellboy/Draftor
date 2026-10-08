#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "draftor_exchange/document_transfer.hpp"

namespace draftor::usb {
constexpr std::size_t BodyLimit = 65536, WireLimit = 131075, SectorBytes = 512;
using exchange::Status;
using exchange::IoResult;
using exchange::TransferQuota;
enum class Access { ReadOnly, ReadWrite };
enum class EndReason { None, Ejected, Forced, Disconnected };

// Exact, synchronous I/O; no buffer may be retained. Backing storage is dedicated
// to transfer and must never be a Draftor working TF block device.
class BlockDevice {
 public:
  virtual ~BlockDevice() = default;
  virtual std::uint32_t sectorCount() const noexcept = 0;
  virtual IoResult read(std::uint64_t offset, void*, std::size_t) noexcept = 0;
  virtual IoResult write(std::uint64_t offset, const void*, std::size_t) noexcept = 0;
  virtual Status sync() noexcept = 0;
};
struct MediaState {
  bool accepting = false, drained = true, wrote = false;
  EndReason end = EndReason::None;
  Status error = Status::Ok;
  std::uint32_t generation = 0;
};
class MediaPort {
 public:
  virtual ~MediaPort() = default;
  virtual Status publish(BlockDevice&, Access, std::uint32_t& generation) noexcept = 0;
  virtual void withdraw(EndReason) noexcept = 0;
  virtual MediaState state() const noexcept = 0;
  // Only clears the backing pointer after admission is closed and requests drain.
  virtual Status detach() noexcept = 0;
};
using exchange::SourceIdentity;
using exchange::ImportSource;
struct InboxCandidate {
  SourceIdentity source{};
  std::array<char, 256> name{};
  bool directory = false, hidden = false;
};
// Filesystem port enumerates one root entry at a time. It must cap traversal,
// validate names and never follow symlinks or recurse into metadata directories.
class InboxFilesystemPort {
 public:
  virtual ~InboxFilesystemPort() = default;
  virtual Status mountReadOnly(std::uint64_t session) noexcept = 0;
  virtual Status next(InboxCandidate&) noexcept = 0; // NotFound = end
  virtual Status open(const InboxCandidate&) noexcept = 0;
  virtual IoResult read(std::uint64_t, void*, std::size_t) noexcept = 0;
  virtual void closeFile() noexcept = 0;
  virtual void unmount() noexcept = 0;
};
class InboxMediaPort {
 public:
  virtual ~InboxMediaPort() = default;
  virtual Status prepare(std::uint64_t session) noexcept = 0;
  virtual BlockDevice& device() noexcept = 0;
  virtual Status seal(std::uint64_t session) noexcept = 0;
  virtual bool stable(std::uint64_t session) const noexcept = 0;
  virtual void release() noexcept = 0;
};
using exchange::Outcome;
using exchange::ImportIntent;
using exchange::ImportResult;
using exchange::DocumentImportPort;
} // namespace draftor::usb
