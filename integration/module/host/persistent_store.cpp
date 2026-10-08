#include "persistent_store.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>

namespace draftor::usb::host {
namespace fs = std::filesystem;
namespace {
std::array<std::uint8_t, 16> modelDocument(std::uint64_t operation) {
  std::array<std::uint8_t, 16> id{{0x48,0x4f,0x53,0x54,0x4d,0x4f,0x44,0x4c}};
  for (unsigned i = 0; i < 8; ++i) id[i + 8] = operation >> (8 * i);
  return id;
}
bool syncDir(const fs::path& p) {
  const int fd = ::open(p.c_str(), O_RDONLY);
  if (fd < 0) return false;
  const bool ok = ::fsync(fd) == 0; ::close(fd); return ok;
}
std::string intentBytes(const ImportIntent& i) {
  // Fixed explicit wire layout; never serialize padding or process pointers.
  std::string bytes("DUSB01", 6);
  for (auto value : {i.operation, i.source.session, i.source.entry, i.source.size})
    for (unsigned b = 0; b < 8; ++b) bytes.push_back(static_cast<char>(value >> (b * 8)));
  bytes.append(i.name.data(), i.name.size()); return bytes;
}
std::string readFile(const fs::path& p) {
  std::ifstream file(p, std::ios::binary);
  if (!file) throw std::runtime_error("read failed");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
bool writeFile(const fs::path& p, std::string_view bytes) {
  const int fd = ::open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return false;
  const bool ok = ::write(fd, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()) && ::fsync(fd) == 0;
  ::close(fd); return ok;
}
}
PersistentStore::PersistentStore(fs::path root) : root_(std::move(root)) {
  fs::create_directories(root_ / "staging"); fs::create_directories(root_ / "documents");
}
PersistentStore::~PersistentStore() { closeFd(); }
void PersistentStore::closeFd() noexcept { if (fd_ >= 0) ::close(fd_); fd_ = -1; }
fs::path PersistentStore::path(std::uint64_t id, bool published) const {
  std::ostringstream name; name << std::hex << std::setw(16) << std::setfill('0') << id;
  return root_ / (published ? "documents" : "staging") / name.str();
}
Status PersistentStore::reserve(const ImportIntent& i, ImportResult& out) noexcept {
  try {
    if (fault == Fault::Reserve || fault == Fault::Unavailable) return Status::Io;
    if (!i.operation || i.name.back()) return Status::InvalidArgument;
    const auto bytes = intentBytes(i);
    const auto published = path(i.operation, true), staging = path(i.operation, false);
    if (fs::exists(published)) {
      if (readFile(published / "intent") != bytes) return Status::Conflict;
      return query(i.operation, out);
    }
    if (active_) return Status::Busy;
    if (fs::exists(staging)) {
      if (readFile(staging / "intent") != bytes) return Status::Conflict;
      // A recovered unpublished transaction is invisible; safely restart it.
      fs::remove_all(staging);
    }
    fs::create_directory(staging);
    if (!writeFile(staging / "intent", bytes) || !syncDir(staging)) return Status::Io;
    active_ = i.operation; state_ = Outcome::Reserved; written_ = 0;
    out = {Status::Ok, state_, active_, modelDocument(active_), 0, {}}; return Status::Ok;
  } catch (...) { return Status::Io; }
}
Status PersistentStore::begin(std::uint64_t id) noexcept {
  try {
    if (id != active_ || state_ != Outcome::Reserved) return Status::Stale;
    fd_ = ::open((path(id, false) / "body").c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd_ < 0) return Status::Io;
    state_ = Outcome::Writing; return Status::Ok;
  } catch (...) { return Status::Io; }
}
IoResult PersistentStore::append(std::uint64_t id, std::string_view bytes) noexcept {
  if (id != active_ || state_ != Outcome::Writing || fd_ < 0) return {Status::Stale, 0};
  if (bytes.size() > SectorBytes || bytes.size() > BodyLimit - written_) return {Status::Bounds, 0};
  if (fault == Fault::Append || fault == Fault::Unavailable) return {Status::Io, 0};
  const auto count = fault == Fault::ShortWrite && !bytes.empty() ? bytes.size() - 1 : bytes.size();
  const auto n = ::write(fd_, bytes.data(), count);
  if (n < 0) return {Status::Io, 0};
  std::array<char, SectorBytes> verify{};
  const auto got = ::pread(fd_, verify.data(), n, written_);
  if (got != n || std::memcmp(verify.data(), bytes.data(), n)) return {Status::Io, 0};
  written_ += n; return {Status::Ok, static_cast<std::size_t>(n)};
}
Status PersistentStore::finish(std::uint64_t id, std::size_t n) noexcept {
  try {
    if (id != active_ || state_ != Outcome::Writing || n != written_) return Status::Contract;
    if (fault == Fault::Finish) return Status::Io;
    struct stat st{};
    if (::fstat(fd_, &st) || st.st_size != static_cast<off_t>(n)) return Status::Io;
    if (fault == Fault::Flush || ::fsync(fd_)) return Status::SyncFailed;
    closeFd();
    if (!writeFile(path(id, false) / "verified", std::to_string(n)) || !syncDir(path(id, false))) return Status::Io;
    state_ = Outcome::Verified; return Status::Ok;
  } catch (...) { return Status::Io; }
}
Status PersistentStore::publish(std::uint64_t id) noexcept {
  try {
    if (id != active_ || state_ != Outcome::Verified) return Status::Stale;
    if (fault == Fault::Publish) return Status::Io;
    if (fs::exists(path(id, true))) return Status::Conflict;
    fs::rename(path(id, false), path(id, true));
    state_ = Outcome::Imported; active_ = 0;
    if (!syncDir(root_ / "documents") || !syncDir(root_ / "staging")) return Status::Uncertain;
    return fault == Fault::LostResult ? Status::Uncertain : Status::Ok;
  } catch (...) { return Status::Io; }
}
void PersistentStore::abort(std::uint64_t id) noexcept {
  try {
    // Never remove a published directory, including after uncertain delivery.
    if (id == active_) { closeFd(); active_ = 0; state_ = Outcome::Aborted; }
    fs::remove_all(path(id, false));
  } catch (...) {}
}
Status PersistentStore::query(std::uint64_t id, ImportResult& out) noexcept {
  try {
    if (fault == Fault::Query || fault == Fault::Unavailable) return Status::Io;
    const auto p = path(id, true);
    if (fs::exists(p)) {
      const auto n = fs::file_size(p / "body");
      if (readFile(p / "verified") != std::to_string(n) || n > BodyLimit) return Status::Io;
      if (!syncDir(root_ / "documents") || !syncDir(root_ / "staging")) return Status::Uncertain;
      out = {Status::Ok, Outcome::Imported, id, modelDocument(id), static_cast<std::size_t>(n), {}};
      return Status::Ok;
    }
    if (id == active_) { out = {Status::Ok, state_, id, modelDocument(id), written_, {}}; return Status::Ok; }
    return Status::NotFound;
  } catch (...) { return Status::Io; }
}
ImportResult PersistentStore::openDocument(std::uint64_t id, bool failOpen) noexcept {
  ImportResult r;
  r.status = query(id, r);
  if (r.status == Status::Ok) {
    if (failOpen) r.status = Status::Io;
    else { current = id; r.outcome = Outcome::Opened; }
  }
  return r;
}
std::size_t PersistentStore::count() const {
  return std::distance(fs::directory_iterator(root_ / "documents"), fs::directory_iterator{});
}
std::string PersistentStore::body(std::uint64_t id) const { return readFile(path(id, true) / "body"); }
} // namespace draftor::usb::host
