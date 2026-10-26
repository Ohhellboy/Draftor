#pragma once
#include "menu_font.hpp"
#include "ui_text.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
namespace r2preview {
inline void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct Raster {
 s3app::MenuFont14 auxFont;s3app::MenuFont16 contentFont;
 std::vector<unsigned char> pixels=std::vector<unsigned char>(120000,255);
 std::vector<std::string> labels;unsigned calls=0;
 void fill(int x,int y,int w,int h,bool ink){
  require(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=300&&y+h<=400,"rectangle outside native screen");
  for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)pixels[row*300+col]=ink?0:255;
 }
 void text(std::string_view text,int x,int y,bool inverse,const s3host::FontPort& font){
  require(x>=0&&y>=0&&y+font.lineHeight()<=400,"line box outside native screen");
  labels.emplace_back(text);++calls;
  for(std::size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
   const auto cp=s3app::display::scalar(text,at);const auto g=font.glyph(cp);
   if(g.missing)throw std::runtime_error("secondary menu missing scalar "+std::to_string(cp)+" in "+std::string(text));
   require(x+g.advance<=292,"caption advance outside right margin");
   for(unsigned r=0;r<g.height;++r)for(unsigned c=0;c<g.width;++c)if(g.bits[r*g.stride+c/8]&(0x80>>(c%8))){
    const int px=x+g.bearing+int(c),py=y+int(r);require(px>=0&&px<300&&py>=0&&py<400,"glyph ink outside screen");
    pixels[py*300+px]=inverse?255:0;
   }
   x+=g.advance;
  }
 }
 auto aux(){return [this](std::string_view t,int x,int y,bool inverse){text(t,x,y,inverse,auxFont);};}
 auto content(){return [this](std::string_view t,int x,int y,bool inverse){text(t,x,y,inverse,contentFont);};}
 auto rect(){return [this](int x,int y,int w,int h,bool ink){fill(x,y,w,h,ink);};}
 bool has(std::string_view s)const {for(const auto& t:labels)if(t.find(s)!=std::string::npos)return true;return false;}
 void save(const char* name){
  const char* root=std::getenv("R2_UI_PREVIEW_DIR");if(!root)return;
  std::filesystem::create_directories(root);const auto base=std::filesystem::path(root)/name;
  std::ofstream image(base.string()+".pgm",std::ios::binary);image<<"P5\n300 400\n255\n";image.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
  std::ofstream log(base.string()+".txt");for(const auto& t:labels)log<<t<<'\n';
  require(image.good()&&log.good(),"preview output failed");
 }
};
}
