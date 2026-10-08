#pragma once
#include "hanime/core.hpp"

namespace hanime {
struct HostResourceMetrics {
  std::uint64_t read_calls{0}, requested_bytes{0}, delivered_bytes{0};
  std::uint64_t control_calls{0}, calls_ns{0}, max_call_ns{0};
  std::size_t open_files{0}, peak_open_files{0};
};
// One immutable file, no read-ahead / whole-file buffer / writable operations.
// Metrics outlive this reader. Serial use. Replacing bytes in an open resource
// file is outside the immutable-resource contract; activate a new package instead.
class HostResourceFile final : public ResourceReader {
 public:
  HostResourceFile(std::string_view path,std::size_t max_read,HostResourceMetrics&);
  ~HostResourceFile() override;
  HostResourceFile(const HostResourceFile&)=delete;
  HostResourceFile& operator=(const HostResourceFile&)=delete;
  bool valid() const noexcept;
  bool healthy() const override { return valid(); }
  std::size_t size() const override;
  bool read(std::size_t,void*,std::size_t) const override;
  bool close();
 private:
  HostResourceMetrics& metrics_;
  int fd_{-1}; std::size_t bytes_{0}, max_read_{0};
};
} // namespace hanime
