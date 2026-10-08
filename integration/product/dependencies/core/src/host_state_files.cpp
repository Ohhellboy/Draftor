#include "hanime/host_state_files.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <limits>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hanime {
namespace {
struct Timer {
  HostStateMetrics& m;
  std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~Timer() {
    const auto n=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-start).count());
    m.calls_ns+=n; m.max_call_ns=std::max(m.max_call_ns,n);
  }
};
bool offset_ok(std::uint64_t off,std::size_t n=0) {
  const auto max=static_cast<std::uint64_t>(std::numeric_limits<off_t>::max());
  return off<=max && n<=max-off;
}
}
HostStateFiles::HostStateFiles(std::string_view a,std::string_view b) {
  valid_=!a.empty() && !b.empty() && a!=b && a.size()<path_capacity && b.size()<path_capacity &&
      a.find('\0')==std::string_view::npos && b.find('\0')==std::string_view::npos;
  if (valid_) {std::memcpy(paths_[0].data(),a.data(),a.size()); std::memcpy(paths_[1].data(),b.data(),b.size());}
}
HostStateFiles::~HostStateFiles() {for (auto fd:fds_) if (fd>=0) ::close(fd);}
bool HostStateFiles::valid() const {return valid_;}
bool HostStateFiles::inject(StateFileOperation op) {
  if (fault.operation!=op || !fault.times) return false;
  if (fault.skip) {--fault.skip; return false;}
  --fault.times; return true;
}
FileResult HostStateFiles::open(unsigned f,bool create) {
  Timer timer{metrics}; ++metrics.requests.control_calls;
  if (!valid_ || f>1 || fds_[f]>=0 || inject(StateFileOperation::Open)) return FileResult::Error;
  fds_[f]=::open(paths_[f].data(),O_RDWR|(create ? O_CREAT|O_EXCL : 0),0600);
  return fds_[f]>=0 ? FileResult::Ok : (errno==ENOENT ? FileResult::Missing : FileResult::Error);
}
bool HostStateFiles::size(unsigned f,std::uint64_t& n) {
  Timer timer{metrics}; ++metrics.requests.control_calls;
  if (f>1 || inject(StateFileOperation::Size)) return false;
  struct stat s{}; if (::fstat(fds_[f],&s)!=0 || s.st_size<0) return false;
  n=static_cast<std::uint64_t>(s.st_size); return true;
}
FileTransfer HostStateFiles::read(unsigned f,std::uint64_t off,void* p,std::size_t n) {
  Timer timer{metrics}; ++metrics.requests.read_calls; metrics.requests.read_bytes+=n;
  if (f>1 || !offset_ok(off,n)) return {};
  const bool faulted=inject(StateFileOperation::Read);
  if (faulted && !fault.perform_then_fail && !fault.short_bytes) return {};
  const auto request=faulted && fault.short_bytes ? std::min(n,fault.short_bytes) : n;
  const auto r=::pread(fds_[f],p,request,static_cast<off_t>(off));
  return {r>=0 && !(faulted && fault.perform_then_fail),r>0 ? static_cast<std::size_t>(r) : 0};
}
FileTransfer HostStateFiles::write(unsigned f,std::uint64_t off,const void* p,std::size_t n) {
  Timer timer{metrics}; ++metrics.requests.write_calls; metrics.requests.write_bytes+=n;
  if (f>1 || !offset_ok(off,n)) return {};
  const bool faulted=inject(StateFileOperation::Write);
  if (faulted && !fault.perform_then_fail && !fault.short_bytes) return {};
  const auto request=faulted && fault.short_bytes ? std::min(n,fault.short_bytes) : n;
  const auto r=::pwrite(fds_[f],p,request,static_cast<off_t>(off));
  return {r>=0 && !(faulted && fault.perform_then_fail),r>0 ? static_cast<std::size_t>(r) : 0};
}
bool HostStateFiles::sync(unsigned f) {
  Timer timer{metrics}; ++metrics.requests.control_calls;
  const bool faulted=inject(StateFileOperation::Sync);
  if (f>1 || (faulted && !fault.perform_then_fail)) return false;
  return ::fsync(fds_[f])==0 && !faulted;
}
bool HostStateFiles::truncate(unsigned f,std::uint64_t n) {
  Timer timer{metrics}; ++metrics.requests.control_calls;
  if (f>1 || !offset_ok(n) || inject(StateFileOperation::Truncate)) return false;
  return ::ftruncate(fds_[f],static_cast<off_t>(n))==0;
}
bool HostStateFiles::close(unsigned f) {
  Timer timer{metrics}; ++metrics.requests.control_calls;
  if (f>1 || inject(StateFileOperation::Close)) return false;
  if (fds_[f]<0) return true;
  const auto fd=fds_[f]; fds_[f]=-1; return ::close(fd)==0;
}
} // namespace hanime
