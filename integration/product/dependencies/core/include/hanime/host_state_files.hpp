#pragma once
#include "hanime/state_v3.hpp"

namespace hanime {
enum class StateFileOperation { None, Open, Size, Read, Write, Sync, Truncate, Close };
// Faults are opt-in, one controlled syscall boundary, still operate real files.
struct StateFileFault {
  StateFileOperation operation{StateFileOperation::None};
  std::size_t skip{0}, times{0}, short_bytes{0};
  bool perform_then_fail{false};
};
struct HostStateMetrics { StateCost requests; std::uint64_t calls_ns{0}, max_call_ns{0}; };
class HostStateFiles final : public StateFilePort {
 public:
  static constexpr std::size_t path_capacity=512;
  HostStateFiles(std::string_view path_a, std::string_view path_b);
  ~HostStateFiles() override; // Only best-effort fd release; never fsync/write.
  bool valid() const;
  FileResult open(unsigned,bool) override;
  bool size(unsigned,std::uint64_t&) override;
  FileTransfer read(unsigned,std::uint64_t,void*,std::size_t) override;
  FileTransfer write(unsigned,std::uint64_t,const void*,std::size_t) override;
  bool sync(unsigned) override;
  bool truncate(unsigned,std::uint64_t) override;
  bool close(unsigned) override;
  StateFileFault fault;
  HostStateMetrics metrics;
 private:
  std::array<std::array<char,path_capacity>,2> paths_{};
  std::array<int,2> fds_{{-1,-1}};
  bool valid_{false};
  bool inject(StateFileOperation);
};
} // namespace hanime
