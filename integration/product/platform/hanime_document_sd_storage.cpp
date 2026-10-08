#include "core_memory.hpp"
#include "hanime_document_sd_storage.hpp"
#include "src/hanime_document/save_diagnostic.hpp"

#include "s3_vfs.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <cerrno>
#include <limits>
#include <string>
#include <utility>

namespace hanime::documents {
namespace {

constexpr std::array<std::string_view, 0> kLegacyReadPaths{};

constexpr std::size_t kMaxTraversalDepth = 16U;

bool hasUnsafeSegment(const std::string_view path,
                      const bool allow_trailing_slash) noexcept {
  if (path.empty() || path.front() != '/' ||
      path.find('\0') != std::string_view::npos ||
      path.find('\\') != std::string_view::npos) {
    return true;
  }

  std::size_t segment_start = 1U;
  while (segment_start <= path.size()) {
    const std::size_t slash = path.find('/', segment_start);
    const std::size_t segment_end = slash == std::string_view::npos
        ? path.size() : slash;
    const std::string_view segment =
        path.substr(segment_start, segment_end - segment_start);
    const bool trailing = segment.empty() && segment_start == path.size();
    if (segment.empty() && !(allow_trailing_slash && trailing)) return true;
    if (segment == "." || segment == "..") return true;
    if (slash == std::string_view::npos) break;
    segment_start = slash + 1U;
  }
  return false;
}

draftor::memory::String parentPath(const std::string_view path) {
  const std::size_t slash = path.rfind('/');
  if (slash == std::string_view::npos || slash == 0U) return "/";
  return draftor::memory::String(path.substr(0U, slash));
}

draftor::memory::String childPath(const std::string_view parent, const char* child_name) {
  if (child_name == nullptr || child_name[0] == '\0') return {};
  const std::string_view child(child_name);
  if (child.front() == '/') return draftor::memory::String(child);
  draftor::memory::String joined(parent);
  if (joined.empty() || joined.back() != '/') joined.push_back('/');
  joined.append(child.data(), child.size());
  return joined;
}

}  // namespace



ArduinoSdStoragePort::ArduinoSdStoragePort(s3fs::Fs& sd) noexcept : sd_(&sd) {}

void ArduinoSdStoragePort::advanceEpoch() noexcept {
  ++mount_epoch_;
  if (mount_epoch_ == 0U) ++mount_epoch_;
}

void ArduinoSdStoragePort::noteMounted() noexcept {
  if (cooperative_file_) cooperative_file_.close();
  cooperative_file_ = s3fs::File{};
  cooperative_file_epoch_ = 0U;
  advanceEpoch();
  available_ = true;
}

void ArduinoSdStoragePort::noteUnmounted() noexcept {
  // Retire a handle while it still belongs to the old mounted volume.  A
  // cooperative job will subsequently observe the epoch change and cannot
  // issue an operation through this retired handle against a replacement SD.
  if (cooperative_file_) cooperative_file_.close();
  cooperative_file_ = s3fs::File{};
  cooperative_file_epoch_ = 0U;
  if (available_) advanceEpoch();
  available_ = false;
}

MediumSession ArduinoSdStoragePort::mediumSession() const noexcept {
  return {available_, mount_epoch_};
}

Status ArduinoSdStoragePort::sessionStatus(
    const std::uint64_t operation_epoch) const noexcept {
  if (mount_epoch_ != operation_epoch) return Status::SessionChanged;
  return available_ ? Status::Ok : Status::MediaUnavailable;
}

bool ArduinoSdStoragePort::isLegacyReadPath(
    const std::string_view path) noexcept {
  return std::find(kLegacyReadPaths.begin(), kLegacyReadPaths.end(), path) !=
         kLegacyReadPaths.end();
}

bool ArduinoSdStoragePort::isNamespacePath(
    const std::string_view path,
    const bool allow_trailing_slash) noexcept {
  if (hasUnsafeSegment(path, allow_trailing_slash)) return false;
  if (path == kRoot) return true;
  return path.size() > kRoot.size() &&
         path.compare(0U, kRoot.size(), kRoot) == 0 &&
         path[kRoot.size()] == '/';
}

bool ArduinoSdStoragePort::isReadablePath(
    const std::string_view path) noexcept {
  return isNamespacePath(path, false) || isLegacyReadPath(path);
}

ReadResult ArduinoSdStoragePort::readImpl(const std::string_view path,
                                           const std::size_t max_bytes,
                                           const bool prefix_only) {
  if (!isReadablePath(path) || path == kRoot || path.back() == '/') {
    return {Status::InvalidName, {}};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable, {}};

  const draftor::memory::String owned_path(path);
  s3fs::File file = save_diag::measured(save_diag::Op::ReadOpen, 0, [&] { return sd_->openFile(owned_path.c_str(), s3fs::Read); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  if (!file) {
    const bool exists = save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_path.c_str()); }, [](const auto&) { return -1; });
    const Status current = sessionStatus(before.mount_epoch);
    if (current != Status::Ok) return {current, {}};
    return {exists ? Status::IoError : Status::NotFound, {}};
  }
  if (save_diag::measured(save_diag::Op::IsDirectory, 0, [&] { return file.isDirectory(); }, [](const auto&) { return -1; })) {
    save_diag::measuredVoid(save_diag::Op::ReadClose, [&] { file.close(); });
    const Status current = sessionStatus(before.mount_epoch);
    return {current == Status::Ok ? Status::IoError : current, {}};
  }

