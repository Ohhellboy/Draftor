#include "menu_name_layout.hpp"
#include "menu_font.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <iostream>
#include <vector>
using namespace s3app::display;
unsigned total=0,relaxed=0;
struct WideTestFont final:s3host::FontPort {
 s3host::GlyphView glyph(uint32_t cp)const noexcept override{return {nullptr,0,0,0,uint16_t(cp=='W'?50:100),0,false};}
 uint16_t lineHeight()const noexcept override{return 28;}
 uint16_t defaultAdvance()const noexcept override{return 100;}
};
void check(std::string_view name,const s3host::FontPort& font,bool expectRelax=false){
 const auto out=wrapMenuName(name,font);assert(out.valid&&out.count&&out.count<=11);assert(out.offsets[0]==0&&out.offsets[out.count]==name.size());
 if(expectRelax)assert(out.relaxed);relaxed+=out.relaxed;++total;
 std::string full;
 for(unsigned i=0;i<out.count;++i){assert(out.offsets[i]<out.offsets[i+1]);auto line=name.substr(out.offsets[i],out.offsets[i+1]-out.offsets[i]);assert(editor::isValidUtf8(line));assert(measure(line,font)<=264);full+=line;
  if(!out.relaxed){const auto last=editor::previousUtf8Boundary(line,line.size());assert(i==0||(scalar(line,0)!=')'&&scalar(line,0)!=0xff09));assert(i+1==out.count||(scalar(line,last)!='('&&scalar(line,last)!=0xff08));}
 }
 assert(full==name);
 // Same inputs have byte-identical offsets; no state, timer or mutation.
 const auto again=wrapMenuName(name,font);assert(out.offsets==again.offsets&&out.count==again.count&&out.relaxed==again.relaxed);
}
int main(int argc,char** argv){
 s3app::MenuFont16 font;
 if(argc>1&&std::string(argv[1])=="--compare"){
  std::string name;while(std::getline(std::cin,name)){auto out=wrapMenuName(name,font);assert(out.valid);std::cout<<unsigned(out.count);for(unsigned i=0;i<=out.count;++i)std::cout<<","<<unsigned(out.offsets[i]);std::cout<<"\n";}return 0;
 }
 for(const auto* s:{"文章","文章（恢复）（1）","城市公共空间与居民日常生活研究甲组访谈记录修订版","关于城市公共空间与日常写作的田野调查记录（恢复）（1）","Research_Methodology_2026_Chapter_Three_Final_Revision","文😀𨿍","a（）b","a((x))b","（x)y）"})check(s,font);
 check(std::string(96,'W'),font);
 std::string han,mixed,suffix,closers;for(int i=0;i<32;++i){han+="汉";closers+="）";}for(int i=0;i<24;++i)mixed+="W汉";for(int i=0;i<25;++i)suffix+="城";suffix+="WW（恢复）（1）";
 check(han,font);check(mixed,font);check(suffix,font);check(closers,font,true);
 check(std::string(96,')'),font,true);check(std::string(96,'('),font,true);
 // A synthetic larger font makes the strict >11-line escape reachable. It
 // cannot claim the real MiSans viewport bound; it verifies policy and bytes.
 WideTestFont wide;std::string forced;for(int i=0;i<12;++i)forced+="(W)";
 const auto forcedResult=wrapMenuName(forced,wide);assert(forcedResult.valid&&forcedResult.relaxed&&forcedResult.count==12&&forcedResult.offsets[12]==forced.size());
 for(int a=0;a<16;++a)for(int b=1;b<24;++b){auto s=std::string(a,'W')+"（"+std::string(b,'W')+"）2026（1）";if(s.size()<=96)check(s,font);}
 // Legal punctuation-heavy name preserving complete grouped content.
 std::string heavy;for(int i=0;i<12;++i)heavy+="(WWWW)";check(heavy,font);
 // Deterministic bounded generated corpus includes unmatched, nested, mixed
 // bracket styles, recovered suffixes, rare missing scalars and combining marks.
 const char* parts[]={"W","汉","（","）","(",")","（恢复）","(12)","a\xcc\x81","😀"};
 uint32_t seed=20261023;
 for(unsigned trial=0;trial<2000;++trial){std::string s;for(unsigned i=0;i<70;++i){seed=seed*1664525U+1013904223U;std::string p=parts[seed%10];if(s.size()+p.size()>96)break;s+=p;}check(s,font);}
 assert(!wrapMenuName(std::string(97,'W'),font).valid);assert(!wrapMenuName(std::string("\xe4\xb8",2),font).valid);assert(!wrapMenuName("W",font,1).valid);assert(wrapMenuName("",font).valid);
 std::printf("{\"result\":\"PASS\",\"names\":%u,\"relaxed_names\":%u,\"complete_bytes\":true,\"max_lines\":11,\"missing_glyph_names_permitted\":true}\n",total,relaxed);
}
