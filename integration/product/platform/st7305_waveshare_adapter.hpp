#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
// The product's logical color enum is stable; no ST7306 wire encoding is used.
#include "st7306_osptek_lpm8_adapter.hpp"
namespace st7305_waveshare {
using Color=st7306_osptek_lpm8::Color;
inline constexpr int Width=300,Height=400;
inline constexpr unsigned NativeRows=Height/2, RowBytes=Width/4;
inline constexpr std::size_t BufferSize=NativeRows*RowBytes;
inline constexpr std::uint8_t ColumnFirst=0x12,ColumnLast=ColumnFirst+Width/12-1;
struct PackedPixel {std::size_t index;unsigned bit;};
// Waveshare portrait mapping, MADCTL=0x48, INVON=0x21: white=1, black=0.
// One byte holds four columns x two rows. No stride or transmitted tail padding.
constexpr PackedPixel packedPixel(int x,int y){return {std::size_t(y/2)*RowBytes+unsigned(x/4),unsigned(7-2*(x%4)-(y%2))};}
constexpr bool white(Color c){return c==Color::White||c==Color::Off;}
inline void pixel(std::uint8_t* fb,int x,int y,Color c){
 if(x<0||y<0||x>=Width||y>=Height)return;
 const auto p=packedPixel(x,y);const auto mask=std::uint8_t(1U<<p.bit);
 fb[p.index]=white(c)?std::uint8_t(fb[p.index]|mask):std::uint8_t(fb[p.index]&~mask);
}
inline bool isWhite(const std::uint8_t* fb,int x,int y){const auto p=packedPixel(x,y);return (fb[p.index]>>p.bit)&1U;}
inline void fill(std::uint8_t* fb,Color c){std::memset(fb,white(c)?255:0,BufferSize);}
inline constexpr unsigned MaxWindows=8,WindowBytes=7;
struct Band {std::uint8_t first=0,last=0;};
struct Plan {Band bands[MaxWindows]{};unsigned count=0,payload=0,compared=0;bool full=false;};
inline Plan fullPlan(){Plan p;p.bands[0]={0,NativeRows-1};p.count=1;p.payload=BufferSize;p.full=true;return p;}
inline Plan plan(const std::uint8_t* current,const std::uint8_t* shadow){
 if(!shadow)return fullPlan();
 Plan p;
 for(unsigned row=0;row<NativeRows;++row){
  p.compared+=RowBytes;
  if(!std::memcmp(current+row*RowBytes,shadow+row*RowBytes,RowBytes))continue;
  if(p.count&&p.bands[p.count-1].last+1U==row){p.bands[p.count-1].last=row;continue;}
  // Bounded coalescing: merge the shortest unchanged gap when all slots are used.
  if(p.count==MaxWindows){
   unsigned merge=p.count-1,gap=row-p.bands[merge].last-1;
   for(unsigned j=0;j+1<p.count;++j){const unsigned g=p.bands[j+1].first-p.bands[j].last-1;if(g<gap){gap=g;merge=j;}}
   if(merge==p.count-1){p.bands[merge].last=row;continue;}
   p.bands[merge].last=p.bands[merge+1].last;
   for(unsigned j=merge+1;j+1<p.count;++j)p.bands[j]=p.bands[j+1];
   --p.count;
  }
  p.bands[p.count++]={std::uint8_t(row),std::uint8_t(row)};
 }
 for(unsigned i=0;i<p.count;++i)p.payload+=(p.bands[i].last-p.bands[i].first+1U)*RowBytes;
 if(p.payload+p.count*WindowBytes>=BufferSize+WindowBytes){auto f=fullPlan();f.compared=p.compared;return f;}
 return p;
}
static_assert(BufferSize==15000&&ColumnLast==0x2A);
}
