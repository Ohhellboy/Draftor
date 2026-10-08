#include "sdfat_inbox.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include <cstring>

namespace draftor::usb::pico {
IoResult SdFatInbox::HostDisk::read(std::uint64_t at, void* p, std::size_t n) noexcept {
  return parent_.owner_ == Owner::Host ? parent_.disk_.read(at,p,n) : IoResult{Status::NotReady,0};
}
IoResult SdFatInbox::HostDisk::write(std::uint64_t at, const void* p, std::size_t n) noexcept {
  return parent_.owner_ == Owner::Host ? parent_.disk_.write(at,p,n) : IoResult{Status::ReadOnly,0};
}
bool SdFatInbox::FilesystemDisk::readSectors(Sector_t s, uint8_t* p, std::size_t n) {
  if ((parent_.owner_ != Owner::Formatting && parent_.owner_ != Owner::Sealed) || n > remaining_ ||
      s > sectorCount() || n > sectorCount()-s) { failed_ = true; return false; }
  remaining_ -= n;
  const auto result = parent_.disk_.read(std::uint64_t(s)*SectorBytes,p,n*SectorBytes);
  const bool okay = result.status == Status::Ok && result.bytes == n*SectorBytes;
  if (!okay) failed_ = true;
  return okay;
}
bool SdFatInbox::FilesystemDisk::writeSectors(Sector_t s, const uint8_t* p, std::size_t n) {
  if (parent_.owner_ != Owner::Formatting || n > remaining_ || s > sectorCount() || n > sectorCount()-s) {
    failed_ = true; return false;
  }
  remaining_ -= n;
  const auto result = parent_.disk_.write(std::uint64_t(s)*SectorBytes,p,n*SectorBytes);
  const bool okay = result.status == Status::Ok && result.bytes == n*SectorBytes;
  if (!okay) failed_ = true;
  return okay;
}
Status SdFatInbox::prepare(std::uint64_t session) noexcept {
  if (owner_ != Owner::None) return Status::Busy;
  exchange::Limits limits;
  if (!quota_.checked(limits)) return Status::InvalidArgument;
  if (!session || disk_.sectorCount() != 16384) return Status::InvalidArgument;
  owner_ = Owner::Formatting; session_ = session; entry_ = 0;
  fs_.budget(16384);
  FatFormatter formatter;
  if (!formatter.format(&fs_,work_.data()) || disk_.sync()!=Status::Ok) {
    release(); return Status::Filesystem;
  }
  owner_ = Owner::Host; return Status::Ok;
}
Status SdFatInbox::seal(std::uint64_t session) noexcept {
  if (owner_ != Owner::Host || session_ != session) return Status::Stale;
  if (disk_.sync()!=Status::Ok) return Status::SyncFailed;
  owner_ = Owner::Sealed; return Status::Ok;
}
Status SdFatInbox::mountReadOnly(std::uint64_t session) noexcept {
  if (!stable(session) || mounted_) return Status::NotReady;
  fs_.budget(64);
  if (!fs_.readSector(0,work_.data()) || work_[510]!=0x55 || work_[511]!=0xaa) return Status::Filesystem;
  const auto le32=[](const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); };
  const auto first=le32(work_.data()+454), count=le32(work_.data()+458);
  if (!first || !count || first>=disk_.sectorCount() || count>disk_.sectorCount()-first ||
      !fs_.readSector(first,work_.data()) || work_[510]!=0x55 || work_[511]!=0xaa) return Status::Filesystem;
  // init() plus explicit root handles avoids changing SdFat global working-volume state.
  if (!volume_.init(&fs_,1)) return Status::Filesystem;
  // A fixed FAT16 root cannot contain a cyclic directory chain. A host-reformatted
  // FAT32/exFAT volume is outside this Inbox contract and is rejected before scan.
  if (volume_.fatType()!=16 || !volume_.rootDirEntryCount() || volume_.rootDirEntryCount()>512 ||
      !volume_.sectorsPerCluster() || volume_.clusterCount()>8192 ||
      std::uint64_t(volume_.dataStartSector())+std::uint64_t(volume_.clusterCount())*volume_.sectorsPerCluster()>disk_.sectorCount() ||
      !root_.openRoot(&volume_)) { volume_.end(); return Status::Filesystem; }
  mounted_ = true; entry_ = 0; return Status::Ok;
}
bool SdFatInbox::describe(FatFile& file, InboxCandidate& out) noexcept {
  InboxCandidate c;
  c.source = {session_,std::uint64_t(file.dirIndex())+1,file.fileSize()};
  const auto length = file.getName(c.name.data(),c.name.size());
  if (!length || length >= c.name.size()-1) c.hidden = true;
  c.directory = !file.isFile();
  c.hidden = c.hidden || file.isHidden() || (file.attrib() & FS_ATTRIB_SYSTEM);
  out = c; return !fs_.failed();
}
Status SdFatInbox::next(InboxCandidate& out) noexcept {
  if (!mounted_ || owner_!=Owner::Sealed) return Status::NotReady;
  if (entry_++ > volume_.rootDirEntryCount()) return Status::Filesystem;
  fs_.budget(64);
  FatFile candidate;
  if (!candidate.openNext(&root_,O_RDONLY))
    return fs_.failed() || root_.getError() ? Status::Filesystem : Status::NotFound;
  const bool okay = describe(candidate,out); candidate.close();
  return okay ? Status::Ok : Status::Filesystem;
}
bool SdFatInbox::validChain(const FatFile& file) noexcept {
  const auto bytesPerCluster = std::size_t(volume_.sectorsPerCluster())*SectorBytes;
  const auto count = (file.fileSize()+bytesPerCluster-1)/bytesPerCluster;
  if (!count) return file.firstCluster()==0;
  if (count>volume_.clusterCount()) return false;
  auto cluster = file.firstCluster();
  // Exact length plus an EOC on the final cluster rejects truncation and cycles,
  // including loops contained entirely inside the filesystem's sector cache.
  for (std::size_t i=0; i<count; ++i) {
    if (cluster<2 || cluster>volume_.clusterCount()+1) return false;
    std::uint32_t next = 0;
    const auto status = volume_.dbgFat(cluster,&next);
    if (i+1==count) return status==0;
    if (status!=1 || next==cluster) return false;
    cluster = next;
  }
  return false;
}
Status SdFatInbox::open(const InboxCandidate& candidate) noexcept {
  if (!mounted_ || !stable(candidate.source.session) || file_.isOpen() || !isTxtCandidate(candidate)) return Status::NotReady;
  exchange::Limits limits;
  if (!quota_.checked(limits)) return Status::InvalidArgument;
  if (candidate.source.size>limits.wire || candidate.source.entry==0 || candidate.source.entry>volume_.rootDirEntryCount()) return Status::Bounds;
  fs_.budget(512);
  FatFile checkRoot;
  if (!checkRoot.openRoot(&volume_) || !file_.open(&checkRoot,static_cast<uint16_t>(candidate.source.entry-1),O_RDONLY)) {
    checkRoot.close(); file_.close(); return Status::Stale;
  }
  checkRoot.close(); InboxCandidate actual;
  if (!describe(file_,actual) || !(actual.source==candidate.source) || std::strcmp(actual.name.data(),candidate.name.data()) ||
      !isTxtCandidate(actual) || !validChain(file_)) {
    file_.close(); return Status::Filesystem;
  }
  return Status::Ok;
}
IoResult SdFatInbox::read(std::uint64_t at, void* p, std::size_t n) noexcept {
  if (!mounted_ || owner_!=Owner::Sealed || !file_.isOpen()) return {Status::NotReady,0};
  if ((n&&!p) || n>SectorBytes || at>file_.fileSize() || n>file_.fileSize()-at) return {Status::Bounds,0};
  fs_.budget(512);
  if (!file_.seekSet(static_cast<uint32_t>(at))) return {Status::Io,0};
  const auto got = file_.read(p,n);
  return got<0 ? IoResult{Status::Io,0} : IoResult{std::size_t(got)==n?Status::Ok:Status::ShortRead,std::size_t(got)};
}
void SdFatInbox::closeFile() noexcept { if (file_.isOpen()) file_.close(); }
void SdFatInbox::unmount() noexcept {
  closeFile(); if (root_.isOpen()) root_.close();
  if (mounted_) volume_.end();
  mounted_=false;
}
void SdFatInbox::release() noexcept { unmount(); owner_=Owner::None; session_=entry_=0; }
} // namespace draftor::usb::pico
