#pragma once
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
namespace ime_device {struct Mount;}
#ifndef BGIME_A_REV1_DAILY
#define BGIME_A_REV1_DAILY 0
#endif
#if BGIME_A_REV1_DAILY && !BGIME_L1_A_REV1_TEST_ONLY
#error "Daily lifecycle requires the inherited A_REV1 policy and six-candidate behavior"
#endif
namespace r2 {
#if WAVESHARE_R7_L1_AREV1_TEST_ONLY
inline constexpr const char* StateRoot="/sd/WAVESHARE_R7_L1_AREV1_TEST_20261112";
#elif BGIME_A_REV1_DAILY
#ifndef BGIME_A_REV1_DAILY_ROOT
#define BGIME_A_REV1_DAILY_ROOT "/sd/HANIME_USER/A_REV1_DAILY_V1"
#endif
inline constexpr const char* StateRoot=BGIME_A_REV1_DAILY_ROOT;
#elif BGIME_L1_A_REV1_TEST_ONLY
inline constexpr const char* StateRoot="/sd/HANIME_S3_TEST/L1_A_REV1_20261111";
#else
inline constexpr const char* StateRoot="/sd/HANIME_S3_TEST/ARCH3_R2_20261107";
#endif
bool allowedStatePath(const std::string&);
// Fixed independent daily root only. Existing files are never copied, replaced,
// truncated, or imported here; StateV3 remains the sole state-file owner.
bool prepareDailyRoot(std::shared_ptr<const ime_device::Mount>);
// One mount instance, one loop task. Short transfers are returned, not retried
// behind the scheduler's back. No native pread/pwrite requirement on FatFs VFS.
class FileOps {
 std::shared_ptr<const ime_device::Mount> mount_;uint64_t epoch_;
 public:
 explicit FileOps(std::shared_ptr<const ime_device::Mount>);
 bool current()const;
 int open(const std::string&,int,int=0600)const;
 int stat(const std::string&,struct stat*)const;
 int size(int,struct stat*)const;
 ssize_t read(int,void*,size_t,off_t)const;
 ssize_t write(int,const void*,size_t,off_t)const;
 int sync(int)const;
 int truncate(int,off_t)const;
 int close(int)const;
 int replace(const std::string&,const std::string&)const;
};
#if R2_HOST_TEST
enum class FaultOp {None,Open,Read,Write,Sync,Close,Replace};
struct Fault {FaultOp op=FaultOp::None;unsigned after=0;bool fired=false;};
extern Fault fault;
#endif
}
