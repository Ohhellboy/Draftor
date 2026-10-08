#pragma once
#include "draftor_usb_transfer/ports.hpp"
#include <FatLib/FatLib.h>

namespace draftor::usb::pico {
// One private disk image. The working TF card is never an MSC BlockDevice.
class SdFatInbox final : public InboxMediaPort, public InboxFilesystemPort {
 public:
  explicit SdFatInbox(BlockDevice& disk, TransferQuota quota = {}) noexcept
    : disk_(disk), host_(*this), fs_(*this), quota_(quota) {}
  ~SdFatInbox() { release(); }
  Status prepare(std::uint64_t) noexcept override;
  BlockDevice& device() noexcept override { return host_; }
  Status seal(std::uint64_t) noexcept override;
  bool stable(std::uint64_t s) const noexcept override { return owner_ == Owner::Sealed && session_ == s; }
  void release() noexcept override;
  Status mountReadOnly(std::uint64_t) noexcept override;
  Status next(InboxCandidate&) noexcept override;
  Status open(const InboxCandidate&) noexcept override;
  IoResult read(std::uint64_t, void*, std::size_t) noexcept override;
  void closeFile() noexcept override;
  void unmount() noexcept override;
 private:
  enum class Owner { None, Formatting, Host, Sealed };
  class HostDisk final : public BlockDevice {
   public:
    explicit HostDisk(SdFatInbox& p) : parent_(p) {}
    std::uint32_t sectorCount() const noexcept override { return parent_.disk_.sectorCount(); }
    IoResult read(std::uint64_t at, void* p, std::size_t n) noexcept override;
    IoResult write(std::uint64_t at, const void* p, std::size_t n) noexcept override;
    Status sync() noexcept override { return parent_.disk_.sync(); }
   private: SdFatInbox& parent_;
  };
  class FilesystemDisk final : public FsBlockDevice {
   public:
    explicit FilesystemDisk(SdFatInbox& p) : parent_(p) {}
    void budget(std::size_t n) noexcept { remaining_ = n; failed_ = false; }
    bool failed() const noexcept { return failed_; }
    bool isBusy() override { return false; }
    Sector_t sectorCount() override { return parent_.disk_.sectorCount(); }
    bool syncDevice() override { return parent_.disk_.sync() == Status::Ok; }
    bool readSector(Sector_t s, uint8_t* p) override { return readSectors(s,p,1); }
    bool writeSector(Sector_t s, const uint8_t* p) override { return writeSectors(s,p,1); }
    bool readSectors(Sector_t, uint8_t*, std::size_t) override;
    bool writeSectors(Sector_t, const uint8_t*, std::size_t) override;
   private: SdFatInbox& parent_; std::size_t remaining_ = 0; bool failed_ = false;
  };
  bool validChain(const FatFile&) noexcept;
  bool describe(FatFile&, InboxCandidate&) noexcept;
  BlockDevice& disk_;
  HostDisk host_;
  FilesystemDisk fs_;
  FatVolume volume_{};
  FatFile root_{}, file_{};
  std::array<std::uint8_t, SectorBytes> work_{};
  TransferQuota quota_{};
  Owner owner_ = Owner::None;
  std::uint64_t session_ = 0, entry_ = 0;
  bool mounted_ = false;
};
} // namespace draftor::usb::pico
