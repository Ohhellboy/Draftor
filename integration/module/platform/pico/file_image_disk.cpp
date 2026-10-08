#include "file_image_disk.hpp"
#include <cstdio>
namespace draftor::usb::pico {
Status FileImageDisk::beginCreate(SdFs& sd) noexcept {
  if (file_) return Status::Busy;
  if (!sd.exists("/DRAFTOR_USB") && !sd.mkdir("/DRAFTOR_USB")) return Status::Io;
  bool created=false;
  for (unsigned i=1;i<=9999;++i) {
    std::snprintf(path_.data(),path_.size(),"/DRAFTOR_USB/INBOX%04u.IMG",i);
    if (sd.exists(path_.data())) continue;
    created=file_.open(&sd,path_.data(),O_RDWR|O_CREAT|O_EXCL); break;
  }
  if (!created) return Status::Full;
  initialized_ = 0; ready_ = false; return Status::Ok;
}
Status FileImageDisk::advanceCreate(std::size_t sectors) noexcept {
  if (!file_) return Status::NotReady;
  if (ready_) return Status::Ok;
  if (!sectors || sectors>32) return Status::InvalidArgument;
  // Preallocation alone would expose stale card bytes. Zero every byte before
  // publishing this image; preparation is owner-context, with no active MSC.
  std::array<uint8_t,SectorBytes> zero{};
  for (std::size_t step=0;step<sectors && initialized_<Bytes;++step) {
    if (file_.write(zero.data(),zero.size())!=zero.size()) { close(); return Status::Full; }
    initialized_ += zero.size();
  }
  if (initialized_<Bytes) return Status::Busy;
  if (!file_.sync() || file_.fileSize()!=Bytes) { close(); return Status::SyncFailed; }
  ready_ = true; return Status::Ok;
}
IoResult FileImageDisk::read(std::uint64_t at,void* p,std::size_t n) noexcept {
  if (!file_ || !ready_) return {Status::NotReady,0};
  if ((n&&!p)||at>Bytes||n>Bytes-at) return {Status::Bounds,0};
  if (!file_.seekSet(at)) return {Status::Io,0};
  const auto got=file_.read(p,n);
  return got<0?IoResult{Status::Io,0}:IoResult{std::size_t(got)==n?Status::Ok:Status::ShortRead,std::size_t(got)};
}
IoResult FileImageDisk::write(std::uint64_t at,const void* p,std::size_t n) noexcept {
  if (!file_ || !ready_) return {Status::NotReady,0};
  if ((n&&!p)||at>Bytes||n>Bytes-at) return {Status::Bounds,0};
  if (!file_.seekSet(at)) return {Status::Io,0};
  const auto got=file_.write(p,n);
  return {got==n?Status::Ok:Status::ShortWrite,got};
}
}
