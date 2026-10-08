#pragma once
#include "ports.hpp"
#include "draftor_exchange/export_session.hpp"

namespace draftor::usb {
// One immutable file. FAT metadata is synthesized; no disk/body copy is made.
class ExportVolume final : public BlockDevice {
 public:
  static constexpr std::uint32_t FatSectors = 17, RootStart = 35,
    DataStart = 39, Clusters = 4096, Sectors = DataStart + Clusters;
  explicit ExportVolume(TransferQuota quota = {}) noexcept : quota_(quota) {}
  Status bind(const exchange::Lease&) noexcept;
  void clear() noexcept;
  std::uint32_t sectorCount() const noexcept override { return Sectors; }
  IoResult read(std::uint64_t, void*, std::size_t) noexcept override;
  IoResult write(std::uint64_t, const void*, std::size_t) noexcept override {
    return {Status::ReadOnly, 0};
  }
  Status sync() noexcept override { return Status::Ok; }
 private:
  void sector(std::uint32_t, std::uint8_t*) const noexcept;
  exchange::Lease lease_{};
  TransferQuota quota_;
  bool bound_ = false;
};
enum class ExportPhase { Idle, Preparing, Published, Closing, Closed };
class UsbExport final {
 public:
  UsbExport(exchange::ExportSession& s, MediaPort& m, TransferQuota quota = {}) noexcept : session_(s), media_(m), volume_(quota) {}
  ~UsbExport();
  UsbExport(const UsbExport&) = delete;
  UsbExport& operator=(const UsbExport&) = delete;
  Status begin() noexcept;
  ExportPhase tick() noexcept;
  void close(EndReason = EndReason::Forced) noexcept;
  ExportPhase phase() const noexcept { return phase_; }
  Status error() const noexcept { return error_; }
 private:
  exchange::ExportSession& session_;
  MediaPort& media_;
  ExportVolume volume_;
  exchange::Lease lease_{};
  ExportPhase phase_ = ExportPhase::Idle;
  Status error_ = Status::Ok;
  std::uint32_t generation_ = 0;
  bool leased_ = false, published_ = false;
};
} // namespace draftor::usb
