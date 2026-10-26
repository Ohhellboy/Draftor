#pragma once
#include "ui_text.hpp"
#include <array>
#include <cstring>
namespace s3app::display {
struct MenuNamePreview {
 char bytes[100]{};uint8_t size=0;
 bool truncated=false,disambiguated=false;
 std::string_view text()const noexcept{return {bytes,size};}
 bool append(std::string_view value)noexcept{
  if(size+value.size()>=sizeof(bytes))return false;
  std::memcpy(bytes+size,value.data(),value.size());size=uint8_t(size+value.size());bytes[size]=0;return true;
 }
};
namespace menu_preview_detail {
struct Parts {
 std::array<uint8_t,97> bytes{};std::array<uint16_t,97> width{};uint8_t count=0;
 explicit Parts(std::string_view name,const s3host::FontPort& font)noexcept{
  for(size_t at=0;at<name.size()&&count<96;){
   const auto w=advance(font,scalar(name,at));at=editor::nextUtf8Boundary(name,at);++count;
   bytes[count]=uint8_t(at);width[count]=uint16_t(width[count-1]+w);
  }
 }
};
inline size_t tailStart(std::string_view name)noexcept{return name.find("（");}
inline MenuNamePreview middle(std::string_view name,const Parts& p,int dotsWidth,int width,size_t minTail)noexcept{
 MenuNamePreview best;int score=-1;size_t bestHead=0,bestTail=0;
 for(size_t head=1;head<p.count;++head)for(size_t tail=std::max<size_t>(1,minTail);head+tail<p.count;++tail){
  if(p.width[head]+dotsWidth+p.width[p.count]-p.width[p.count-tail]>width)continue;
  if(p.bytes[head]+3+name.size()-p.bytes[p.count-tail]>=100)continue;
  const int delta=int(head*2)-int(tail*3);const int value=int(head+tail)*100-(delta<0?-delta:delta);
  if(value>score){score=value;bestHead=head;bestTail=tail;}
 }
 if(score>=0){best.append(name.substr(0,p.bytes[bestHead]));best.append("…");best.append(name.substr(p.bytes[p.count-bestTail]));best.truncated=true;}
 return best;
}
// The caller validates each bounded name and measures it once for both its
// base preview and any disambiguation. No page or cross-frame state is kept.
inline MenuNamePreview base(std::string_view name,const Parts& p,const s3host::FontPort& font,
                            int dotsWidth,int width)noexcept{
 MenuNamePreview out;if(p.width[p.count]<=width){out.append(name);return out;}
 const auto suffix=tailStart(name);
 if(suffix!=name.npos){
  size_t tail=0;for(size_t i=0;i<=p.count;++i)if(p.bytes[i]==suffix){tail=p.count-i;break;}
  if(tail&&p.width[p.count]-p.width[p.count-tail]+dotsWidth+font.defaultAdvance()<=width)
   return middle(name,p,dotsWidth,width,tail);
  return middle(name,p,dotsWidth,width,0);
 }
 size_t head=0;while(head<p.count&&p.width[head+1]+dotsWidth<=width)++head;
 out.append(name.substr(0,p.bytes[head]));out.append("…");out.truncated=true;return out;
}
inline MenuNamePreview distinguish(std::string_view name,const Parts& p,size_t common,
                                   int dotsWidth,int width)noexcept{
 MenuNamePreview best;if(common>=p.count)return best;
 const auto suffix=tailStart(name);
 size_t tail=std::min<size_t>(5,p.count-common-1);
 if(suffix!=name.npos)for(size_t i=0;i<=p.count;++i)if(p.bytes[i]==suffix){tail=p.count-i;break;}
 // If the entire true suffix is too wide, retain a real bounded tail instead.
 if(tail>=p.count-common)tail=std::min<size_t>(5,p.count-common-1);
 for(size_t head=1;head<=common;++head){
  const bool gap=tail&&common+1<p.count-tail;
  const int used=p.width[head]+dotsWidth+(p.width[common+1]-p.width[common])+
    (gap?dotsWidth:0)+(tail?p.width[p.count]-p.width[p.count-tail]:0);
  if(used>width)continue;
  MenuNamePreview candidate;
  if(!candidate.append(name.substr(0,p.bytes[head]))||!candidate.append("…")||
     !candidate.append(name.substr(p.bytes[common],p.bytes[common+1]-p.bytes[common])))continue;
  if(gap&&!candidate.append("…"))continue;
  if(tail&&!candidate.append(name.substr(p.bytes[p.count-tail])))continue;
  candidate.truncated=true;candidate.disambiguated=true;best=candidate;
 }
 return best;
}
}
// Items are the existing cached page. Compare previews only; never alter names,
// IDs, sort order or the catalog. Duplicate real names need no synthetic ID.
template<class Items>
MenuNamePreview menuNamePreview(const Items& items,size_t index,const s3host::FontPort& font,int width=264)noexcept{
 if(index>=items.size())return {};
 const std::string_view name=items[index].name;
 if(name.size()>96||!editor::isValidUtf8(name))return {};
 const menu_preview_detail::Parts parts(name,font);
 if(parts.width[parts.count]<=width){MenuNamePreview full;full.append(name);return full;}
 const int dotsWidth=measure("…",font);
 auto result=menu_preview_detail::base(name,parts,font,dotsWidth,width);const auto original=result;
 if(!result.truncated)return result;
 for(size_t j=0;j<items.size();++j){
  const std::string_view other=items[j].name;
  if(j==index||other==name||other.size()>96||!editor::isValidUtf8(other))continue;
  const menu_preview_detail::Parts peer(other,font);
  if(menu_preview_detail::base(other,peer,font,dotsWidth,width).text()!=original.text())continue;
  size_t common=0;
  while(common<parts.count&&common<peer.count&&
   name.substr(parts.bytes[common],parts.bytes[common+1]-parts.bytes[common])==
   other.substr(peer.bytes[common],peer.bytes[common+1]-peer.bytes[common]))++common;
  auto distinct=menu_preview_detail::distinguish(name,parts,common,dotsWidth,width);
  if(distinct.size)result=distinct;
 }
 return result;
}
}
