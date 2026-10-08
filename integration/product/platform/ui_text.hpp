#pragma once
#include "font_port.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <string_view>
namespace s3app::display {
inline uint32_t scalar(std::string_view text,size_t at){
 const auto n=editor::nextUtf8Boundary(text,at)-at;
 const auto b=static_cast<unsigned char>(text[at]);uint32_t cp=b;
 if(n>1){cp=b&((1u<<(7-n))-1);for(size_t i=1;i<n;++i)cp=(cp<<6)|(static_cast<unsigned char>(text[at+i])&63);}
 return cp;
}
inline int advance(const s3host::FontPort& font,uint32_t cp){return std::max(1,int(font.glyph(cp).advance));}
inline size_t fit(std::string_view text,const s3host::FontPort& font,int width){
 size_t at=0;int used=0;
 while(at<text.size()){int next=advance(font,scalar(text,at));if(used+next>width)break;used+=next;at=editor::nextUtf8Boundary(text,at);}
 return at;
}
inline int measure(std::string_view text,const s3host::FontPort& font){
 int width=0;for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at))width+=advance(font,scalar(text,at));return width;
}
}
