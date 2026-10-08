#include "hid_keyboard.hpp"
#include <algorithm>
namespace s3hid {
const Report* Map::report(uint8_t id)const noexcept{for(unsigned i=0;i<count_;++i)if(reports_[i].id==id&&reports_[i].fields)return &reports_[i];return nullptr;}
bool Map::parse(const uint8_t* b,size_t n) noexcept {
 reports_={};count_=0;if(!b||n>4096)return false;
 struct Global{uint32_t page=0,size=0,count=0,id=0;};Global g;std::array<Global,4> stack{};unsigned sp=0;
 std::array<uint16_t,16> usages{};unsigned un=0;uint32_t lo=0,hi=0;bool range=false,haveLo=false;
 for(size_t p=0;p<n;){
  uint8_t prefix=b[p++];if(prefix==0xfe)return false;
  unsigned len=prefix&3;if(len==3)len=4;if(p+len>n)return false;
  uint32_t v=0;for(unsigned j=0;j<len;++j)v|=uint32_t(b[p++])<<(j*8);
  unsigned type=(prefix>>2)&3,tag=prefix>>4;
  if(type==1){switch(tag){case 0:g.page=v;break;case 7:g.size=v;break;case 8:if(v==0||v>255)return false;g.id=v;break;case 9:g.count=v;break;case 10:if(sp==4)return false;stack[sp++]=g;break;case 11:if(!sp)return false;g=stack[--sp];break;default:break;}}
  else if(type==2){switch(tag){case 0:if(un==16)return false;usages[un++]=v&0xffff;break;case 1:lo=v;haveLo=true;break;case 2:hi=v;range=haveLo;break;default:break;}}
  else if(type==0){
   if(tag==8){
    Report* r=nullptr;for(unsigned j=0;j<count_;++j)if(reports_[j].id==g.id)r=&reports_[j];
    if(!r){if(count_==8)return false;r=&reports_[count_++];r->id=g.id;}
    if(g.size>32||g.count>256||g.size*g.count>4096||r->bits+g.size*g.count>4096)return false;
    if(!(v&1)&&g.page==7){
     if(!g.size||!g.count||r->fields==16||(range&&(hi<lo||hi>255))||(!range&&un==0))return false;
     auto& f=r->field[r->fields++];f.offset=r->bits;f.size=g.size;f.count=g.count;f.variable=v&2;f.min=lo;f.max=hi;f.n=un;f.usages=usages;
     if(f.variable&&((range&&hi-lo+1<g.count)||(!range&&un<g.count)))return false;
    }
    r->bits+=g.size*g.count;
   }
   usages={};un=0;lo=hi=0;range=haveLo=false;
  }
 }
 if(sp) { return false; }
 for(unsigned j=0;j<count_;++j) { if(reports_[j].fields) { return true; } }
 return false;
}
bool Map::decode(const Report& r,const uint8_t* b,size_t n,State& out)noexcept{
 if(!b||n!=(r.bits+7u)/8u) { return false; }
 State next;
 for(unsigned j=0;j<r.fields;++j){auto& f=r.field[j];
  for(unsigned i=0;i<f.count;++i){uint32_t v=0;unsigned at=f.offset+i*f.size;
   for(unsigned k=0;k<f.size;++k)if(b[(at+k)/8]&(1u<<((at+k)%8)))v|=1u<<k;
   unsigned usage=f.variable?(f.n?f.usages[std::min<unsigned>(i,f.n-1)]:f.min+i):v;
   if(!f.variable&&usage>=1&&usage<=3)return false; // ErrorRollOver: invalidate held state.
   if(f.variable&&!v) { continue; }
   if(usage>255) { return false; }
   next.set(usage);
  }
 }out=next;return true;
}
uint32_t uk(uint8_t u,uint8_t m)noexcept{
 bool shift=m&0x22;
 if((m&0x40)&&u==8)return 0x20ac;
 if(m&0x40)return 0;
 if(u>=4&&u<=29)return (shift?'A':'a')+u-4;
 if(u>=30&&u<=39){constexpr uint32_t normal[]={'1','2','3','4','5','6','7','8','9','0'};constexpr uint32_t shifted[]={'!','"',0xa3,'$','%','^','&','*','(',')'};return shift?shifted[u-30]:normal[u-30];}
 switch(u){case 44:return ' ';case 45:return shift?'_':'-';case 46:return shift?'+':'=';case 47:return shift?'{':'[';case 48:return shift?'}':']';case 49:case 50:return shift?'~':'#';case 51:return shift?':':';';case 52:return shift?'@':'\'';case 53:return shift?0xac:'`';case 54:return shift?'<':',';case 55:return shift?'>':'.';case 56:return shift?'?':'/';case 100:return shift?'|':'\\';default:return 0;}
}
static hanime::body_extension::RepeatKey arrow(uint8_t u)noexcept{
 using K=hanime::body_extension::RepeatKey;
 switch(u){case 42:return K::Backspace;case 76:return K::DeleteForward;case 79:return K::Right;case 80:return K::Left;case 81:return K::Down;case 82:return K::Up;default:return K::None;}
}
void Repeat::event(const Event&e)noexcept{
 mods_=e.mods;if(e.usage>=224){if(mods_&0xdd)repeater_.cancel();return;}
 auto key=arrow(e.usage);
 if(!e.down){repeater_.keyUp(key);return;}
 if(!focused_||(mods_&0xdd)){repeater_.cancel();return;}
 if(key!=hanime::body_extension::RepeatKey::None){
  if(repeater_.keyDown(key,e.ms,0)){usage_=e.usage;epoch_=e.epoch;}
 }else repeater_.cancel();
}
bool Repeat::poll(uint32_t now,uint32_t epoch,Event& out)noexcept{
 // Full 64-bit document identity is checked by context(), never truncated.
 auto emission=repeater_.service(now,mods_&0x22?editor::input::ModifierShift:0,{epoch==epoch_,focused_,false,0});
 if(!emission.emitted)return false;
 out={epoch,now,usage_,mods_,true};return true;
}
}
