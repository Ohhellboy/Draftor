#include "s3_vfs.hpp"
#include "device_metrics.hpp"
#include "io_profile.hpp"
#include "operation_profile.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <algorithm>
#include <cstring>

namespace s3fs {
struct File::Handle {
 int fd=-1;
 DIR* dir=nullptr;
 bool failed=false, writing=false;
 bool counted=false;
 std::string path;
 ~Handle(){if(counted)ime_device::metrics.closeHandle(false);s3profile::Scope timing(s3profile::Access);if(fd>=0)s3profile::call(s3profile::Close, [&] { return ::close(fd); });if(dir)s3profile::call(s3profile::Closedir, [&] { return ::closedir(dir); });}
};

File::File(const std::string& path,const char* mode, OpenKind kind){
 s3profile::Scope timing(s3profile::Access);
#ifndef ARDUINO
 if(openObserver)openObserver(path.c_str(),mode);
#endif
 if(s3profile::active)++s3profile::record.opens;
 auto h=std::make_shared<Handle>();h->path=path;
 struct stat st{};
 bool directory=kind==OpenKind::Directory;
 if(kind==OpenKind::Any)
  directory=s3profile::call(s3profile::Stat, [&] { return ::stat(path.c_str(),&st); })==0&&S_ISDIR(st.st_mode);
 if(directory){
  if(std::strcmp(mode,Read))return;
  h->dir=s3profile::call(s3profile::Opendir, [&] { return ::opendir(path.c_str()); });
  if(!h->dir){open_errno_=errno;return;}
  h->counted=true;ime_device::metrics.openHandle(false);
 }else{
  int flags=O_RDONLY;
  if(!std::strcmp(mode,Write)){flags=O_WRONLY|O_CREAT|O_TRUNC;h->writing=true;}
  else if(!std::strcmp(mode,Append)){flags=O_WRONLY|O_CREAT|O_APPEND;h->writing=true;}
  else if(std::strcmp(mode,Read))return;
  h->fd=s3profile::call(s3profile::Open, [&] { return ::open(path.c_str(),flags,0666); });
  if(h->fd<0){open_errno_=errno;return;}
  h->counted=true;ime_device::metrics.openHandle(false);
  // FAT f_open already rejects directories. fstat also enforces this on host POSIX.
  // ESP-IDF fstat reads the live FIL size/type without another pathname traversal.
  if(kind==OpenKind::Regular){
   if(s3profile::call(s3profile::Fstat, [&] { return ::fstat(h->fd,&st); })!=0){open_errno_=errno;return;}
   if(!S_ISREG(st.st_mode)){open_errno_=EISDIR;return;}
  }
 }
 h_=std::move(h);
}
File::operator bool()const{return h_&&(h_->fd>=0||h_->dir);}
bool File::isDirectory()const{return h_&&h_->dir;}
bool File::isFile()const{return h_&&h_->fd>=0;}
size_t File::size()const{
 s3profile::Scope timing(s3profile::Access);
 struct stat st{};
 if(!h_||h_->fd<0||s3profile::call(s3profile::Fstat, [&] { return ::fstat(h_->fd,&st); })!=0){
  if(h_)h_->failed=true;
  return size_t(-1);
 }
 return st.st_size;
}
int File::read(uint8_t* p,size_t n){
 s3profile::Scope timing(s3profile::Read);
 if(s3profile::active){
  auto& r=s3profile::record;
  ++r.reads;r.max_read=std::max(r.max_read,uint32_t(n));
  ++r.read_sizes[n<=64?0:n<=512?1:n<=4096?2:3];
 }
 if(!h_||h_->fd<0||faults.read)return -1;
 if(faults.max_read_bytes)n=std::min(n,faults.max_read_bytes);
 const auto started=ime_device::clockUs();ssize_t count=::read(h_->fd,p,n);
 ime_device::ioProfile.record(ime_device::IoKind::DocumentRead,n,count>0?size_t(count):0,ime_device::clockUs()-started,count>=0);
 if(count<0)h_->failed=true;
 else if(s3profile::active)s3profile::record.read_bytes+=count;
 return int(count);
}
size_t File::write(const uint8_t* p,size_t n){
 s3profile::Scope timing(s3profile::Write);
 if(s3profile::active)++s3profile::record.writes;
 if(!h_||h_->fd<0)return 0;
 if(faults.write_bytes>=0)n=std::min(n,size_t(faults.write_bytes));
 const auto started=ime_device::clockUs();ssize_t count=::write(h_->fd,p,n);
 ime_device::ioProfile.record(ime_device::IoKind::DocumentWrite,n,count>0?size_t(count):0,ime_device::clockUs()-started,count>=0&&size_t(count)==n);
 if(count<0){h_->failed=true;return 0;}
 if(faults.write_bytes>=0)faults.write_bytes-=count;
 if(s3profile::active)s3profile::record.write_bytes+=count;
 return count;
}
bool File::flush(){
 s3profile::Scope timing(s3profile::Write);
 if(!h_||h_->fd<0)return false;
 bool ok=false;
 if(!h_->failed){
  const auto started=ime_device::clockUs();const auto result=s3profile::call(s3profile::Fsync, [&] { return ::fsync(h_->fd); });
  ime_device::ioProfile.record(ime_device::IoKind::DocumentSync,0,0,ime_device::clockUs()-started,result==0);
  ok=result==0&&!faults.flush;
 }
 if(!ok)h_->failed=true;
 return ok;
}
bool File::close(){
 s3profile::Scope timing(s3profile::Access);
 if(!h_)return false;
 bool ok=!h_->failed;
 if(h_->fd>=0){ok=(s3profile::call(s3profile::Close, [&] { return ::close(h_->fd); })==0)&&ok;h_->fd=-1;}
 if(h_->dir){ok=(s3profile::call(s3profile::Closedir, [&] { return ::closedir(h_->dir); })==0)&&ok;h_->dir=nullptr;}
 if(h_->writing&&faults.close)ok=false;
 if(h_->counted){ime_device::metrics.closeHandle(false);h_->counted=false;}
 h_.reset();
 return ok;
}
const char* File::name()const{
 if(!h_)return "";
 auto slash=h_->path.rfind('/');
 return h_->path.c_str()+(slash==std::string::npos?0:slash+1);
}
bool File::nextEntry(DirectoryEntry& out){
 s3profile::Scope timing(s3profile::Access);
 out={};
 if(!isDirectory())return false;
 if(faults.directory){h_->failed=true;return false;}
 for(;;){
  errno=0;
  auto entry=s3profile::call(s3profile::Readdir, [&] { return ::readdir(h_->dir); });
  if(!entry){if(errno)h_->failed=true;return false;}
  if(!std::strcmp(entry->d_name,".")||!std::strcmp(entry->d_name,".."))continue;
  if(s3profile::active)++s3profile::record.entries;
  out.name=entry->d_name;
  out.directory=entry->d_type==DT_DIR;
  out.regular=entry->d_type==DT_REG;
  if(entry->d_type==DT_UNKNOWN){
   struct stat st{};
   if(s3profile::call(s3profile::Stat, [&] { return ::stat((h_->path+"/"+out.name).c_str(),&st); })!=0){h_->failed=true;return false;}
   out.directory=S_ISDIR(st.st_mode);out.regular=S_ISREG(st.st_mode);
  }
  return true;
 }
}
bool File::error()const{return h_&&h_->failed;}
File Fs::open(const char* p,const char* mode)const{return File(root_+p,mode,File::OpenKind::Any);}
File Fs::openFile(const char* p,const char* mode)const{return File(root_+p,mode,File::OpenKind::Regular);}
File Fs::openDirectory(const char* p)const{return File(root_+p,Read,File::OpenKind::Directory);}
bool Fs::exists(const char* p)const{
 s3profile::Scope timing(s3profile::Access);
 struct stat st{};
 return s3profile::call(s3profile::Stat, [&] { return ::stat((root_+p).c_str(),&st); })==0;
}
PathKind Fs::kind(const char* p)const{
 s3profile::Scope timing(s3profile::Access);
 struct stat st{};
 if(s3profile::call(s3profile::Stat, [&] { return ::stat((root_+p).c_str(),&st); })!=0)return errno==ENOENT?PathKind::Missing:PathKind::Error;
 if(S_ISDIR(st.st_mode))return PathKind::Directory;
 return S_ISREG(st.st_mode)?PathKind::File:PathKind::Error;
}
bool Fs::mkdir(const char* p)const{
 s3profile::Scope timing(s3profile::Access);
 return s3profile::call(s3profile::Mkdir, [&] { return ::mkdir((root_+p).c_str(),0777); })==0;
}
bool Fs::remove(const char* p)const{
 s3profile::Scope timing(s3profile::Access);
 return s3profile::call(s3profile::Unlink, [&] { return ::unlink((root_+p).c_str()); })==0;
}
bool Fs::rename(const char* a,const char* b)const{
 s3profile::Scope timing(s3profile::Access);
 return !faults.rename&&s3profile::call(s3profile::Rename, [&] { return ::rename((root_+a).c_str(),(root_+b).c_str()); })==0;
}
}
