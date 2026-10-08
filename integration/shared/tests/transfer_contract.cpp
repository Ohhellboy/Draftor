#include "draftor_exchange/import_job.hpp"
#include "draftor_exchange/transfer_selection.hpp"
#include "check.hpp"
#include <limits>
using namespace draftor::exchange;
struct Source final : ImportSource {
  std::string bytes = "\xef\xbb\xbf" "a\r\n";
  bool acknowledged = true;
  SourceIdentity identity() const noexcept override { return {0x123456789abcdef0ull, 2, bytes.size()}; }
  std::uint64_t sourceLease() const noexcept override { return 3; }
  bool stable() const noexcept override { return true; }
  bool confirmed() const noexcept override { return acknowledged; }
  IoResult read(std::uint64_t at, void* out, std::size_t n) noexcept override {
    if (at > bytes.size() || n > bytes.size() - at) return {Status::Bounds, 0};
    std::memcpy(out, bytes.data() + at, n); return {Status::Ok, n};
  }
};
struct Store final : DocumentImportPort {
  ImportResult receipt{}; std::string body; unsigned reserves=0, appends=0, finishes=0, publishes=0, aborts=0;
  bool publishWait=false, importedBeforeWait=false;
  Status reserve(const ImportIntent& i, ImportResult& out) noexcept override {
    ++reserves;
    if (receipt.operation == i.operation && receipt.outcome == Outcome::Imported) { out=receipt; return Status::Ok; }
    receipt={}; receipt.operation=i.operation; receipt.outcome=Outcome::Reserved;
    for(unsigned n=0;n<16;++n) { receipt.document[n]=0xa0+n; receipt.collection[n]=0xb0+n; }
    out=receipt; body.clear(); return Status::Ok;
  }
  Status begin(std::uint64_t) noexcept override { receipt.outcome=Outcome::Writing; return Status::Ok; }
  IoResult append(std::uint64_t, std::string_view b) noexcept override { ++appends; body.append(b); return {Status::Ok,b.size()}; }
  Status finish(std::uint64_t, std::size_t n) noexcept override {
    ++finishes; CHECK(n==body.size()); if(finishes<4) return Status::Busy;
    receipt.bodyBytes=n; receipt.outcome=Outcome::Verified; return Status::Ok;
  }
  Status publish(std::uint64_t) noexcept override {
    ++publishes;
    if(importedBeforeWait) receipt.outcome=Outcome::Imported;
    if(publishWait || publishes<4) return Status::Busy;
    receipt.outcome=Outcome::Imported; return Status::Ok;
  }
  void abort(std::uint64_t) noexcept override { ++aborts; if(receipt.outcome!=Outcome::Imported) receipt.outcome=Outcome::Aborted; }
  Status query(std::uint64_t, ImportResult& out) noexcept override { out=receipt; return Status::Ok; }
};
static ImportIntent intent(Source& s) { ImportIntent i; i.operation=91; i.source=s.identity(); i.name[0]='x'; return i; }
static void finish(ImportJob& j) {
  for(unsigned i=0;i<100;++i) {
    const auto p=j.tick(); if(p==ImportPhase::Complete||p==ImportPhase::Failed||p==ImportPhase::Uncertain) return;
  }
  CHECK(false);
}
int main() {
  Source source; Store store; ImportJob job(store,{2});
  CHECK(job.begin(source,intent(source))==Status::Ok); finish(job);
  CHECK(job.phase()==ImportPhase::Complete && store.body=="a\n" && store.finishes==4 && store.publishes==4);
  CHECK(job.result().document[15]==0xaf && job.result().collection[15]==0xbf && job.result().operation==91);
  CHECK(store.appends==1 && store.aborts==0);
  source.acknowledged=false; Store unconfirmed; ImportJob recovery(unconfirmed);
  CHECK(recovery.begin(source,intent(source))==Status::Uncertain && unconfirmed.reserves==0);
  source.acknowledged=true;
  Store limited; ImportJob tooSmall(limited,{1}); CHECK(tooSmall.begin(source,intent(source))==Status::TooLarge);
  Store invalid; ImportJob invalidQuota(invalid,{std::numeric_limits<std::size_t>::max()});
  CHECK(invalidQuota.begin(source,intent(source))==Status::InvalidArgument && invalid.reserves==0);
  for(bool published : {false,true}) {
    Store pending; pending.publishWait=true; pending.importedBeforeWait=published; ImportJob j(pending);
    CHECK(j.begin(source,intent(source))==Status::Ok);
    for(unsigned n=0;n<30 && !pending.publishes;++n) j.tick();
    CHECK(j.phase()==ImportPhase::Publishing); j.cancel();
    CHECK(j.phase()==(published?ImportPhase::Complete:ImportPhase::Uncertain));
    CHECK(j.result().outcome!=Outcome::Aborted);
  }
  // These QR/Wi-Fi ports are explicitly Host-only selection contracts.
  // Raw QR bytes and TXT normalization are independently preserved in one build.
  TransferSelection owner; std::uint64_t a=0,b=777;
  for(auto terminal : {TransferStage::Closing,TransferStage::Failed}) {
    CHECK(owner.acquire(TransferChannel::Qr,a)==Status::Ok);
    const std::string raw=source.bytes;
    CHECK(owner.acquire(TransferChannel::Wifi,b)==Status::Busy && b==777 && raw==source.bytes);
    CHECK(owner.stage(a,TransferStage::Transferring)==Status::Ok);
    CHECK(owner.stage(a,terminal)==Status::Ok && owner.release(a,false)==Status::Busy);
    CHECK(owner.acquire(TransferChannel::UsbImport,b)==Status::Busy && owner.channel()==TransferChannel::Qr);
    CHECK(owner.release(a,true)==Status::Ok && owner.release(a,true)==Status::Stale);
    CHECK(owner.acquire(TransferChannel::Wifi,b)==Status::Ok && b!=a);
    CHECK(owner.stage(a,TransferStage::Closing)==Status::Stale);
    Store next; ImportJob text(next); CHECK(text.begin(source,intent(source))==Status::Ok); finish(text);
    CHECK(raw=="\xef\xbb\xbf" "a\r\n" && next.body=="a\n");
    CHECK(owner.stage(b,TransferStage::Closing)==Status::Ok && owner.release(b,true)==Status::Ok); b=777;
  }
  std::printf("PASS neutral transfer contracts checks=%zu\n",checks);
}
