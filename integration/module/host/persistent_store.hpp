#pragma once
#include "draftor_usb_transfer/ports.hpp"
#include <filesystem>

namespace draftor::usb::host {
enum class Fault { None, Reserve, Append, ShortWrite, Finish, Flush, Publish, LostResult, Query, Unavailable };
// Host reference, not a new Draftor save system. Each published directory
// atomically contains body + intent. Reopening this object recovers receipts.
class PersistentStore final : public DocumentImportPort {
 public:
  explicit PersistentStore(std::filesystem::path root);
  ~PersistentStore();
  Status reserve(const ImportIntent&, ImportResult&) noexcept override;
  Status begin(std::uint64_t) noexcept override;
  IoResult append(std::uint64_t, std::string_view) noexcept override;
  Status finish(std::uint64_t, std::size_t) noexcept override;
  Status publish(std::uint64_t) noexcept override;
  void abort(std::uint64_t) noexcept override;
  Status query(std::uint64_t, ImportResult&) noexcept override;
  ImportResult openDocument(std::uint64_t, bool failOpen = false) noexcept;
  std::size_t count() const;
  std::string body(std::uint64_t) const;
  Fault fault = Fault::None;
  std::uint64_t current = 777; // unrelated current document sentinel
 private:
  std::filesystem::path path(std::uint64_t, bool published) const;
  void closeFd() noexcept;
  std::filesystem::path root_;
  std::uint64_t active_ = 0;
  std::size_t written_ = 0;
  Outcome state_ = Outcome::None;
  int fd_ = -1;
};
} // namespace draftor::usb::host
