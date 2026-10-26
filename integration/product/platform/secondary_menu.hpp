#pragma once
#include "ui_text.hpp"
#include <cstring>
#include <cstdlib>

// Drawing-only helpers. No persistent state, allocation, or controller calls.
namespace s3app::display::secondary {
template<class Fill,class Text> void header(const char* title,Fill fill,Text aux){
 fill(0,0,300,400,false);fill(0,0,300,28,true);aux(title,8,2,true);
}
template<class Text> int wrapped(std::string_view text,const s3host::FontPort& font,
                                int x,int y,int width,int pitch,int bottom,Text draw){
 while(!text.empty()&&y+pitch<=bottom){
  const auto n=fit(text,font,width);if(!n)break;
  draw(text.substr(0,n),x,y,false);text.remove_prefix(n);y+=pitch;
 }
 return y;
}
template<class Fill> void triangle(int y,Fill fill){
 for(int dy=-5;dy<=5;++dy)fill(10,y+14+dy,6-std::abs(dy),1,true);
}
// Display-only middle ellipsis; the original external TXT name is untouched.
// Preserve the real tail, including its extension, without allocating a string.
template<class Text> void preview(std::string_view name,const s3host::FontPort& font,
                                 int x,int y,int width,Text draw){
 if(measure(name,font)<=width){draw(name,x,y,false);return;}
 constexpr std::string_view dots="…";
 const int budget=width-measure(dots,font);
 std::size_t tail=name.size();int tailWidth=0;
 while(tail){const auto prev=editor::previousUtf8Boundary(name,tail);
  const int w=advance(font,scalar(name,prev));if(tailWidth+w>budget/2)break;
  tailWidth+=w;tail=prev;
 }
 const auto head=fit(name.substr(0,tail),font,budget-tailWidth);
 draw(name.substr(0,head),x,y,false);x+=measure(name.substr(0,head),font);
 draw(dots,x,y,false);x+=measure(dots,font);draw(name.substr(tail),x,y,false);
}
}
