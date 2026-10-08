#pragma once
#include "font_port.hpp"
#include <algorithm>
#include <array>
#include <string_view>
namespace ime_device {
struct RawViewport {
 std::size_t begin=0,end=0;unsigned cursor_x=0,width=0;
 // One transient map shared by glyphs, caret, underline and clipping. Raw offsets
 // still name letters; a visual gap never becomes an editable character.
 std::array<unsigned,65> x{};
 unsigned at(std::size_t offset)const{return x[std::min(offset,std::size_t(64))]-x[begin];}
};
inline RawViewport rawViewport(std::string_view raw,std::size_t cursor,bool editing,
                               const s3host::FontPort& font,unsigned pixels=284,
                               const uint16_t* boundaries=nullptr,std::size_t boundary_count=0){
 const auto size=std::min(raw.size(),std::size_t(64));cursor=std::min(cursor,size);
 RawViewport v;const auto gap=(s3host::FontPort::advance(&font,' ')+1)/2;
 std::size_t boundary=0;
 for(std::size_t i=0;i<size;++i){
  while(boundary<boundary_count&&boundaries[boundary]<i)++boundary;
  if(i&&boundary<boundary_count&&boundaries[boundary]==i)v.x[i]+=gap;
  v.x[i+1]=v.x[i]+s3host::FontPort::advance(&font,uint8_t(raw[i]));
 }
 const auto limit=pixels?pixels-1:0;
 while(v.begin<size&&v.x[size]-v.x[v.begin]>limit)++v.begin;
 if(editing){
  v.begin=0;const auto context=cursor<size&&limit>28?limit-28:limit;
  while(v.begin<cursor&&v.x[cursor]-v.x[v.begin]>context)++v.begin;
 }
 v.end=v.begin;
 while(v.end<size&&v.x[v.end+1]-v.x[v.begin]<=limit)++v.end;
 v.cursor_x=editing?v.at(cursor):0;v.width=v.at(v.end);return v;
}
enum class BoundaryEdge:uint8_t {CandidateTop,CandidateBottom,CandidateLeft,CandidateRight,RawLeft,RawRight};
struct BoundaryRect {int x,y,width,height;};
inline BoundaryRect boundaryRect(BoundaryEdge edge,int y,std::size_t rows){
 const auto height=int(std::max(std::size_t(1),std::min(rows,std::size_t(3))))*34;
 switch(edge){
 case BoundaryEdge::CandidateBottom:return {8,y+48+height-4,284,2};
 case BoundaryEdge::CandidateLeft:return {6,y+48,2,height-4};
 case BoundaryEdge::CandidateRight:return {292,y+48,2,height-4};
 case BoundaryEdge::RawLeft:return {6,y+30,2,14};
 case BoundaryEdge::RawRight:return {292,y+30,2,14};
 default:return {8,y+46,284,2};
 }
}
}
