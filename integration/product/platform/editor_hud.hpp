#pragma once
#include "metadata_font.hpp"
#include "editor_geometry.hpp"
#include "vertical_hud.hpp"
#include "ble_keyboard_host.hpp"
#include "loading_status.hpp"
#include "device_ime.hpp"
#include <cstdio>

namespace s3app::editor_hud {
// UI-only, borrowed snapshot. No storage calls, ownership or state transitions.
struct State {
 bool chinese=false,dirty=false,saving=false,savePending=false,imeActive=false,rawEdit=false,vertical=false;
 hanime_extension::SchemeRules scheme=hanime_extension::SchemeRules::R1Xiaohe;
 keyboard::Status keyboard{};
 ime_device::LoadStage load=ime_device::LoadStage::Idle;
 hanime::CompositionStatus submission{};
 std::string_view notice,imeNotice;
};
inline State state(const App& app,const ime_device::DeviceIme* ime,
                  hanime_extension::SchemeRules scheme,keyboard::Status ble,
                  const ime_device::LoadSnapshot& load) noexcept {
 State s;s.chinese=ime&&ime->chinese();s.dirty=app.sharedState().document.dirty();
 s.saving=app.persistence.backgroundSaveActive();s.savePending=app.manualSavePending();
 s.imeActive=ime&&ime->active();s.rawEdit=ime&&ime->rawEditing();s.scheme=scheme;
 s.vertical=!app.horizontalActive();
 s.keyboard=ble;s.load=load.stage;s.notice=app.notice;
 if(ime){s.imeNotice=ime->notice();s.submission=ime->receipt().status;}
 return s;
}
inline const char* mode(bool chinese,hanime_extension::SchemeRules scheme) noexcept {
 if(!chinese)return "En";
 switch(scheme){case hanime_extension::SchemeRules::R1Xiaohe:return "双";
 case hanime_extension::SchemeRules::FullPinyin:return "全";
 case hanime_extension::SchemeRules::DirectExact:return "五";}return "?";
}
enum class Bluetooth {Disconnected,Pairing,Connected,Off};
inline Bluetooth bluetooth(keyboard::Phase phase) noexcept {
 switch(phase){case keyboard::Phase::Ready:return Bluetooth::Connected;
 case keyboard::Phase::Pairing:return Bluetooth::Pairing;
 default:return Bluetooth::Disconnected;}
}
inline const char* bluetoothText(Bluetooth value) noexcept {
 switch(value){case Bluetooth::Connected:return "已连接";case Bluetooth::Pairing:return "配对中";
 case Bluetooth::Off:return "已关闭";default:return "未连接";}
 // No actual Phase maps to Off: the existing product has no off state.
}
// MetadataFont uses immutable small Latin advances and fixed 14px UI14 cells.
// This metric-only adapter avoids ui14 bitmap unpacking for width queries.
class Metrics final:public s3host::FontPort {
 s3host::BuiltinSmallFont latin_;
 public:
 s3host::GlyphView glyph(uint32_t cp)const noexcept override {
  const auto g=latin_.glyph(cp);
  return {nullptr,0,0,0,uint16_t(g.missing?14:g.advance),0,false};
 }
 uint16_t lineHeight()const noexcept override{return 20;}
 uint16_t defaultAdvance()const noexcept override{return 14;}
};
inline unsigned width(std::string_view text) noexcept {
 const Metrics face;unsigned result=0;
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))
  result+=face.glyph(display::scalar(text,at)).advance;
 return result;
}
template<class Pixel>
void text(const MetadataFont& face,std::string_view value,int x,int y,Pixel pixel,int right=294){
 for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){
  const auto g=face.glyph(display::scalar(value,at));if(x+g.advance>right)break;
  for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col)
   if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))pixel(x+g.bearing+int(col),y+int(row));
  x+=g.advance;
 }
}
template<class Pixel>
void title(std::string_view name,bool vertical,const MetadataFont& face,Pixel pixel){
 if(!vertical){text(face,name,8,editor_geometry::horizontalTitleY,pixel);return;}
 // Keep the original vertical bounded ellipsis/alignment, now name-only.
 const Metrics metrics;VerticalHudLine line;hudAppend(line,name,metrics);
 text(face,line.text,292-int(line.width),4,pixel);
}
inline const char* storageWarning(std::string_view value) noexcept {
 if(value=="Save failed; command cancelled")return "保存失败，命令取消";
 if(value=="IO_ERROR")return "存储读写失败";
 if(value=="MEDIA_UNAVAILABLE")return "存储不可用";
 if(value=="SESSION_CHANGED")return "存储已变化";
 if(value=="CORRUPT")return "文稿损坏";
 if(value=="PARTIAL"||value=="OK_DEGRADED")return "存储异常";
 if(value=="Memory low; operation failed"||value=="RESOURCE_EXHAUSTED"||value=="INSTALL_FAILED")return "内存不足，操作失败";
 return nullptr;
}
inline bool routine(std::string_view value) noexcept {
 return value.empty()||value=="OK"||value=="Saved"||value=="Unsaved changes"||
  value=="Snapshot saved; newer edits"||value=="Starting"||value=="Cancelled"||
  value=="Command cancelled"||value=="Saving snapshot..."||value=="Waiting for save"||
  value=="English only - IME reserved"||value=="Rename cancelled";
}
inline bool ordinaryCandidate(std::string_view value) noexcept {
 return value.empty()||value=="CN ready"||value=="Candidates"||value=="Candidate ready"||value=="Candidates ready"||
  value=="Full code candidates ready"||value=="Raw buffered"||value=="Boundary";
}
inline std::string_view feedback(const State& s) noexcept {
 if(s.load==ime_device::LoadStage::FontError||s.load==ime_device::LoadStage::ResourceError)
  return "资源错误，请查看日志";
 if(s.keyboard.phase==keyboard::Phase::Error)return "蓝牙故障，请查看日志";
 if(s.keyboard.phase==keyboard::Phase::AwaitRelease)return "松开按键，再轻按 Shift";
 if(s.keyboard.phase==keyboard::Phase::Ready&&s.keyboard.pairing_not_saved)return "已连接，配对未保存";
 if(s.imeActive&&s.submission==hanime::CompositionStatus::UnknownOutcome)return "提交结果待确认，请勿重复提交";
 if(s.imeActive&&s.submission==hanime::CompositionStatus::RetryableNoEffect)return "提交未生效：空格重试，Esc编辑";
 if(s.load==ime_device::LoadStage::FontRead||s.load==ime_device::LoadStage::FontIndex)return "正在加载字体";
 if(s.load!=ime_device::LoadStage::Idle&&s.load!=ime_device::LoadStage::Ready)return "正在准备中文输入";
 if(s.imeActive){
  if(s.imeNotice=="Raw edit: Down loads marked range"||s.imeNotice=="Raw syllable editor"||
     s.imeNotice=="Raw code editor"||s.imeNotice.substr(0,13)=="Raw syllables"||
     (s.rawEdit&&ordinaryCandidate(s.imeNotice)))return s.vertical?"上下移动，左键载入，Esc返回":"左右移动，下键载入，Esc返回";
  if(s.imeNotice=="All selected; Space/Enter to commit")return "空格或回车提交";
  if(!ordinaryCandidate(s.imeNotice))return s.imeNotice;
 }
 if(!routine(s.notice)&&!storageWarning(s.notice))return s.notice;
 return {};
}
inline const char* saveText(const State& s) noexcept {
 if(const auto* warning=storageWarning(s.notice))return warning;
 if(s.saving)return "保存中…";
 if(s.savePending)return "等待保存";
 return s.dirty?"未保存":"已保存";
}
template<class Fill,class Pixel>
[[gnu::noinline]] void footer(const State& s,const MetadataFont& face,Fill fill,Pixel pixel){
 // This row was already reserved in both orientations. Clear the full row,
 // including the old long caption, without touching body/IME/Find geometry.
 fill(0,380,300,20);
 if(s.keyboard.phase==keyboard::Phase::Pairing&&s.keyboard.passkey<1000000){
  char pin[96];const auto* warning=storageWarning(s.notice);
  if(warning)std::snprintf(pin,sizeof(pin),"%s；输入 %06lu 后回车",warning,(unsigned long)s.keyboard.passkey);
  else std::snprintf(pin,sizeof(pin),"输入 %06lu 后回车",(unsigned long)s.keyboard.passkey);
  text(face,pin,8,383,pixel);return;
 }
 const auto message=feedback(s);
 if(!message.empty()&&!storageWarning(s.notice)){
  const Metrics metrics;VerticalHudLine line;hudAppend(line,message,metrics);
  text(face,line.text,8,383,pixel);return;
 }
 const auto* left=mode(s.chinese,s.scheme);
 const auto* ble=s.keyboard.phase==keyboard::Phase::Error?"蓝牙故障":bluetoothText(bluetooth(s.keyboard.phase));
 const auto* save=saveText(s);const int bleX=292-int(width(ble));
 const int saveX=bleX-12-int(width(save));
 text(face,left,8,383,pixel);text(face,save,saveX,383,pixel);text(face,ble,bleX,383,pixel);
}
} // namespace s3app::editor_hud