  const std::size_t file_size = static_cast<std::size_t>(save_diag::measured(save_diag::Op::Size, 0, [&] { return file.size(); }, [](const auto&) { return -1; }));
  if (!prefix_only && file_size > max_bytes) {
    save_diag::measuredVoid(save_diag::Op::ReadClose, [&] { file.close(); });
    const Status current = sessionStatus(before.mount_epoch);
    return {current == Status::Ok ? Status::TooLarge : current, {}};
  }
  const std::size_t wanted = prefix_only
      ? std::min(file_size, max_bytes) : file_size;
  draftor::memory::String bytes(wanted, '\0');
  std::size_t at = 0U;
  Status status = Status::Ok;
  while (at < wanted) {
    const std::size_t count =
        std::min(std::size_t(4096), wanted - at);
    const int got = save_diag::measured(save_diag::Op::Read, count, [&] { return file.read(
        reinterpret_cast<std::uint8_t*>(&bytes[at]), count); }, [&](int n) { return n > 0 && static_cast<std::size_t>(n) <= count ? 1 : 0; });
    if (got <= 0 || static_cast<std::size_t>(got) > count) {
      status = Status::IoError;
      break;
    }
    at += static_cast<std::size_t>(got);
  }
  save_diag::measuredVoid(save_diag::Op::ReadClose, [&] { file.close(); });

  const Status current = sessionStatus(before.mount_epoch);
  if (current != Status::Ok) return {current, {}};
  if (status != Status::Ok) return {status, {}};
  return {Status::Ok, std::move(bytes)};
}

ReadResult ArduinoSdStoragePort::read(const std::string_view path,
                                      const std::size_t max_bytes) {
  return save_diag::measuredResult(save_diag::Op::FullRead, [&]() -> ReadResult {
  return readImpl(path, max_bytes, false);

  });
}

ReadResult ArduinoSdStoragePort::readPrefix(const std::string_view path,
                                            const std::size_t max_bytes) {
  return save_diag::measuredResult(save_diag::Op::PrefixRead, [&]() -> ReadResult {
  return readImpl(path, max_bytes, true);

  });
}

Status ArduinoSdStoragePort::mkdirsImpl(
    const std::string_view path,
    const std::uint64_t operation_epoch) {
  return save_diag::measuredStatus(save_diag::Op::DirectoryPlan, [&]() -> Status {
  draftor::memory::String target(path);
  while (target.size() > 1U && target.back() == '/') target.pop_back();

  std::size_t slash = target.find('/', 1U);
  while (true) {
    const std::size_t end = slash == draftor::memory::String::npos
        ? target.size() : slash;
    const draftor::memory::String directory = target.substr(0U, end);
    // stat already tells us whether the parent is a directory. Do not open
    // and close every existing parent again for each small metadata write.
    const auto kind = sd_->kind(directory.c_str());
    if (kind == s3fs::PathKind::Missing) {
      if (!sd_->mkdir(directory.c_str())) {
        const Status current = sessionStatus(operation_epoch);
        return current == Status::Ok ? Status::IoError : current;
      }
    } else if (kind != s3fs::PathKind::Directory) {
      return Status::IoError;
    }

    const Status current = sessionStatus(operation_epoch);
    if (current != Status::Ok) return current;
    if (slash == draftor::memory::String::npos) break;
    slash = target.find('/', slash + 1U);
  }
  return Status::Ok;

  });
}

