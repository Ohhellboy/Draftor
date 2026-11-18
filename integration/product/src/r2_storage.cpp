#include "r2_storage.hpp"
#include "tf_resources.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <limits>
namespace r2 {
#if R2_HOST_TEST
Fault fault;
static bool inject(FaultOp op){if(fault.op!=op||fault.fired)return false;if(fault.after){--fault.after;return false;}fault.fired=true;errno=EIO;return true;}
#define INJECT(op) inject(FaultOp::op)
#else
#define INJECT(op) false
#endif
bool allowedStatePath(const std::string& p){
 if(p.find("..")!=std::string::npos)return false;
#if R2_HOST_TEST
 // Host rigs use only the task-owned temporary fixture tree.
 return p.rfind("/private/tmp/bgime-r2-",0)==0||p.rfind("/tmp/bgime-r2-",0)==0;
#else
 return p.rfind(std::string(StateRoot)+"/",0)==0;
#endif
}
bool prepareDailyRoot(std::shared_ptr<const ime_device::Mount> mount){
#if BGIME_A_REV1_DAILY && !R2_HOST_TEST
 if(!mount||!mount->available||!mount->epoch){errno=ENODEV;return false;}
#if WAVESHARE_R7_L1_AREV1_TEST_ONLY
 const std::string root=StateRoot,prefix="/sd/";
#else
 const std::string root=StateRoot,prefix="/sd/HANIME_USER/";
#endif
 if(root.rfind(prefix,0)!=0||root.size()==prefix.size()||root.find("..")!=std::string::npos||
    root.find('/',prefix.size())!=std::string::npos){errno=EINVAL;return false;}
 // FatFs has no symlink entries. Accept only directories, and never remove an
 // incompatible existing object in order to create a fresh learning store.
#if WAVESHARE_R7_L1_AREV1_TEST_ONLY
 for(const char* name:{StateRoot}){
#else
 for(const char* name:{"/sd/HANIME_USER",StateRoot}){
#endif
  struct stat info{};
  if(::stat(name,&info)==0){if(!S_ISDIR(info.st_mode)){errno=ENOTDIR;return false;}continue;}
  if(errno!=ENOENT||::mkdir(name,0700)!=0)return false;
  if(::stat(name,&info)!=0||!S_ISDIR(info.st_mode))return false;
 }
 return true;
#else
 (void)mount;errno=EPERM;return false; // Host fixtures provision their own /tmp root.
#endif
}
FileOps::FileOps(std::shared_ptr<const ime_device::Mount> m):mount_(std::move(m)),epoch_(mount_?mount_->epoch:0){}
bool FileOps::current()const {if(mount_&&mount_->available&&epoch_&&mount_->epoch==epoch_)return true;errno=ENODEV;return false;}
int FileOps::open(const std::string& p,int flags,int mode)const {if(!current()||!allowedStatePath(p)){errno=EPERM;return -1;}if(INJECT(Open))return -1;return ::open(p.c_str(),flags,mode);}
int FileOps::stat(const std::string& p,struct stat* s)const {if(!current()||!allowedStatePath(p)){errno=EPERM;return -1;}return ::stat(p.c_str(),s);}
int FileOps::size(int fd,struct stat* s)const {return current()?::fstat(fd,s):-1;}
static bool offset(off_t off,size_t n){return off>=0&&n<=uint64_t(std::numeric_limits<off_t>::max())-uint64_t(off);}
ssize_t FileOps::read(int fd,void* p,size_t n,off_t off)const {if(!current()||!offset(off,n))return -1;if(::lseek(fd,off,SEEK_SET)!=off)return -1;if(INJECT(Read))return n?::read(fd,p,n-1):-1;return ::read(fd,p,n);}
ssize_t FileOps::write(int fd,const void* p,size_t n,off_t off)const {if(!current()||!offset(off,n))return -1;if(::lseek(fd,off,SEEK_SET)!=off)return -1;if(INJECT(Write))return n?::write(fd,p,n-1):-1;return ::write(fd,p,n);}
int FileOps::sync(int fd)const {if(!current()||INJECT(Sync))return -1;return ::fsync(fd);}
int FileOps::truncate(int fd,off_t n)const {return current()?::ftruncate(fd,n):-1;}
int FileOps::close(int fd)const {
 // On retired mounts a numeric descriptor might already belong to a new VFS.
 // Never touch it. The unmount owner must retire handles before unmounting.
 if(!current())return -1;
 const bool failed=INJECT(Close);const int rc=::close(fd);return failed?-1:rc;
}
int FileOps::replace(const std::string& pending,const std::string& main)const {
 if(!current()||!allowedStatePath(pending)||!allowedStatePath(main)){errno=EPERM;return -1;}
 // FatFs does not promise POSIX rename-over-existing. A synced, closed pending
 // file survives the gap. Missing main + pending fails closed on reopen; the
 // authoritative Count remains usable by accurate E. This is not atomic rename.
 if(INJECT(Replace))return -1;
 if(::unlink(main.c_str())!=0&&errno!=ENOENT)return -1;
 if(!current())return -1;
 return ::rename(pending.c_str(),main.c_str());
}
}
