#include "loading_status.hpp"
#include <cstdio>
#include <cstring>
namespace ime_device {
namespace {const char* schemeLabel(hanime_extension::SchemeRules r){
 switch(r){case hanime_extension::SchemeRules::FullPinyin:return "PY";case hanime_extension::SchemeRules::R1Xiaohe:return "XH";case hanime_extension::SchemeRules::DirectExact:return "WB";}return "?";}}
void modeCaption(char* out,size_t size,bool chinese,hanime_extension::SchemeRules rules,bool dirty){
 std::snprintf(out,size,"%s%s | %s",chinese?"CN ":"EN",chinese?schemeLabel(rules):"",dirty?"UNSAVED":"SAVED");
}
LoadSnapshot loadSnapshot(const ChineseFont& font,const Resources& r){
 LoadSnapshot p;
 if(font.failed()){p.stage=LoadStage::FontError;std::snprintf(p.text,sizeof(p.text),"Error: %s",font.error());return p;}
 if(!font.ready()){
  if(font.loadedBytes()<ChineseFont::Bytes){p.stage=LoadStage::FontRead;p.done=font.loadedBytes();p.total=ChineseFont::Bytes;std::snprintf(p.text,sizeof(p.text),"Font read/CRC: %llu%%",(unsigned long long)(p.done*100/p.total));}
  else{p.stage=LoadStage::FontIndex;p.done=font.checkedGlyphs();p.total=ChineseFont::Glyphs;std::snprintf(p.text,sizeof(p.text),"Font index: %llu/%llu",(unsigned long long)p.done,(unsigned long long)p.total);}
  return p;
 }
 p.done=r.stageWork();
 switch(r.state()){
 case ResourceState::Dormant:p.stage=LoadStage::Idle;std::snprintf(p.text,sizeof(p.text),"CN idle; Caps Lock to load");break;
 case ResourceState::OpenShared:case ResourceState::OpenSharedIndex:case ResourceState::ActivateShared:
  p.stage=LoadStage::SharedOpen;std::snprintf(p.text,sizeof(p.text),"Opening verified shared resource");break;
 case ResourceState::ValidateShared:p.stage=LoadStage::SharedCheck;std::snprintf(p.text,sizeof(p.text),"Shared bootstrap: %llu work",(unsigned long long)p.done);break;
 case ResourceState::AdmitText:{
  p.stage=LoadStage::TextAdmission;p.done=0;
  std::snprintf(p.text,sizeof(p.text),"Checking published text limits");break;
 }
 case ResourceState::OpenScheme:case ResourceState::OpenSchemeIndex:case ResourceState::ActivateScheme:
  p.stage=LoadStage::SchemeOpen;std::snprintf(p.text,sizeof(p.text),"Opening verified %s scheme",schemeLabel(r.rules()));break;
 case ResourceState::ValidateScheme:p.stage=LoadStage::SchemeCheck;std::snprintf(p.text,sizeof(p.text),"%s bootstrap: %llu work",schemeLabel(r.rules()),(unsigned long long)p.done);break;
 case ResourceState::OpenOrder:case ResourceState::OpenOrderIndex:case ResourceState::ActivateOrder:
  p.stage=LoadStage::SchemeOpen;std::snprintf(p.text,sizeof(p.text),"Opening verified %s order",schemeLabel(r.rules()));break;
 case ResourceState::ValidateOrder:p.stage=LoadStage::SchemeCheck;std::snprintf(p.text,sizeof(p.text),"Order bootstrap: %llu work",(unsigned long long)p.done);break;
 case ResourceState::Ready:
  if(!r.ready()){p.stage=LoadStage::ResourceError;std::snprintf(p.text,sizeof(p.text),"Error: resource health failed; see log");}
  else{p.stage=LoadStage::Ready;std::snprintf(p.text,sizeof(p.text),"CN ready: %s; verified on read",schemeLabel(r.rules()));}
  break;
 case ResourceState::Error:p.stage=LoadStage::ResourceError;std::snprintf(p.text,sizeof(p.text),"Error: %s",r.error().c_str());break;
 }
 return p;
}
bool LoadingStatus::update(const LoadSnapshot& next,uint32_t now){
 const bool transition=!seen_||next.stage!=shown_.stage||next.detail!=shown_.detail;
 if(!transition&&uint32_t(now-last_)<RefreshMs)return false;
 if(!transition&&next.done==shown_.done&&next.total==shown_.total&&std::strcmp(next.text,shown_.text)==0)return false;
 shown_=next;last_=now;seen_=true;return true;
}
}