Status ArduinoSdStoragePort::mkdirs(const std::string_view path) {
  if (!isNamespacePath(path, true)) return Status::InvalidName;
  const MediumSession before = mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  return mkdirsImpl(path, before.mount_epoch);
}

Status ArduinoSdStoragePort::write(const std::string_view path,
                                   const std::string_view bytes) {
  return writeParts(path, &bytes, 1U);
}

Status ArduinoSdStoragePort::writeParts(
    const std::string_view path,
    const std::string_view* parts,
    const std::size_t part_count) {
  return save_diag::measuredStatus(save_diag::Op::WriteParts, [&]() -> Status {
  if (!isNamespacePath(path, false) || path == kRoot || path.back() == '/') {
    return Status::InvalidName;
  }
  if (parts == nullptr && part_count != 0U) return Status::Conflict;

  std::size_t total_size = 0U;
  for (std::size_t i = 0U; i < part_count; ++i) {
    if (parts[i].size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) -
            total_size) {
      return Status::TooLarge;
    }
    total_size += parts[i].size();
  }
  (void)total_size;

  const MediumSession before = mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const draftor::memory::String owned_path(path);
  Status status = Status::Ok;

  // ESP32 s3fs::Write is "w" (create/truncate); never substitute FILE_APPEND.
  // Only the inactive record slot is replaced; RecordStore verifies before acknowledgement.
  s3fs::File file = save_diag::measured(save_diag::Op::WriteOpen, 0, [&] { return sd_->openFile(owned_path.c_str(), s3fs::Write); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  // O_CREAT|O_TRUNC handles an existing inactive slot and a missing leaf.
  // ENOENT here means a missing parent: create parents only in that case.
  // No cache: every write still opens the actual path and checks all I/O results.
  if (!file && file.openError() == ENOENT) {
    status = mkdirsImpl(parentPath(path), before.mount_epoch);
    if (status != Status::Ok) return status;
    file = sd_->openFile(owned_path.c_str(), s3fs::Write);
  }
  if (!file) {
    const Status current = sessionStatus(before.mount_epoch);
    return current == Status::Ok ? Status::IoError : current;
  }

  status = Status::Ok;
  for (std::size_t i = 0U; i < part_count && status == Status::Ok; ++i) {
    for (std::size_t offset = 0; offset < parts[i].size();) {
      const std::size_t count = std::min(kVerifyBufferBytes, parts[i].size()-offset);
      const std::size_t written = file.write(reinterpret_cast<const std::uint8_t*>(parts[i].data()+offset), count);
      if (written != count) { status=Status::IoError; break; }
      offset += written;
      const Status current = sessionStatus(before.mount_epoch);
      if (current != Status::Ok) { status=current; break; }
    }
  }
  if (!file.flush()) status = Status::IoError;
  if (!file.close()) status = Status::IoError;

  const Status current = sessionStatus(before.mount_epoch);
  return current == Status::Ok ? status : current;

  });
}

Status ArduinoSdStoragePort::verifyParts(
    const std::string_view path,
    const std::string_view* parts,
    const std::size_t part_count) {
  return save_diag::measuredStatus(save_diag::Op::VerifyParts, [&]() -> Status {
  if (!isNamespacePath(path, false) || path == kRoot || path.back() == '/') {
    return Status::InvalidName;
  }
  if (parts == nullptr && part_count != 0U) return Status::Conflict;

  const MediumSession before = mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const draftor::memory::String owned_path(path);
  s3fs::File file = save_diag::measured(save_diag::Op::VerifyOpen, 0, [&] { return sd_->openFile(owned_path.c_str(), s3fs::Read); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  if (!file) {
    const bool exists = save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_path.c_str()); }, [](const auto&) { return -1; });
    const Status current = sessionStatus(before.mount_epoch);
    if (current != Status::Ok) return current;
    return exists ? Status::IoError : Status::NotFound;
  }
  if (save_diag::measured(save_diag::Op::IsDirectory, 0, [&] { return file.isDirectory(); }, [](const auto&) { return -1; })) {
    save_diag::measuredVoid(save_diag::Op::VerifyClose, [&] { file.close(); });
    const Status current = sessionStatus(before.mount_epoch);
    return current == Status::Ok ? Status::IoError : current;
  }

  std::array<std::uint8_t, kVerifyBufferBytes> buffer{};
  Status status = Status::Ok;
  for (std::size_t part = 0U;
       part < part_count && status == Status::Ok; ++part) {
    std::size_t at = 0U;
    while (at < parts[part].size()) {
      const std::size_t wanted =
          std::min(buffer.size(), parts[part].size() - at);
      std::size_t received = 0U;
      while (received < wanted) {
        const int got = save_diag::measured(save_diag::Op::VerifyRead, wanted - received, [&] { return file.read(buffer.data() + received,
                                  wanted - received); }, [&](int n) { return n > 0 && static_cast<std::size_t>(n) <= wanted - received ? 1 : 0; });
        if (got <= 0 ||
            static_cast<std::size_t>(got) > wanted - received) {
          status = Status::IoError;
          break;
        }
        received += static_cast<std::size_t>(got);
      }
      if (status != Status::Ok) break;
      if (save_diag::measured(save_diag::Op::Compare, wanted, [&] { return std::memcmp(buffer.data(), parts[part].data() + at, wanted); }, [](int n) { return n == 0 ? 1 : 0; }) != 0) {
        status = Status::IoError;
        break;
      }
      at += wanted;
      const Status current = sessionStatus(before.mount_epoch);
      if (current != Status::Ok) {
        status = current;
        break;
      }
    }
  }
  if (status == Status::Ok) {
    std::uint8_t extra = 0U;
    const int got = save_diag::measured(save_diag::Op::Eof, 1, [&] { return file.read(&extra, 1U); }, [](int n) { return n == 0 ? 1 : 0; });
    if (got != 0) status = Status::IoError;
  }
  save_diag::measuredVoid(save_diag::Op::VerifyClose, [&] { file.close(); });

  const Status current = sessionStatus(before.mount_epoch);
  return current == Status::Ok ? status : current;

  });
}

