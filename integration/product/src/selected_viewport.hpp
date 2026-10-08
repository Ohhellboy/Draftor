#pragma once
#include "hanime_extension/extension.hpp"
#include "font_port.hpp"
#include "ui_text.hpp"
#include "utf8.hpp"
#include <algorithm>
namespace ime_device {
struct SelectedView {size_t begin=0,end=0;bool left=false,right=false;};
// Byte-aligned viewport over already materialized member text. No ownership of
// text, glyphs or identities, no allocation, no TF access and no dirty timer.
class SelectedViewport {
 size_t begin_=0;
 public:
 void reset(){begin_=0;}
 SelectedView update(const hanime_extension::Snapshot& s,size_t cursor,bool editing,
                     const s3host::FontPort& font,unsigned pixels=284){
  const auto text=s.selected_text;
  if(text.empty()||!pixels){reset();return {};}
  auto width=[&](size_t a,size_t b){unsigned n=0;for(;a<b;a=editor::nextUtf8Boundary(text,a))n+=s3host::FontPort::advance(&font,s3app::display::scalar(text,a));return n;};
  size_t target_begin=text.size(),target_end=text.size();
  if(editing){
   for(size_t i=0;i<s.member_count;++i){const auto& m=s.members[i];
    if(cursor>=m.begin&&cursor<m.end){target_begin=m.text_begin;target_end=m.text_begin+m.text_size;
     // A wide member has no per-character alignment contract. Proportional
     // navigation only picks a display neighborhood within this owned member;
     // it is never a claimed syllable-to-character mapping or selection range.
     if(width(target_begin,target_end)>pixels){
      size_t scalars=0;for(size_t a=target_begin;a<target_end;a=editor::nextUtf8Boundary(text,a))++scalars;
      const auto index=std::min(scalars?scalars-1:0,(cursor-m.begin)*scalars/std::max(size_t(1),m.end-m.begin));
      for(size_t n=0;n<index;++n)target_begin=editor::nextUtf8Boundary(text,target_begin);
      target_end=editor::nextUtf8Boundary(text,target_begin);
     }
     break;
    }
   }
  }
  // Fixed passes over the existing bounded text pool, without a pixel array.
  unsigned total=0,old_x=0,target_x=0,target_right=0;
  begin_=std::min(begin_,text.size());
  for(size_t a=0;a<text.size();a=editor::nextUtf8Boundary(text,a)){
   const auto advance=s3host::FontPort::advance(&font,s3app::display::scalar(text,a));
   if(a<begin_)old_x=total+advance;
   if(a<target_begin)target_x=total+advance;
   if(a<target_end)target_right=total+advance;
   total+=advance;
  }
  unsigned desired=total<=pixels?0:std::min(old_x,total-pixels);
  if(target_x<desired)desired=target_x;
  if(target_right>desired+pixels)desired=target_right-pixels;
  begin_=0;unsigned skipped=0;
  while(begin_<text.size()&&skipped<desired){const auto next=editor::nextUtf8Boundary(text,begin_);skipped+=width(begin_,next);begin_=next;}
  size_t end=begin_;unsigned used=0;
  while(end<text.size()){
   const auto next=editor::nextUtf8Boundary(text,end);const auto advance=width(end,next);
   if(used+advance>pixels)break;
   used+=advance;end=next;
  }
  return {begin_,end,begin_!=0,end<text.size()};
 }
};
}
