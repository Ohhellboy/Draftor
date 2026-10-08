#include "hanime/host_resource_file.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hanime {
namespace {
struct Timer {
  HostResourceMetrics& m;
  std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~Timer() {
    auto n=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
    m.calls_ns+=n; m.max_call_ns=std::max(m.max_call_ns,n);
  }
};
}
HostResourceFile::HostResourceFile(std::string_view path,std::size_t limit,HostResourceMetrics& m):metrics_(m),max_read_(limit) {
  std::array<char,512> name{}; // Bounded construction scratch, no retained path.
  if(path.empty() || path.size()>=name.size() || path.find('\0')!=std::string_view::npos || !limit) return;
  std::memcpy(name.data(),path.data(),path.size());
  {Timer timer{m}; ++m.control_calls; fd_=::open(name.data(),O_RDONLY);}
  if(fd_<0) return;
  ++m.open_files; m.peak_open_files=std::max(m.peak_open_files,m.open_files);
  struct stat info{}; int result=0;
  {Timer timer{m}; ++m.control_calls; result=::fstat(fd_,&info);}
  if(result!=0 || !S_ISREG(info.st_mode) || info.st_size<=0 ||
      static_cast<std::uint64_t>(info.st_size)>std::numeric_limits<std::size_t>::max()) {close();return;}
  bytes_=static_cast<std::size_t>(info.st_size);
}
HostResourceFile::~HostResourceFile(){close();}
bool HostResourceFile::valid() const noexcept{return fd_>=0 && bytes_;}
std::size_t HostResourceFile::size() const{return bytes_;}
bool HostResourceFile::read(std::size_t off,void* destination,std::size_t count) const {
  if(!valid() || !destination || !count || count>max_read_ || off>bytes_ || count>bytes_-off ||
      off>static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) return false;
  Timer timer{metrics_}; ++metrics_.read_calls; metrics_.requested_bytes+=count;
  // One actual syscall, short/error/EINTR is failure, never hidden retry loops.
  const auto result=::pread(fd_,destination,count,static_cast<off_t>(off));
  if(result>0) metrics_.delivered_bytes+=static_cast<std::uint64_t>(result);
  return result>=0 && static_cast<std::size_t>(result)==count;
}
bool HostResourceFile::close() {
  if(fd_<0) return true;
  Timer timer{metrics_}; ++metrics_.control_calls; const int fd=fd_; fd_=-1; bytes_=0;
  --metrics_.open_files;
  return ::close(fd)==0;
}
} // namespace hanime
