#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "txt.hpp"

namespace draftor::exchange {
constexpr std::size_t DefaultBodyLimit = 65536, TransferChunkBytes = 512;
struct TransferQuota {
  std::size_t body = DefaultBodyLimit;
  bool checked(Limits& out) const noexcept { return Limits::checked(body, out); }
};
enum class Status { Ok, Busy, NotReady, InvalidArgument, Bounds, ReadOnly, Stale,
  Io, Full, ShortRead, ShortWrite, SyncFailed, Filesystem, TooLarge, InvalidText,
  Cancelled, Contract, Exhausted, Conflict, NotFound, Uncertain };
struct IoResult { Status status = Status::Ok; std::size_t bytes = 0; };
struct SourceIdentity {
  std::uint64_t session = 0, entry = 0, size = 0;
  bool operator==(const SourceIdentity& b) const noexcept {
    return session == b.session && entry == b.entry && size == b.size;
  }
};
class ImportSource {
 public:
  virtual ~ImportSource() = default;
  virtual SourceIdentity identity() const noexcept = 0;
  virtual std::uint64_t sourceLease() const noexcept = 0;
  virtual bool stable() const noexcept = 0;
  // Stable bytes alone do not prove complete receipt after an interrupted transfer.
  virtual bool confirmed() const noexcept { return false; }
  virtual IoResult read(std::uint64_t offset, void*, std::size_t) noexcept = 0;
};
enum class Outcome { None, Reserved, Writing, Verified, Imported, Opened, Aborted };
struct ImportIntent {
  std::uint64_t operation = 0; // durable caller-issued ID, never inferred from name
  SourceIdentity source{};
  std::array<char, 256> name{}; // display metadata only; not a storage path
};
struct ImportResult {
  Status status = Status::NotReady;
  Outcome outcome = Outcome::None;
  std::uint64_t operation = 0;
  std::array<std::uint8_t, 16> document{};
  std::size_t bodyBytes = 0;
  TextFailure text{};
  std::array<std::uint8_t, 16> collection{};
};
// CreateNew only. Every call is synchronous/non-reentrant. finish verifies exact
// staging length and durable data. publish exposes a complete verified body;
// durable same-operation query reconciles interrupted publication/receipt delivery.
// finish/publish may return Busy for cooperative progress; repeat on a later tick.
// A failed/uncertain publish must be queried, never retried with a new ID.
class DocumentImportPort {
 public:
  virtual ~DocumentImportPort() = default;
  virtual Status reserve(const ImportIntent&, ImportResult&) noexcept = 0;
  virtual Status begin(std::uint64_t operation) noexcept = 0;
  virtual IoResult append(std::uint64_t operation, std::string_view) noexcept = 0;
  virtual Status finish(std::uint64_t operation, std::size_t bodyBytes) noexcept = 0;
  virtual Status publish(std::uint64_t operation) noexcept = 0;
  virtual void abort(std::uint64_t operation) noexcept = 0;
  virtual Status query(std::uint64_t operation, ImportResult&) noexcept = 0;
};
} // namespace draftor::exchange
