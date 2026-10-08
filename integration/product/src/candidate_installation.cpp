#include "tf_resources.hpp"
#include <sys/stat.h>
#include <cerrno>
namespace ime_device {
namespace {
struct RequiredFile { const char* name;size_t bytes; };
// Exact fourteen-file B2 release unit from the validated manifest. The font is
// separately loaded from the old explicit path. These sizes do not grant trust.
constexpr RequiredFile Required[] = {
 {"full_pinyin.hnm",2080808},
 {"full_pinyin.hnv",68608},
 {"full_pinyin_base.hbo",10102232},
 {"full_pinyin_base.hnv",327680},
 {"shared.hnv",58698752},
 {"wubi86.hnv",50176},
 {"wubi86_base.hbo",7369432},
 {"wubi86_base.hnv",239616},
 {"xiaohe.hnv",91136},
 {"xiaohe_base.hbo",13873768},
 {"xiaohe_base.hnv",449536},
 {"shared.hns",29349067},
 {"wubi86.hnm",1482792},
 {"xiaohe.hnm",2756648},
};
bool regular(const std::string& path, size_t* bytes=nullptr){
 struct stat st{};
 if(::stat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<0)return false;
 if(bytes)*bytes=static_cast<size_t>(st.st_size);
 return true;
}
}
bool Resources::checkInstallation(){
 // No active reader is replaced or retired by this boot-only check.
 if(state_!=ResourceState::Dormant)return false;
 struct stat st{};
 if(::stat((root_+"/STAGING_INCOMPLETE").c_str(),&st)==0||errno!=ENOENT)
  return fail("candidate installation incomplete");
 if(!regular(root_+"/CANDIDATE_READY.json"))return fail("candidate installation marker absent");
 for(const auto& f:Required){
  size_t bytes=0;
  if(!regular(root_+"/"+f.name,&bytes)||bytes!=f.bytes)return fail(f.name,"required set missing/size mismatch");
 }
 // No JSON authorizes resources. Each subsequent bootstrap/read still checks
 // the compiled RC pin; equal-size corruption cannot bypass that chain.
 return true;
}
}
