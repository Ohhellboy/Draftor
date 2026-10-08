#include "fatfs_inbox.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include "esp_heap_caps.h"
#include <cstring>
#include <cstdio>
#include <limits>

namespace draftor::usb::s3 {
static_assert(sizeof(TCHAR) == 1 && FF_LFN_UNICODE == 2, "UTF-8 FatFS API required");
static_assert(FF_VOLUMES <= 10, "Numeric one-digit FatFS drive names required");
PsramDisk::~PsramDisk() { release(); }
void PsramDisk::release() noexcept { heap_caps_free(memory_); memory_ = nullptr; }
Status PsramDisk::allocate() noexcept {
  if (memory_) return Status::Ok;
  if (!validCapacity()) return Status::InvalidArgument;
  memory_ = static_cast<std::uint8_t*>(heap_caps_calloc(1, bytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  return memory_ ? Status::Ok : Status::Io;
}
IoResult PsramDisk::read(std::uint64_t at, void* p, std::size_t n) noexcept {
  if (!memory_) return {Status::NotReady, 0};
  if (at > bytes_ || n > bytes_ - at) return {Status::Bounds, 0};
  if (n && !p) return {Status::InvalidArgument, 0};
  if (n) std::memcpy(p, memory_ + at, n);
  return {Status::Ok, n};
}
IoResult PsramDisk::write(std::uint64_t at, const void* p, std::size_t n) noexcept {
  if (!memory_) return {Status::NotReady, 0};
  if (at > bytes_ || n > bytes_ - at) return {Status::Bounds, 0};
  if (n && !p) return {Status::InvalidArgument, 0};
  if (n) std::memcpy(memory_ + at, p, n);
  return {Status::Ok, n};
}
FatFsInbox* FatFsInbox::drives_[FF_VOLUMES]{};
FatFsInbox::~FatFsInbox() { release(); }
DSTATUS FatFsInbox::diskStatus(BYTE p) {
  if (p >= FF_VOLUMES || !drives_[p]) return STA_NOINIT;
  const auto owner = drives_[p]->owner_;
  return owner == Owner::Preparing ? 0 : owner == Owner::Sealed ? STA_PROTECT : STA_NOINIT;
}
DSTATUS FatFsInbox::diskInit(BYTE p) { return diskStatus(p); }
DRESULT FatFsInbox::diskRead(BYTE p, BYTE* out, DWORD lba, UINT n) {
  if (diskStatus(p) & STA_NOINIT) return RES_NOTRDY;
  const auto r = drives_[p]->disk_.read(std::uint64_t(lba) * SectorBytes, out, std::size_t(n) * SectorBytes);
  return r.status == Status::Ok && r.bytes == std::size_t(n) * SectorBytes ? RES_OK : RES_ERROR;
}
DRESULT FatFsInbox::diskWrite(BYTE p, const BYTE* in, DWORD lba, UINT n) {
  const auto status = diskStatus(p);
  if (status & STA_NOINIT) return RES_NOTRDY;
  if (status & STA_PROTECT) return RES_WRPRT;
  const auto r = drives_[p]->disk_.write(std::uint64_t(lba) * SectorBytes, in, std::size_t(n) * SectorBytes);
  return r.status == Status::Ok && r.bytes == std::size_t(n) * SectorBytes ? RES_OK : RES_ERROR;
}
DRESULT FatFsInbox::diskIoctl(BYTE p, BYTE cmd, void* out) {
  if (diskStatus(p) & STA_NOINIT) return RES_NOTRDY;
  switch (cmd) {
    case CTRL_SYNC: return drives_[p]->disk_.sync() == Status::Ok ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT: if (!out) return RES_PARERR; *static_cast<DWORD*>(out) = drives_[p]->disk_.sectorCount(); return RES_OK;
    case GET_SECTOR_SIZE: if (!out) return RES_PARERR; *static_cast<WORD*>(out) = SectorBytes; return RES_OK;
    case GET_BLOCK_SIZE: if (!out) return RES_PARERR; *static_cast<DWORD*>(out) = 1; return RES_OK;
    default: return RES_PARERR; // no trim passthrough to any working storage
  }
}
Status FatFsInbox::prepare(std::uint64_t session) noexcept {
  if (owner_ != Owner::None) return Status::Busy;
  if (!disk_.sectorCount() || !session) return Status::NotReady;
  if (ff_diskio_get_drive(&pdrv_) != ESP_OK || pdrv_ >= FF_VOLUMES) return Status::Busy;
  drives_[pdrv_] = this;
  const ff_diskio_impl_t impl{diskInit, diskStatus, diskRead, diskWrite, diskIoctl};
  ff_diskio_register(pdrv_, &impl);
  VolToPart[pdrv_] = {pdrv_, 0};
  drive_[0] = static_cast<char>('0' + pdrv_); drive_[1] = ':'; drive_[2] = 0;
  owner_ = Owner::Preparing; session_ = session; entry_ = 0;
  // Format only the dedicated inbox. No filesystem is mounted during Host I/O.
  const MKFS_PARM options{FM_FAT | FM_SFD, 2, 1, 128, 512};
  if (f_mkfs(drive_, &options, formatWork_.data(), formatWork_.size()) != FR_OK || disk_.sync() != Status::Ok) {
    release(); return Status::Filesystem;
  }
  if (f_mount(nullptr, drive_, 0) != FR_OK) { release(); return Status::Filesystem; }
  owner_ = Owner::Host; return Status::Ok;
}
Status FatFsInbox::seal(std::uint64_t session) noexcept {
  if (owner_ != Owner::Host || session != session_) return Status::Stale;
  owner_ = Owner::Sealed; return Status::Ok;
}
bool FatFsInbox::stable(std::uint64_t session) const noexcept { return owner_ == Owner::Sealed && session == session_; }
Status FatFsInbox::mountReadOnly(std::uint64_t session) noexcept {
  if (!stable(session) || mounted_) return Status::NotReady;
  if (f_mount(&fat_, drive_, 1) != FR_OK) { f_mount(nullptr, drive_, 0); return Status::Filesystem; }
  mounted_ = true;
  // The dedicated volume is always formatted with a fixed FAT12/16 root.
  // Reject host-replaced FAT32 roots before traversal: linked root chains can
  // cycle, while this root has a fixed, bounded 128-slot extent.
  if ((fat_.fs_type != FS_FAT12 && fat_.fs_type != FS_FAT16) || fat_.n_rootdir != 128) {
    unmount(); return Status::Filesystem;
  }
  if (f_opendir(&directory_, drive_) != FR_OK) { unmount(); return Status::Filesystem; }
  dirOpen_ = true; return Status::Ok;
}
Status FatFsInbox::next(InboxCandidate& out) noexcept {
  if (!dirOpen_ || owner_ != Owner::Sealed) return Status::NotReady;
  // The root has at most 128 entries on this formatted inbox; cap malformed
  // media traversal independently to prevent an unbounded candidate loop.
  if (entry_ >= 4096) return Status::Filesystem;
  if (f_readdir(&directory_, &info_) != FR_OK) return Status::Filesystem;
  if (!info_.fname[0]) return Status::NotFound;
  InboxCandidate c;
  c.source = {session_, ++entry_, info_.fsize};
  const auto n = std::strlen(info_.fname);
  if (n >= c.name.size()) { c.hidden = true; }
  else std::memcpy(c.name.data(), info_.fname, n + 1);
  c.directory = info_.fattrib & AM_DIR; c.hidden = c.hidden || (info_.fattrib & (AM_HID | AM_SYS));
  out = c; return Status::Ok;
}
Status FatFsInbox::open(const InboxCandidate& c) noexcept {
  if (!mounted_ || !stable(c.source.session) || fileOpen_ || !isTxtCandidate(c)) return Status::NotReady;
  // Re-enumerate the sealed directory with the existing FatFS implementation to
  // bind entry identity, name and size; a caller cannot forge a path candidate.
  FF_DIR verify{}; FILINFO item{};
  if (f_opendir(&verify, drive_) != FR_OK) return Status::Filesystem;
  bool found = false;
  for (std::uint64_t i = 1; i <= c.source.entry && i <= 4096; ++i) {
    if (f_readdir(&verify, &item) != FR_OK || !item.fname[0]) break;
    if (i == c.source.entry) found = c.source.size == item.fsize &&
      std::strcmp(c.name.data(), item.fname) == 0 && !(item.fattrib & (AM_DIR | AM_HID | AM_SYS));
  }
  f_closedir(&verify);
  if (!found) return Status::Stale;
  std::snprintf(filename_.data(), filename_.size(), "%s/%s", drive_, c.name.data());
  if (f_open(&file_, filename_.data(), FA_READ) != FR_OK) return Status::Filesystem;
  fileOpen_ = true; return Status::Ok;
}
IoResult FatFsInbox::read(std::uint64_t at, void* out, std::size_t n) noexcept {
  if (!fileOpen_ || owner_ != Owner::Sealed) return {Status::NotReady, 0};
  if (n > SectorBytes || at > f_size(&file_) || n > f_size(&file_) - at) return {Status::Bounds, 0};
  if (f_lseek(&file_, at) != FR_OK) return {Status::Io, 0};
  UINT got = 0;
  const auto s = f_read(&file_, out, n, &got);
  return {s == FR_OK ? Status::Ok : Status::Io, got};
}
void FatFsInbox::closeFile() noexcept { if (fileOpen_) f_close(&file_); fileOpen_ = false; }
void FatFsInbox::unmount() noexcept {
  closeFile(); if (dirOpen_) f_closedir(&directory_); dirOpen_ = false;
  if (mounted_) f_mount(nullptr, drive_, 0);
  mounted_ = false;
}
void FatFsInbox::release() noexcept {
  unmount();
  if (pdrv_ != FF_DRV_NOT_USED) { ff_diskio_unregister(pdrv_); drives_[pdrv_] = nullptr; }
  pdrv_ = FF_DRV_NOT_USED; owner_ = Owner::None; session_ = 0;
}
} // namespace draftor::usb::s3
