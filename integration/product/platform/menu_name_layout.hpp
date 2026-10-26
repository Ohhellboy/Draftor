#pragma once
#include "ui_text.hpp"
#include <array>
#include <cstdint>
#include <string_view>
namespace s3app::display {
struct MenuNameLines {
 std::array<uint8_t,98> offsets{}; // line starts, then end sentinel; original bytes
 uint8_t count=0;
 bool relaxed=false;
 bool valid=false;
};
// The Closeout bracket/group reachability algorithm, bounded by the existing
// 96-byte name limit. This adapts display only; no storage validation or rename
// operation consults this result. Missing glyph advances still make progress.
inline MenuNameLines wrapMenuName(std::string_view text,const s3host::FontPort& font,int width=264) noexcept {
 MenuNameLines result;
 if(text.size()>96||width<=0||!editor::isValidUtf8(text))return result;
 result.valid=true;if(text.empty())return result;
 std::array<uint8_t,97> bytes{};
 std::array<uint16_t,97> advances{};
 std::array<uint8_t,96> kind{};
 size_t n=0;
 for(size_t at=0;at<text.size();){
  const auto cp=scalar(text,at);const auto metric=font.glyphAdvance(cp);
  const unsigned w=metric?metric:font.defaultAdvance();
  if(w>unsigned(width)||advances[n]+w>65535){result.valid=false;return result;}
  kind[n]=cp=='('?1:cp==')'?2:cp==0xff08?3:cp==0xff09?4:0;
  at=editor::nextUtf8Boundary(text,at);++n;bytes[n]=uint8_t(at);advances[n]=uint16_t(advances[n-1]+w);
 }
 std::array<uint8_t,13> banned{};
 auto ban=[&](size_t i){banned[i/8]|=uint8_t(1U<<(i%8));};
 auto allowed=[&](size_t i){return !(banned[i/8]&(1U<<(i%8)));};
 auto opening=[](uint8_t k){return k==1||k==3;};
 auto closing=[](uint8_t k){return k==2||k==4;};
 for(size_t i=1;i<n;++i)if(opening(kind[i-1])||closing(kind[i]))ban(i);
 struct Group {uint8_t first,last;};
 std::array<uint8_t,96> stack{};size_t depth=0;
 std::array<Group,48> groups{};size_t groupCount=0;
 for(size_t i=0;i<n;++i){
  if(opening(kind[i]))stack[depth++]=uint8_t(i);
  else if(closing(kind[i])&&depth){
   const auto a=stack[depth-1];
   if(kind[i]==kind[a]+1){--depth;groups[groupCount++]={a,uint8_t(i+1)};}
   else depth=0;
  }
 }
 auto protect=[&](size_t a,size_t z){for(size_t i=a+1;i<z;++i)ban(i);};
 for(size_t i=0;i<groupCount;++i){const auto g=groups[i];if(advances[g.last]-advances[g.first]<=width)protect(g.first,g.last);}
 size_t tail=n;bool found=true;
 while(found){
  found=false;
  for(size_t i=0;i<groupCount;++i){const auto g=groups[i];if(g.last!=tail)continue;
   const auto inside=text.substr(bytes[g.first+1],bytes[g.last-1]-bytes[g.first+1]);
   bool number=!inside.empty();for(unsigned char c:inside)number=number&&c>='0'&&c<='9';
   if(inside=="恢复"||number){tail=g.first;found=true;break;}
  }
 }
 if(tail<n&&advances[n]-advances[tail]<=width)protect(tail,n);
 // next==255 denotes unreachable. Favor the furthest reachable legal break,
 // exactly as the reviewed Closeout implementation does.
 std::array<uint8_t,97> next{};next.fill(255);next[n]=uint8_t(n);
 for(size_t i=n;i-->0;)for(size_t j=i+1;j<=n&&advances[j]-advances[i]<=width;++j)
  if(allowed(j)&&next[j]!=255)next[i]=uint8_t(j);
 if(next[0]!=255){
  size_t i=0;
  while(i<n){i=next[i];result.offsets[++result.count]=bytes[i];}
  if(result.count<=11)return result;
 }
 // The fixed viewport and complete real name take priority over punctuation.
 // This also handles 32 unmatched fullwidth closers without altering bytes.
 result.relaxed=true;result.count=0;result.offsets.fill(0);
 for(size_t i=0;i<n;){
  size_t j=i+1;while(j<n&&advances[j+1]-advances[i]<=width)++j;
  result.offsets[++result.count]=bytes[j];i=j;
 }
 return result;
}
}
