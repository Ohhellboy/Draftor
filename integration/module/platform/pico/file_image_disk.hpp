#pragma once
#include "draftor_usb_transfer/ports.hpp"
#include <SdFat.h>
namespace draftor::usb::pico {
class FileImageDisk final : public BlockDevice {
 public:
  static constexpr std::size_t Bytes = 8*1024*1024;
  Status beginCreate(SdFs&) noexcept;
  Status advanceCreate(std::size_t sectors = 8) noexcept;
  void close() noexcept { if (file_) file_.close(); ready_ = false; initialized_ = 0; }
  const char* path() const noexcept { return path_.data(); }
  std::uint32_t sectorCount() const noexcept override { return file_ && ready_ ? Bytes/SectorBytes : 0; }
  IoResult read(std::uint64_t,void*,std::size_t) noexcept override;
  IoResult write(std::uint64_t,const void*,std::size_t) noexcept override;
  Status sync() noexcept override { return file_ && file_.sync() ? Status::Ok : Status::SyncFailed; }
 private: FsFile file_{}; std::array<char,48> path_{}; std::size_t initialized_ = 0; bool ready_ = false;
};
}
