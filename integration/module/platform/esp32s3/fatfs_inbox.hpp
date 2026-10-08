#pragma once
#include "draftor_usb_transfer/ports.hpp"
#include "ff.h"
#include "diskio_impl.h"

namespace draftor::usb::s3 {
// Dedicated volatile storage, allocated only for an inbox session.
// Product releases it only after the InboxSession closes and all leases drain.
class PsramDisk final : public BlockDevice {
 public:
  static constexpr std::size_t DefaultBytes = 512 * 1024;
  explicit PsramDisk(std::size_t bytes = DefaultBytes) noexcept : bytes_(bytes) {}
  ~PsramDisk();
  Status allocate() noexcept;
  void release() noexcept;
  std::size_t requestedBytes() const noexcept { return bytes_; }
  bool validCapacity() const noexcept {
    return bytes_ && bytes_ % SectorBytes == 0 && bytes_ / SectorBytes <= UINT32_MAX;
  }
  std::size_t bytes() const noexcept { return memory_ ? bytes_ : 0; }
  std::uint32_t sectorCount() const noexcept override { return memory_ ? bytes_ / SectorBytes : 0; }
  IoResult read(std::uint64_t, void*, std::size_t) noexcept override;
  IoResult write(std::uint64_t, const void*, std::size_t) noexcept override;
  Status sync() noexcept override { return memory_ ? Status::Ok : Status::NotReady; }
 private:
  std::uint8_t* memory_ = nullptr;
  std::size_t bytes_;
};
class FatFsInbox final : public InboxMediaPort, public InboxFilesystemPort {
 public:
  explicit FatFsInbox(BlockDevice& disk) noexcept : disk_(disk) {}
  ~FatFsInbox();
  Status prepare(std::uint64_t) noexcept override;
  BlockDevice& device() noexcept override { return disk_; }
  Status seal(std::uint64_t) noexcept override;
  bool stable(std::uint64_t) const noexcept override;
  void release() noexcept override;
  Status mountReadOnly(std::uint64_t) noexcept override;
  Status next(InboxCandidate&) noexcept override;
  Status open(const InboxCandidate&) noexcept override;
  IoResult read(std::uint64_t, void*, std::size_t) noexcept override;
  void closeFile() noexcept override;
  void unmount() noexcept override;
 private:
  enum class Owner { None, Preparing, Host, Sealed };
  static DSTATUS diskInit(BYTE);
  static DSTATUS diskStatus(BYTE);
  static DRESULT diskRead(BYTE, BYTE*, DWORD, UINT);
  static DRESULT diskWrite(BYTE, const BYTE*, DWORD, UINT);
  static DRESULT diskIoctl(BYTE, BYTE, void*);
  static FatFsInbox* drives_[FF_VOLUMES]; // SDK C trampoline, owner context only
  BlockDevice& disk_;
  FATFS fat_{};
  FIL file_{};
  FF_DIR directory_{};
  FILINFO info_{};
  std::array<std::uint8_t, FF_MAX_SS> formatWork_{};
  std::array<char, 260> filename_{};
  char drive_[4]{};
  BYTE pdrv_ = FF_DRV_NOT_USED;
  Owner owner_ = Owner::None;
  std::uint64_t session_ = 0, entry_ = 0;
  bool mounted_ = false, fileOpen_ = false, dirOpen_ = false;
};
} // namespace draftor::usb::s3
