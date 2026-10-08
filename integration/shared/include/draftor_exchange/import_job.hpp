#pragma once
#include "document_transfer.hpp"

namespace draftor::exchange {
enum class ImportPhase { Idle, Reading, Finishing, Verifying, Publishing, Complete, Failed, Uncertain };
class ImportJob final {
 public:
  explicit ImportJob(DocumentImportPort& port, TransferQuota quota = {}) noexcept
    : port_(port), quota_(quota), text_(quota.body) {}
  ~ImportJob();
  ImportJob(const ImportJob&) = delete;
  ImportJob& operator=(const ImportJob&) = delete;
  Status begin(ImportSource&, const ImportIntent&) noexcept;
  ImportPhase tick() noexcept;
  void cancel() noexcept;
  ImportPhase phase() const noexcept { return phase_; }
  ImportResult result() const noexcept { return result_; }
 private:
  void fail(Status) noexcept;
  bool append(const exchange::Step&) noexcept;
  DocumentImportPort& port_;
  ImportSource* source_ = nullptr;
  ImportIntent intent_{};
  ImportResult result_{};
  TransferQuota quota_;
  ImportWire text_;
  std::array<char, TransferChunkBytes> input_{}, output_{};
  std::size_t have_ = 0, used_ = 0, offset_ = 0;
  std::uint64_t sourceLease_ = 0;
  ImportPhase phase_ = ImportPhase::Idle;
};
} // namespace draftor::exchange
