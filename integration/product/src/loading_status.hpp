#pragma once
#include "chinese_font.hpp"
namespace ime_device {
enum class LoadStage { FontRead, FontIndex, SharedOpen, SharedCheck, TextAdmission, SchemeOpen, SchemeCheck, Ready, Idle, FontError, ResourceError };
struct LoadSnapshot {
 LoadStage stage=LoadStage::Idle;
 uint64_t done=0,total=0;
 unsigned detail=0;
 char text[80]{};
};
// No reads or allocations. Progress is prepared outside draw/layout callbacks.
LoadSnapshot loadSnapshot(const ChineseFont&,const Resources&);
void modeCaption(char*,size_t,bool chinese,hanime_extension::SchemeRules,bool dirty);
inline uint64_t receivedUs(uint64_t nowUs,uint32_t nowMs,uint32_t eventMs){
 const auto age=uint64_t(uint32_t(nowMs-eventMs))*1000;
 return age<=nowUs?nowUs-age:0;
}
class LoadingStatus {
 LoadSnapshot shown_{};uint32_t last_=0;bool seen_=false;
 public:
 static constexpr uint32_t RefreshMs=500;
 bool update(const LoadSnapshot&,uint32_t now);
 const LoadSnapshot& shown()const{return shown_;}
};
}
