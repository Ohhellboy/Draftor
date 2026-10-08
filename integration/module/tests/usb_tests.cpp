#include "draftor_usb_transfer/export.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include "draftor_usb_transfer/import.hpp"
#include "media_bridge.hpp"
#include "persistent_store.hpp"
#include "model_support.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

using namespace draftor::usb;
namespace fs = std::filesystem;
static fs::path fixtures;
static void run(ImportJob& job) {
  for (unsigned i = 0; i < 2000; ++i) {
    const auto p = job.tick();
    if (p == ImportPhase::Complete || p == ImportPhase::Failed || p == ImportPhase::Uncertain) return;
  }
  CHECK(false);
}
struct Source : ImportSource {
  std::string bytes;
  bool valid = true, shortRead = false, changeOnRead = false;
  explicit Source(std::string s) : bytes(std::move(s)) {}
  SourceIdentity identity() const noexcept override { return {1, 1, bytes.size()}; }
  std::uint64_t sourceLease() const noexcept override { return 1; }
  bool stable() const noexcept override { return valid; }
  bool confirmed() const noexcept override { return true; }
  IoResult read(std::uint64_t at, void* p, std::size_t n) noexcept override {
    CHECK(n <= 512 && at <= bytes.size() && n <= bytes.size() - at);
    if (shortRead && n) --n;
    std::memcpy(p, bytes.data() + at, n);
    if (changeOnRead) valid = false;
    return {Status::Ok, n};
  }
};
static ImportIntent intent(ImportSource& s, std::uint64_t id) {
  ImportIntent i; i.operation = id; i.source = s.identity();
  std::memcpy(i.name.data(), "same-name.txt", 14); return i;
}
struct Disk : BlockDevice {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(256 * 1024);
  Status writeFailure = Status::Ok, syncFailure = Status::Ok;
  bool shortWrite = false;
  std::uint32_t sectorCount() const noexcept override { return bytes.size() / 512; }
  IoResult read(std::uint64_t at, void* p, std::size_t n) noexcept override {
    if (at > bytes.size() || n > bytes.size() - at) return {Status::Bounds, 0};
    if (n) std::memcpy(p, bytes.data() + at, n);
    return {Status::Ok, n};
  }
  IoResult write(std::uint64_t at, const void* p, std::size_t n) noexcept override {
    if (writeFailure != Status::Ok) return {writeFailure, 0};
    if (at > bytes.size() || n > bytes.size() - at) return {Status::Full, 0};
    if (shortWrite && n) --n;
    if (n) std::memcpy(bytes.data() + at, p, n);
    return {Status::Ok, n};
  }
  Status sync() noexcept override { return syncFailure; }
};
// Filesystem ownership model, intentionally NOT a FAT implementation. The S3
// adapter uses real SDK FatFS; these offsets let Host test actual block bytes.
struct Inbox : InboxMediaPort, InboxFilesystemPort {
  struct File { std::string name, bytes; bool dir = false; };
  std::vector<File> files;
  Disk disk;
  std::uint64_t session = 0;
  std::size_t scan = 0, selected = 0, mounts = 0, reads = 0;
  bool sealed = false, mounted = false, opened = false, changed = false;
  Status prepare(std::uint64_t s) noexcept override {
    session = s; scan = 0; sealed = mounted = opened = changed = false;
    std::fill(disk.bytes.begin(), disk.bytes.end(), 0); return Status::Ok;
  }
  BlockDevice& device() noexcept override { return disk; }
  Status seal(std::uint64_t s) noexcept override { CHECK(session == s && !mounted); sealed = true; return Status::Ok; }
  bool stable(std::uint64_t s) const noexcept override { return sealed && !changed && s == session; }
  void release() noexcept override { CHECK(!opened && !mounted); sealed = false; session = 0; }
  Status mountReadOnly(std::uint64_t s) noexcept override {
    CHECK(stable(s)); mounted = true; ++mounts; return Status::Ok;
  }
  std::uint64_t offset(std::size_t i) const {
    std::uint64_t at = 0; for (std::size_t j = 0; j < i; ++j) at += files[j].bytes.size(); return at;
  }
  Status next(InboxCandidate& c) noexcept override {
    CHECK(mounted && sealed);
    if (scan == files.size()) return Status::NotFound;
    c = {}; c.source = {session, scan + 1, files[scan].bytes.size()}; c.directory = files[scan].dir;
    std::snprintf(c.name.data(), c.name.size(), "%s", files[scan].name.c_str()); ++scan; return Status::Ok;
  }
  Status open(const InboxCandidate& c) noexcept override {
    CHECK(mounted && sealed && !opened);
    if (!c.source.entry || c.source.entry > files.size()) return Status::Stale;
    selected = c.source.entry - 1;
    if (c.name.data() != files[selected].name || c.source.size != files[selected].bytes.size()) return Status::Stale;
    opened = true; return Status::Ok;
  }
  IoResult read(std::uint64_t at, void* p, std::size_t n) noexcept override {
    CHECK(opened && mounted && sealed); ++reads; return disk.read(offset(selected) + at, p, n);
  }
  void closeFile() noexcept override { opened = false; }
  void unmount() noexcept override { CHECK(!opened); mounted = false; }
  void hostCopy(MediaBridge& b) {
    for (std::size_t i = 0; i < files.size(); ++i) {
      for (std::size_t at = 0; at < files[i].bytes.size(); at += 512) {
        const auto n = std::min<std::size_t>(512, files[i].bytes.size() - at);
        CHECK(b.write(b.generation(), offset(i) + at, files[i].bytes.data() + at, n).status == Status::Ok);
      }
    }
  }
};
static void publish(UsbExport& x) {
  CHECK(x.begin() == Status::Ok);
  for (unsigned i = 0; i < 1000 && x.phase() == ExportPhase::Preparing; ++i) x.tick();
  CHECK(x.phase() == ExportPhase::Published);
}
static void export_volume() {
  for (const auto& text : {std::string{}, std::string("a\n") + u8"简繁體𨿍🙂é", std::string(65536, 'x')}) {
    ModelOwner owner(text); TrackingAllocator alloc; dx::ExportSession snapshot(owner, alloc, {BodyLimit});
    MediaBridge bridge; UsbExport transfer(snapshot, bridge); publish(transfer);
    CHECK(alloc.live == (text.empty() ? 0u : 1u));
    std::vector<char> image(std::size_t(ExportVolume::Sectors) * 512);
    std::vector<unsigned> order(ExportVolume::Sectors);
    for (unsigned i = 0; i < order.size(); ++i) order[i] = i;
    std::mt19937 random(71); std::shuffle(order.begin(), order.end(), random);
    for (const auto lba : order) {
      CHECK(bridge.read(bridge.generation(), lba * 512u, image.data() + lba * 512u, 512).status == Status::Ok);
    }
    CHECK(static_cast<unsigned char>(image[510]) == 0x55 && static_cast<unsigned char>(image[511]) == 0xaa);
    CHECK(std::memcmp(image.data() + 512, image.data() + 18 * 512, 17 * 512) == 0);
    CHECK(std::string(image.data() + 35 * 512 + 32, 11) == "DOCUMENTTXT");
    CHECK(image[35 * 512 + 32 + 11] == 0x21);
    CHECK(std::string(image.data() + 39 * 512, text.size()) == text);
    for (unsigned i = 0; i < 40; ++i) {
      std::array<char, 721> out{}; const auto at = random() % (image.size() - out.size());
      CHECK(bridge.read(bridge.generation(), at, out.data(), out.size()).bytes == out.size());
      CHECK(std::memcmp(out.data(), image.data() + at, out.size()) == 0);
    }
    char byte = 'X';
    CHECK(bridge.write(bridge.generation(), 0, &byte, 1).status == Status::ReadOnly);
    CHECK(bridge.read(bridge.generation(), image.size(), &byte, 1).status == Status::Bounds);
    CHECK(bridge.read(bridge.generation() + 1, 0, &byte, 1).status == Status::Stale);
    if (text.size() == 65536) { std::ofstream out(fixtures / "export-fat16.img", std::ios::binary); out.write(image.data(), image.size()); }
    bridge.withdraw(EndReason::Ejected); transfer.tick();
    CHECK(transfer.phase() == ExportPhase::Closed && !alloc.live && !owner.frozen);
    CHECK(bridge.read(bridge.generation(), 0, &byte, 1).status == Status::NotReady);
    const auto old = bridge.generation(); publish(transfer);
    CHECK(bridge.read(old, 0, &byte, 1).status == Status::Stale);
    transfer.close(); transfer.tick(); CHECK(!alloc.live);
  }
  for (unsigned failure = 0; failure < 4; ++failure) {
    ModelOwner owner("test"); TrackingAllocator alloc;
    if (failure == 0) alloc.fail_after = 0;
    if (failure == 1) owner.read_fails = true;
    if (failure == 2) owner.mutate_in_borrow = true;
    dx::ExportSession snapshot(owner, alloc, {BodyLimit}); MediaBridge bridge; UsbExport x(snapshot, bridge);
    CHECK(x.begin() == Status::Ok);
    if (failure == 3) x.close();
    for (unsigned i = 0; i < 10 && x.phase() != ExportPhase::Closed; ++i) x.tick();
    CHECK(x.phase() == ExportPhase::Closed && !alloc.live && !owner.frozen);
  }
}
struct SlowDisk : Disk {
  std::mutex mutex; std::condition_variable cv; bool entered = false, proceed = false;
  IoResult read(std::uint64_t at, void* p, std::size_t n) noexcept override {
    std::unique_lock<std::mutex> lock(mutex); entered = true; cv.notify_all();
    CHECK(cv.wait_for(lock, std::chrono::seconds(5), [&] { return proceed; }));
    return Disk::read(at, p, n);
  }
};
static void request_lifetime() {
  SlowDisk disk; MediaBridge bridge; std::uint32_t gen = 0;
  CHECK(bridge.publish(disk, Access::ReadOnly, gen) == Status::Ok);
  char out{};
  std::thread worker([&] { CHECK(bridge.read(gen, 0, &out, 1).status == Status::Ok); });
  { std::unique_lock<std::mutex> l(disk.mutex); CHECK(disk.cv.wait_for(l, std::chrono::seconds(5), [&] { return disk.entered; })); }
  CHECK(bridge.read(gen, 0, &out, 1).status == Status::Busy);
  bridge.withdraw(EndReason::Forced);
  CHECK(!bridge.state().accepting && !bridge.state().drained);
  CHECK(bridge.detach() == Status::Busy);
  CHECK(bridge.read(gen, 0, &out, 1).status == Status::NotReady);
  { std::lock_guard<std::mutex> l(disk.mutex); disk.proceed = true; } disk.cv.notify_all(); worker.join();
  CHECK(bridge.state().drained && bridge.detach() == Status::Ok);
  Disk next; std::uint32_t newer{}; CHECK(bridge.publish(next, Access::ReadWrite, newer) == Status::Ok);
  CHECK(newer != gen && bridge.write(gen, 0, &out, 1).status == Status::Stale);
  bridge.withdraw(EndReason::Forced); CHECK(bridge.detach() == Status::Ok);
  struct InterruptedDisk : Disk {
    MediaBridge& bridge; mutable unsigned calls = 0;
    explicit InterruptedDisk(MediaBridge& b) : bridge(b) {}
    std::uint32_t sectorCount() const noexcept override {
      if (++calls == 2) bridge.withdraw(EndReason::Disconnected);
      return Disk::sectorCount();
    }
  } interrupted(bridge);
  const auto retained = newer;
  CHECK(bridge.publish(interrupted, Access::ReadWrite, newer) == Status::NotReady);
  CHECK(newer == retained && !bridge.state().accepting && bridge.detach() == Status::Ok);
}
static void seal(InboxSession& s, MediaBridge& b, EndReason end = EndReason::Ejected) {
  b.withdraw(end);
  for (unsigned i = 0; i < 4; ++i) s.tick();
}
static void inbox_ownership() {
  const std::vector<std::vector<Inbox::File>> sets = {{}, {{"a.txt", "a"}},
    {{"a.txt", "a"}, {"B.TXT", u8"𨿍"}, {"c.Txt", "c"}},
    {{".DS_Store", "meta"}, {"._a.txt", "meta"}, {".Trashes", "", true}, {".Spotlight-V100", "", true},
     {".fseventsd", "", true}, {"folder.txt", "", true}, {"photo.jpg", "xx"}, {"good.txt", "ok"}},
    {{".DS_Store", "meta"}, {"._file.txt", "meta"}}, {{"image.png", "foo"}}};
  const std::size_t expected[] = {0, 1, 3, 1, 0, 0};
  for (std::size_t i = 0; i < sets.size(); ++i) {
    Inbox backing; backing.files = sets[i]; MediaBridge bridge; InboxSession s(backing, backing, bridge);
    CHECK(s.begin() == Status::Ok && s.tick() == InboxPhase::HostMounted);
    InboxCandidate c; CHECK(s.next(c) == Status::NotReady);
    backing.hostCopy(bridge); s.tick(); CHECK(backing.mounts == 0 && backing.reads == 0);
    const auto generation = bridge.generation(); seal(s, bridge, i % 2 ? EndReason::Forced : EndReason::Ejected);
    CHECK(s.phase() == InboxPhase::Inspecting && backing.mounts == 1);
    CHECK(bridge.write(generation, 0, "x", 1).status == Status::NotReady);
    std::size_t count = 0;
    while (true) {
      const auto r = s.next(c); if (r == Status::NotFound) break; if (r == Status::Busy) continue;
      CHECK(r == Status::Ok); ++count;
      ImportSource* source = nullptr; CHECK(s.acquire(c, source) == Status::Ok); const auto lease = s.sourceLease();
      CHECK(s.close() == Status::Busy); char bytes[32]{};
      CHECK(source->read(0, bytes, c.source.size).bytes == c.source.size);
      CHECK(std::string(bytes, c.source.size) == backing.files[c.source.entry - 1].bytes);
      CHECK(s.acquire(c, source) == Status::Busy && source == &s);
      CHECK(s.release(lease) == Status::Ok && s.release(lease) == Status::Stale);
      CHECK(s.acquire(c, source) == Status::Ok && s.release(lease) == Status::Stale);
      CHECK(s.release(s.sourceLease()) == Status::Ok);
    }
    CHECK(count == expected[i]); CHECK(s.close() == Status::Ok);
    CHECK(s.begin() == Status::Ok); s.tick(); seal(s, bridge);
    ImportSource* source = nullptr;
    if (count) CHECK(s.acquire(c, source) == Status::Stale);
    CHECK(s.close() == Status::Ok);
  }
  for (Status failure : {Status::Io, Status::Full, Status::ShortWrite, Status::SyncFailed}) {
    Inbox backing; MediaBridge bridge; InboxSession s(backing, backing, bridge);
    CHECK(s.begin() == Status::Ok); s.tick();
    if (failure == Status::SyncFailed) backing.disk.syncFailure = failure;
    else if (failure == Status::ShortWrite) backing.disk.shortWrite = true;
    else backing.disk.writeFailure = failure;
    const auto r = bridge.write(bridge.generation(), 0, "xy", 2);
    CHECK(r.status == (failure == Status::SyncFailed ? Status::Ok : failure));
    seal(s, bridge); CHECK(s.phase() == InboxPhase::Closed && s.result().status == failure && !backing.mounts);
  }
  Inbox backing; backing.files = {{"big.txt", std::string(WireLimit + 1, 'a')}};
  MediaBridge bridge; InboxSession s(backing, backing, bridge); s.begin(); s.tick(); seal(s, bridge);
  InboxCandidate c; CHECK(s.next(c) == Status::Ok); ImportSource* source = nullptr;
  CHECK(s.acquire(c, source) == Status::TooLarge && !source); s.close();

  // Whole USB block write -> seal -> stable source -> streaming transaction.
  std::string wire = "\xef\xbb\xbf"; for (unsigned i = 0; i < BodyLimit; ++i) wire += "\r\n";
  Inbox full; full.files = {{".DS_Store", "meta"}, {"NORMAL.Txt", wire}};
  MediaBridge transport; InboxSession session(full, full, transport);
  CHECK(session.begin() == Status::Ok); session.tick(); full.hostCopy(transport);
  CHECK(!full.reads && !full.mounts); seal(session, transport);
  CHECK(session.next(c) == Status::Busy && session.next(c) == Status::Ok);
  CHECK(session.acquire(c, source) == Status::Ok);
  host::PersistentStore store(fixtures / "inbox-import"); ImportJob job(store);
  const auto operation = intent(*source, 1); CHECK(job.begin(*source, operation) == Status::Ok);
  CHECK(transport.write(transport.generation(), 4, "bad", 3).status == Status::NotReady);
  run(job); CHECK(job.phase() == ImportPhase::Complete && store.body(1) == std::string(BodyLimit, '\n'));
  CHECK(job.begin(*source, operation) == Status::Ok && job.phase() == ImportPhase::Complete && store.count() == 1);
  CHECK(session.release(session.sourceLease()) == Status::Ok);
  CHECK(session.acquire(c, source) == Status::Ok);
  CHECK(job.begin(*source, intent(*source, 2)) == Status::Ok);
  CHECK(session.release(session.sourceLease()) == Status::Ok);
  CHECK(session.acquire(c, source) == Status::Ok);
  job.tick(); CHECK(job.phase() == ImportPhase::Failed && job.result().status == Status::Stale && store.count() == 1);
  CHECK(session.release(session.sourceLease()) == Status::Ok);
  CHECK(session.acquire(c, source) == Status::Ok);
  CHECK(job.begin(*source, intent(*source, 3)) == Status::Ok);
  full.changed = true; job.tick(); CHECK(job.phase() == ImportPhase::Failed && store.count() == 1);
  CHECK(session.release(session.sourceLease()) == Status::Ok); CHECK(session.close() == Status::Ok);
}
static void import_text() {
  const std::string bom = "\xef\xbb\xbf";
  std::vector<std::pair<std::string, std::string>> cases = {{"", ""}, {"ASCII", "ASCII"},
    {u8"中文 简体 繁體 𨿍 🙂 é", u8"中文 简体 繁體 𨿍 🙂 é"},
    {bom + "a\r\nb\rc\n\t  \n\nend  ", "a\nb\nc\n\t  \n\nend  "},
    {bom + bom + "x" + bom, bom + "x" + bom},
    {std::string(511, 'x') + "\r\ny\r", std::string(511, 'x') + "\ny\n"},
    {std::string(511, 'x') + u8"𨿍", std::string(511, 'x') + u8"𨿍"},
    {std::string(65536, 'x'), std::string(65536, 'x')}};
  std::string large = bom; for (unsigned i = 0; i < 65536; ++i) large += "\r\n";
  CHECK(large.size() == WireLimit); cases.push_back({large, std::string(65536, '\n')});
  host::PersistentStore store(fixtures); std::uint64_t id = 1;
  for (const auto& c : cases) {
    Source source(c.first); ImportJob job(store); CHECK(job.begin(source, intent(source, id)) == Status::Ok);
    CHECK(store.count() == id - 1); run(job);
    CHECK(job.phase() == ImportPhase::Complete && job.result().outcome == Outcome::Imported);
    CHECK(store.body(id) == c.second && store.current == 777); ++id;
  }
  std::vector<std::string> invalid = {std::string(65537, 'x'), std::string("a\0b", 3), "\xc0\xaf",
    "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe4\xb8", "\x80", "\xe2X"};
  for (const auto& bytes : invalid) {
    Source source(bytes); ImportJob job(store); CHECK(job.begin(source, intent(source, id)) == Status::Ok); run(job);
    CHECK(job.phase() == ImportPhase::Failed && job.result().status == Status::InvalidText);
    CHECK(store.count() == cases.size() && store.current == 777); ++id;
  }
  for (unsigned f = 0; f < 3; ++f) {
    Source source("abc"); source.shortRead = f == 0; source.changeOnRead = f == 1; source.valid = f != 2;
    ImportJob job(store); const auto r = job.begin(source, intent(source, id++));
    if (r == Status::Ok) run(job);
    CHECK(job.phase() == ImportPhase::Failed && store.count() == cases.size());
  }
}
static void transactions() {
  std::uint64_t id = 1; Source source("new\r\n" + std::string(1600, 'x'));
  const std::vector<host::Fault> faults = {host::Fault::Reserve, host::Fault::Append, host::Fault::ShortWrite,
    host::Fault::Finish, host::Fault::Flush, host::Fault::Publish, host::Fault::Unavailable};
  for (const auto f : faults) {
    host::PersistentStore store(fixtures / std::to_string(id)); store.fault = f; ImportJob job(store);
    const auto r = job.begin(source, intent(source, id)); if (r == Status::Ok) run(job);
    CHECK(job.phase() != ImportPhase::Complete && store.count() == 0 && store.current == 777); ++id;
  }
  const auto root = fixtures / "recover";
  {
    host::PersistentStore store(root); ImportJob job(store); const auto i = intent(source, 501);
    CHECK(job.begin(source, i) == Status::Ok); job.tick(); job.cancel(); CHECK(store.count() == 0);
    CHECK(job.begin(source, i) == Status::Ok); store.fault = host::Fault::LostResult; run(job);
    CHECK(job.phase() == ImportPhase::Complete && store.count() == 1 && store.current == 777);
    source.valid = false; CHECK(job.begin(source, i) == Status::Ok && job.phase() == ImportPhase::Complete);
    CHECK(store.count() == 1); store.abort(i.operation); CHECK(store.count() == 1);
    auto open = store.openDocument(501, true); CHECK(open.status == Status::Io && open.outcome == Outcome::Imported && store.current == 777);
    open = store.openDocument(501); CHECK(open.outcome == Outcome::Opened && store.current == 501);
  }
  {
    host::PersistentStore store(root); ImportJob job(store);
    CHECK(job.begin(source, intent(source, 501)) == Status::Ok && job.phase() == ImportPhase::Complete);
    source.valid = true; CHECK(job.begin(source, intent(source, 502)) == Status::Ok); run(job);
    CHECK(store.count() == 2 && store.body(501) == store.body(502) && store.current == 777);
    Source different("other"); CHECK(job.begin(different, intent(different, 501)) == Status::Conflict);
    CHECK(store.count() == 2);
    CHECK(job.begin(source, intent(source, 503)) == Status::Ok);
    ImportJob duplicate(store); CHECK(duplicate.begin(source, intent(source, 503)) == Status::Busy);
    while (job.phase() != ImportPhase::Publishing) job.tick();
    store.fault = host::Fault::Query; job.tick(); CHECK(job.phase() == ImportPhase::Uncertain && store.count() == 3);
    store.fault = host::Fault::None;
    CHECK(job.begin(source, intent(source, 503)) == Status::Ok && job.phase() == ImportPhase::Complete && store.count() == 3);
  }
  // Recreate a store after an interrupted, unpublished write.
  {
    host::PersistentStore store(fixtures / "crash"); ImportResult r; const auto i = intent(source, 601);
    CHECK(store.reserve(i, r) == Status::Ok && store.begin(601) == Status::Ok);
    CHECK(store.append(601, "partial").status == Status::Ok && store.count() == 0);
  }
  {
    host::PersistentStore store(fixtures / "crash"); ImportJob job(store);
    CHECK(job.begin(source, intent(source, 601)) == Status::Ok); run(job); CHECK(store.count() == 1);
  }
}
static void receipt_confirmation() {
  unsigned id = 0;
  for (auto end : {EndReason::Forced, EndReason::Disconnected, EndReason::Ejected}) {
    Inbox backing; backing.files = {{"received.txt", "valid UTF-8 but completeness is not implied"}};
    MediaBridge bridge; InboxSession inbox(backing, backing, bridge);
    CHECK(inbox.begin() == Status::Ok); inbox.tick(); backing.hostCopy(bridge); seal(inbox, bridge, end);
    InboxCandidate candidate; CHECK(inbox.next(candidate) == Status::Ok);
    ImportSource* source = nullptr; CHECK(inbox.acquire(candidate, source) == Status::Ok);
    host::PersistentStore store(fixtures / std::to_string(++id)); ImportJob job(store);
    const auto result = job.begin(*source, intent(*source, id));
    if (end == EndReason::Ejected) { CHECK(result == Status::Ok); run(job); CHECK(store.count() == 1); }
    else CHECK(result == Status::Uncertain && store.count() == 0);
    CHECK(inbox.release(inbox.sourceLease()) == Status::Ok && inbox.close() == Status::Ok);
  }
}
static void budget() {
  std::printf("sizeof: ExportSession=%zu ExportVolume=%zu UsbExport=%zu InboxSession=%zu ImportJob=%zu MediaBridge=%zu ImportWire=%zu\n",
    sizeof(dx::ExportSession), sizeof(ExportVolume), sizeof(UsbExport), sizeof(InboxSession), sizeof(ImportJob), sizeof(MediaBridge), sizeof(dx::ImportWire));
  CHECK(sizeof(ImportJob) < 2048 && sizeof(InboxSession) < 256 && sizeof(ExportVolume) < 256);
}
int main(int argc, char** argv) {
  CHECK(argc == 3); fixtures = argv[2]; fs::create_directories(fixtures);
  const std::pair<const char*, void(*)()> tests[] = {{"export_volume", export_volume}, {"request_lifetime", request_lifetime},
    {"inbox_ownership", inbox_ownership}, {"import_text", import_text}, {"transactions", transactions}, {"budget", budget}, {"receipt_confirmation", receipt_confirmation}};
  for (auto test : tests) if (test.first == std::string(argv[1])) { test.second(); std::printf("PASS %s checks=%zu\n", test.first, checks); return 0; }
  return 2;
}