Status ArduinoSdStoragePort::remove(const std::string_view path) {
  if (!isNamespacePath(path, false) || path == kRoot || path.back() == '/') {
    return Status::InvalidName;
  }
  const MediumSession before = mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const draftor::memory::String owned_path(path);
  if (!save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_path.c_str()); }, [](const auto&) { return -1; })) {
    const Status current = sessionStatus(before.mount_epoch);
    return current == Status::Ok ? Status::NotFound : current;
  }
  const bool removed = save_diag::measured(save_diag::Op::Remove, 0, [&] { return sd_->remove(owned_path.c_str()); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  const Status current = sessionStatus(before.mount_epoch);
  if (current != Status::Ok) return current;
  return removed ? Status::Ok : Status::IoError;
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeExists(
    const std::string_view path) {
  const bool namespace_directory = path == "/HANIME_S3_TEST" || path == kRoot;
  if (!namespace_directory && !isNamespacePath(path, true)) {
    return {Status::InvalidName};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable};
  const draftor::memory::String owned(path);
  const auto kind = save_diag::measured(
      save_diag::Op::Exists, 0,
      [&] { return sd_->kind(owned.c_str()); },
      [](const auto&) { return -1; });
  const Status current = sessionStatus(before.mount_epoch);
  if(current!=Status::Ok)return {current};
  if(kind==s3fs::PathKind::Error)return {Status::IoError};
  return {Status::Ok,0U,kind==s3fs::PathKind::Directory?1U:kind==s3fs::PathKind::File?2U:0U,kind!=s3fs::PathKind::Missing};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeMkdir(
    const std::string_view path) {
  const bool namespace_directory = path == "/HANIME_S3_TEST" || path == kRoot;
  if (!namespace_directory && !isNamespacePath(path, true)) {
    return {Status::InvalidName};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable};
  const draftor::memory::String owned(path);
  const bool made = save_diag::measured(
      save_diag::Op::Mkdir, 0,
      [&] { return sd_->mkdir(owned.c_str()); },
      [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  const Status current = sessionStatus(before.mount_epoch);
  return {current == Status::Ok && !made ? Status::IoError : current};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeRemove(
    const std::string_view path) {
  if (!isNamespacePath(path, false) || path == kRoot || path.back() == '/') {
    return {Status::InvalidName};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable};
  const draftor::memory::String owned(path);
  const bool removed = save_diag::measured(
      save_diag::Op::Remove, 0,
      [&] { return sd_->remove(owned.c_str()); },
      [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  const Status current = sessionStatus(before.mount_epoch);
  return {current == Status::Ok && !removed ? Status::IoError : current};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeOpen(
    const std::string_view path, const CooperativeOpenMode mode) {
  const bool namespace_directory = mode == CooperativeOpenMode::Read &&
      (path == "/HANIME_S3_TEST" || path == kRoot);
  if ((!namespace_directory && !isNamespacePath(path, false)) ||
      path.back() == '/' || cooperative_file_) {
    return {Status::Conflict};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable};
  const draftor::memory::String owned(path);
  const auto diagnostic_op = mode == CooperativeOpenMode::Write
      ? save_diag::Op::WriteOpen : save_diag::Op::ReadOpen;
  cooperative_file_ = save_diag::measured(
      diagnostic_op, 0,
      [&] { return sd_->openFile(owned.c_str(),
                             mode == CooperativeOpenMode::Write
                                 ? s3fs::Write : s3fs::Read); },
      [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  const Status current = sessionStatus(before.mount_epoch);
  if (current != Status::Ok || !cooperative_file_) {
    const bool missing=cooperative_file_.openError()==ENOENT;
    cooperative_file_ = s3fs::File{};
    cooperative_file_epoch_ = 0U;
    return {current == Status::Ok ? (missing?Status::NotFound:Status::IoError) : current};
  }
  cooperative_file_epoch_ = before.mount_epoch;
  cooperative_mode_ = mode;
  return {Status::Ok};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeIsDirectory() {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_ || current != Status::Ok) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  const bool directory = save_diag::measured(
      save_diag::Op::IsDirectory, 0,
      [&] { return cooperative_file_.isDirectory(); },
      [](const auto&) { return -1; });
  return {sessionStatus(cooperative_file_epoch_), 0U, 0U, directory};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeSize() {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_ || current != Status::Ok) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  const std::size_t size = static_cast<std::size_t>(save_diag::measured(
      save_diag::Op::Size, 0, [&] { return cooperative_file_.size(); },
      [](const auto&) { return -1; }));
  return {sessionStatus(cooperative_file_epoch_), 0U, size, false};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeRead(
    std::uint8_t* out, const std::size_t count) {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_ || cooperative_mode_ != CooperativeOpenMode::Read ||
      current != Status::Ok || (out == nullptr && count != 0U)) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  const int got = save_diag::measured(
      save_diag::Op::Read, count,
      [&] { return cooperative_file_.read(out, count); },
      [&](const int n) {
        return n >= 0 && static_cast<std::size_t>(n) <= count ? 1 : 0;
      });
  const Status after = sessionStatus(cooperative_file_epoch_);
  if (after != Status::Ok) return {after};
  if (got < 0 || static_cast<std::size_t>(got) > count) {
    return {Status::IoError};
  }
  return {Status::Ok, static_cast<std::size_t>(got)};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeWrite(
    const std::uint8_t* bytes, const std::size_t count) {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_ || cooperative_mode_ != CooperativeOpenMode::Write ||
      current != Status::Ok || (bytes == nullptr && count != 0U)) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  const std::size_t written = save_diag::measured(
      save_diag::Op::Write, count,
      [&] { return cooperative_file_.write(bytes, count); },
      [&](const std::size_t n) { return n == count ? 1 : 0; });
  const Status after = sessionStatus(cooperative_file_epoch_);
  if (after != Status::Ok) return {after};
  return {written == count ? Status::Ok : Status::IoError, written};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeFlush() {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_ || cooperative_mode_ != CooperativeOpenMode::Write ||
      current != Status::Ok) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  if (!cooperative_file_.flush()) return {Status::IoError};
  return {sessionStatus(cooperative_file_epoch_)};
}

CooperativeIoResult ArduinoSdStoragePort::cooperativeClose() {
  const Status current = sessionStatus(cooperative_file_epoch_);
  if (!cooperative_file_) {
    return {current == Status::Ok ? Status::Conflict : current};
  }
  const auto diagnostic_op = cooperative_mode_ == CooperativeOpenMode::Write
      ? save_diag::Op::WriteClose : save_diag::Op::ReadClose;
  (void)diagnostic_op;
  const bool closed = cooperative_file_.close();
  cooperative_file_ = s3fs::File{};
  cooperative_file_epoch_ = 0U;
  return {current == Status::Ok && !closed ? Status::IoError : current};
}

SdPathStat ArduinoSdStoragePort::stat(const std::string_view path) {
  if (!isNamespacePath(path, true)) return {Status::InvalidName, 0U, false};
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable, 0U, false};
  draftor::memory::String owned_path(path);
  while (owned_path.size() > kRoot.size() && owned_path.back() == '/') {
    owned_path.pop_back();
  }
  s3fs::File file = save_diag::measured(save_diag::Op::Open, 0, [&] { return sd_->open(owned_path.c_str(), s3fs::Read); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  if (!file) {
    const bool exists = save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_path.c_str()); }, [](const auto&) { return -1; });
    const Status current = sessionStatus(before.mount_epoch);
    if (current != Status::Ok) return {current, 0U, false};
    return {exists ? Status::IoError : Status::NotFound, 0U, false};
  }
  const bool is_directory = save_diag::measured(save_diag::Op::IsDirectory, 0, [&] { return file.isDirectory(); }, [](const auto&) { return -1; });
  const std::size_t size = is_directory
      ? 0U : static_cast<std::size_t>(save_diag::measured(save_diag::Op::Size, 0, [&] { return file.size(); }, [](const auto&) { return -1; }));
  save_diag::measuredVoid(save_diag::Op::Close, [&] { file.close(); });
  const Status current = sessionStatus(before.mount_epoch);
  return current == Status::Ok
      ? SdPathStat{Status::Ok, size, is_directory}
      : SdPathStat{current, 0U, false};
}

Status ArduinoSdStoragePort::rename(const std::string_view from,
                                    const std::string_view to) {
  if (!isNamespacePath(from, false) || !isNamespacePath(to, false) ||
      from == kRoot || to == kRoot || from.back() == '/' || to.back() == '/') {
    return Status::InvalidName;
  }
  const MediumSession before = mediumSession();
  if (!before.available) return Status::MediaUnavailable;
  const draftor::memory::String owned_from(from);
  const draftor::memory::String owned_to(to);
  if (!save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_from.c_str()); }, [](const auto&) { return -1; })) {
    const Status current = sessionStatus(before.mount_epoch);
    return current == Status::Ok ? Status::NotFound : current;
  }
  if (owned_from == owned_to) return Status::Ok;
  if (save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(owned_to.c_str()); }, [](const auto&) { return -1; })) {
    const Status current = sessionStatus(before.mount_epoch);
    return current == Status::Ok ? Status::Conflict : current;
  }
  Status status = mkdirsImpl(parentPath(to), before.mount_epoch);
  if (status != Status::Ok) return status;
  const bool renamed = sd_->rename(owned_from.c_str(), owned_to.c_str());
  const Status current = sessionStatus(before.mount_epoch);
  if (current != Status::Ok) return current;
  return renamed ? Status::Ok : Status::IoError;
}

Status ArduinoSdStoragePort::collectDirectory(
    const draftor::memory::String& directory,
    const std::string_view prefix,
    const std::string_view after,
    const std::size_t keep_paths,
    const std::size_t depth,
    const std::uint64_t operation_epoch,
    draftor::memory::Vector<draftor::memory::String>* paths) {
  if (paths == nullptr || depth > kMaxTraversalDepth) return Status::IoError;
  s3fs::File folder = save_diag::measured(save_diag::Op::Open, 0, [&] { return sd_->openDirectory(directory.c_str()); }, [](const auto& r) { return static_cast<bool>(r) ? 1 : 0; });
  if (!folder || !save_diag::measured(save_diag::Op::IsDirectory, 0, [&] { return folder.isDirectory(); }, [](const auto&) { return -1; })) {
    if (folder) save_diag::measuredVoid(save_diag::Op::Close, [&] { folder.close(); });
    const Status current = sessionStatus(operation_epoch);
    return current == Status::Ok ? Status::IoError : current;
  }

  Status status = Status::Ok;
  while (status == Status::Ok) {
    s3fs::DirectoryEntry entry;
    if (!folder.nextEntry(entry)) { if (folder.error()) status = Status::IoError; break; }
    const char* raw_name = entry.name.c_str();
    const draftor::memory::String path = childPath(directory, raw_name);
    if (path.empty() || !isNamespacePath(path, false)) {
      status = Status::IoError;
      break;
    }

    if (entry.directory) {
      const draftor::memory::String childPrefix=path+"/";
      if(prefix.substr(0,childPrefix.size())!=childPrefix && childPrefix.compare(0,prefix.size(),prefix)!=0)continue;
      status = collectDirectory(path, prefix, after, keep_paths, depth + 1U,
                                operation_epoch, paths);
    } else if (entry.regular) {
      if (path.compare(0U, prefix.size(), prefix) == 0 && path > after) {
        auto insertion = std::lower_bound(paths->begin(), paths->end(), path);
        if (insertion == paths->end() || *insertion != path) {
          if (paths->size() < keep_paths) {
            paths->insert(insertion, path);
          } else if (keep_paths != 0U && path < paths->back()) {
            paths->pop_back();
            insertion = std::lower_bound(paths->begin(), paths->end(), path);
            paths->insert(insertion, path);
          }
        }
      }
    }

    const Status current = sessionStatus(operation_epoch);
    if (current != Status::Ok) status = current;
  }
  save_diag::measuredVoid(save_diag::Op::Close, [&] { folder.close(); });
  return status;
}

Status ArduinoSdStoragePort::visitDocumentIds(void* context, Status(*visitor)(void*, const DocumentId&)) {
 const auto before=mediumSession();if(!before.available)return Status::MediaUnavailable;
 auto folder=sd_->openDirectory("/HANIME_S3_TEST/v01/docs");
 if(!folder)return folder.openError()==ENOENT?Status::Ok:Status::IoError;
 Status status=Status::Ok;s3fs::DirectoryEntry entry;
 while(folder.nextEntry(entry)){
  if(!entry.directory)continue;
  DocumentId id{};if(!DocumentId::fromHex(entry.name,&id))continue;
  status=visitor(context,id);if(status!=Status::Ok&&status!=Status::OkDegraded)break;
  status=sessionStatus(before.mount_epoch);if(status!=Status::Ok)break;
 }
 const bool failed=folder.error();const bool closed=folder.close();if(failed||!closed)status=Status::IoError;
 const auto current=sessionStatus(before.mount_epoch);return current==Status::Ok?status:current;
}

ListResult ArduinoSdStoragePort::list(const std::string_view prefix,
                                      const std::string_view after,
                                      const std::size_t max_paths) {
  if (!isNamespacePath(prefix, true) ||
      (!after.empty() && !isNamespacePath(after, false))) {
    return {Status::InvalidName, {}, false};
  }
  const MediumSession before = mediumSession();
  if (!before.available) return {Status::MediaUnavailable, {}, false};

  if (!save_diag::measured(save_diag::Op::Exists, 0, [&] { return sd_->exists(draftor::memory::String(kRoot).c_str()); }, [](const auto&) { return -1; })) {
    const Status current = sessionStatus(before.mount_epoch);
    return {current == Status::Ok ? Status::Ok : current, {}, false};
  }

  const std::size_t limit = std::min(max_paths, kMaxDeviceListPaths);
  const std::size_t keep_paths = limit + 1U;
  draftor::memory::Vector<draftor::memory::String> paths;
  paths.reserve(keep_paths);
  const Status status = collectDirectory(draftor::memory::String(kRoot), prefix, after,
                                         keep_paths, 0U,
                                         before.mount_epoch, &paths);
  if (status != Status::Ok) return {status, {}, false};
  const Status current = sessionStatus(before.mount_epoch);
  if (current != Status::Ok) return {current, {}, false};

  const bool has_more = paths.size() > limit;
  if (has_more) paths.pop_back();
  return {Status::Ok, std::move(paths), has_more};
}

}  // namespace hanime::documents
