#include "draftor_usb_transfer/export.hpp"
#include <algorithm>
#include <cstring>
#include <exception>

namespace draftor::usb {
namespace {
void put16(std::uint8_t* p, std::uint16_t n) { p[0] = n; p[1] = n >> 8; }
void put32(std::uint8_t* p, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) p[i] = n >> (8 * i);
}
}
Status ExportVolume::bind(const exchange::Lease& l) noexcept {
  if (bound_) return Status::Busy;
  exchange::Limits limits;
  if (!quota_.checked(limits) || quota_.body > std::size_t(Clusters) * SectorBytes) return Status::InvalidArgument;
  if (l.size > quota_.body || (l.size && !l.data) || !l.token.session || !l.token.generation)
    return Status::InvalidArgument;
  lease_ = l; bound_ = true; return Status::Ok;
}
void ExportVolume::clear() noexcept { lease_ = {}; bound_ = false; }
void ExportVolume::sector(std::uint32_t lba, std::uint8_t* out) const noexcept {
  std::memset(out, 0, SectorBytes);
  const auto clusters = static_cast<std::uint32_t>((lease_.size + 511) / 512);
  if (!lba) {
    out[0] = 0xeb; out[1] = 0x3c; out[2] = 0x90;
    std::memcpy(out + 3, "DRAFTOR ", 8); put16(out + 11, 512);
    out[13] = 1; put16(out + 14, 1); out[16] = 2; put16(out + 17, 64);
    put16(out + 19, Sectors); out[21] = 0xf8; put16(out + 22, FatSectors);
    put16(out + 24, 32); put16(out + 26, 64); out[36] = 0x80; out[38] = 0x29;
    put32(out + 39, static_cast<std::uint32_t>(lease_.token.session));
    std::memcpy(out + 43, "DRAFTOR    ", 11); std::memcpy(out + 54, "FAT16   ", 8);
    out[510] = 0x55; out[511] = 0xaa;
  } else if (lba < RootStart) {
    const auto first = ((lba - 1) % FatSectors) * 256;
    for (unsigned i = 0; i < 256; ++i) {
      const auto c = first + i;
      std::uint16_t value = 0;
      if (c == 0) value = 0xfff8;
      else if (c == 1) value = 0xffff;
      else if (c < clusters + 2) value = c == clusters + 1 ? 0xffff : c + 1;
      put16(out + i * 2, value);
    }
  } else if (lba == RootStart) {
    std::memcpy(out, "DRAFTOR    ", 11); out[11] = 8;
    auto* e = out + 32;
    std::memcpy(e, "DOCUMENTTXT", 11); e[11] = 0x21;
    put16(e + 16, 0x21); put16(e + 18, 0x21); put16(e + 24, 0x21);
    put16(e + 26, clusters ? 2 : 0); put32(e + 28, lease_.size);
  } else if (lba >= DataStart) {
    const auto at = static_cast<std::size_t>(lba - DataStart) * SectorBytes;
    if (at < lease_.size) std::memcpy(out, lease_.data + at, std::min(SectorBytes, lease_.size - at));
  }
}
IoResult ExportVolume::read(std::uint64_t at, void* buffer, std::size_t n) noexcept {
  if (!bound_) return {Status::NotReady, 0};
  if (n && !buffer) return {Status::InvalidArgument, 0};
  constexpr std::uint64_t capacity = std::uint64_t(Sectors) * SectorBytes;
  if (at > capacity || n > capacity - at) return {Status::Bounds, 0};
  auto* dst = static_cast<std::uint8_t*>(buffer);
  std::uint8_t scratch[SectorBytes];
  std::size_t done = 0;
  while (done < n) {
    const auto offset = static_cast<std::size_t>(at % SectorBytes);
    const auto take = std::min(n - done, SectorBytes - offset);
    sector(at / SectorBytes, scratch);
    std::memcpy(dst + done, scratch + offset, take);
    at += take; done += take;
  }
  return {Status::Ok, n};
}
UsbExport::~UsbExport() {
  if (phase_ != ExportPhase::Idle && phase_ != ExportPhase::Closed) std::terminate();
}
Status UsbExport::begin() noexcept {
  if (phase_ != ExportPhase::Idle && phase_ != ExportPhase::Closed) return Status::Busy;
  if (session_.begin() != exchange::SessionError::None) return Status::NotReady;
  phase_ = ExportPhase::Preparing; error_ = Status::Ok; return Status::Ok;
}
void UsbExport::close(EndReason why) noexcept {
  if (phase_ == ExportPhase::Idle || phase_ == ExportPhase::Closed) return;
  if (published_) media_.withdraw(why);
  // The single R1 lease pins E while the platform drains its own requests.
  session_.stop(); phase_ = ExportPhase::Closing;
}
ExportPhase UsbExport::tick() noexcept {
  if (phase_ == ExportPhase::Preparing) {
    if (session_.tick() == exchange::Phase::Ready) {
      if (session_.acquire(lease_) != exchange::ReaderStatus::Ok) {
        error_ = Status::NotReady; close();
      } else {
        leased_ = true; error_ = volume_.bind(lease_);
        if (error_ == Status::Ok) error_ = media_.publish(volume_, Access::ReadOnly, generation_);
        if (error_ != Status::Ok) close();
        else { published_ = true; phase_ = ExportPhase::Published; }
      }
    } else if (session_.phase() == exchange::Phase::Closed) {
      error_ = Status::NotReady; phase_ = ExportPhase::Closed;
    }
  } else if (phase_ == ExportPhase::Published) {
    session_.tick();
    const auto m = media_.state();
    if (!m.accepting || m.generation != generation_ || session_.phase() != exchange::Phase::Ready) {
      if (m.error != Status::Ok) error_ = m.error;
      else if (session_.phase() != exchange::Phase::Ready || m.generation != generation_) error_ = Status::Stale;
      close(m.end == EndReason::None ? EndReason::Forced : m.end);
    }
  }
  if (phase_ == ExportPhase::Closing) {
    if (published_ && media_.detach() != Status::Ok) return phase_;
    published_ = false; volume_.clear();
    if (leased_) session_.release(lease_.token);
    leased_ = false; lease_ = {}; phase_ = ExportPhase::Closed;
  }
  return phase_;
}
} // namespace draftor::usb
