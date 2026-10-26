#pragma once
#include "src/hanime_document/types.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
namespace s3app {
// Text-only metadata input. No document, clipboard, history, heap or timer.
class NameInput {
 public:
 static constexpr size_t Capacity=hanime::documents::kMaxNameBytes;
 const char* text()const noexcept{return bytes_.data();}
 size_t size()const noexcept{return size_;}
 size_t cursor()const noexcept{return cursor_;}
 bool selected()const noexcept{return anchor_!=cursor_;}
 size_t selectionBegin()const noexcept{return std::min(size_t(anchor_),cursor_);}
 size_t selectionEnd()const noexcept{return std::max(size_t(anchor_),cursor_);}
 bool selectAll()noexcept{if(!size_||(anchor_==0&&cursor_==size_))return false;anchor_=0;cursor_=size_;++revision_;return true;}
 uint32_t revision()const noexcept{return revision_;}
 void clear()noexcept{bytes_.fill(0);size_=cursor_=anchor_=0;++revision_;}
 bool assign(std::string_view value){clear();return insert(value);}
 bool insert(std::string_view value){
  if(value.size()>Capacity-(size_-(selectionEnd()-selectionBegin()))||!hanime::documents::isValidUtf8(value))return false;
  if(value.empty())return true;
  if(selected())eraseSelection();
  std::memmove(bytes_.data()+cursor_+value.size(),bytes_.data()+cursor_,size_-cursor_+1);
  std::memcpy(bytes_.data()+cursor_,value.data(),value.size());size_+=value.size();cursor_+=value.size();anchor_=uint16_t(cursor_);++revision_;return true;
 }
 bool left(bool extend=false)noexcept{return move(!extend&&selected()?selectionBegin():cursor_?previous(cursor_):0,extend);}
 bool right(bool extend=false)noexcept{return move(!extend&&selected()?selectionEnd():cursor_<size_?next(cursor_):size_,extend);}
 bool home(bool extend=false)noexcept{return move(0,extend);}
 bool end(bool extend=false)noexcept{return move(size_,extend);}
 bool backspace()noexcept{if(selected()){eraseSelection();return true;}if(!cursor_)return false;auto start=previous(cursor_);erase(start,cursor_);cursor_=anchor_=start;return true;}
 bool deleteForward()noexcept{if(selected()){eraseSelection();return true;}if(cursor_==size_)return false;erase(cursor_,next(cursor_));return true;}
 private:
 bool move(size_t to,bool extend)noexcept{const auto anchor=extend?anchor_:uint16_t(to);if(to==cursor_&&anchor==anchor_)return false;cursor_=to;anchor_=anchor;++revision_;return true;}
 void eraseSelection()noexcept{const auto begin=selectionBegin();erase(begin,selectionEnd());cursor_=anchor_=begin;}
 size_t previous(size_t at)const noexcept{do{--at;}while(at&&(static_cast<unsigned char>(bytes_[at])&0xc0)==0x80);return at;}
 size_t next(size_t at)const noexcept{++at;while(at<size_&&(static_cast<unsigned char>(bytes_[at])&0xc0)==0x80)++at;return at;}
 void erase(size_t begin,size_t end)noexcept{std::memmove(bytes_.data()+begin,bytes_.data()+end,size_-end+1);size_-=end-begin;std::fill(bytes_.begin()+size_+1,bytes_.end(),0);++revision_;}
 std::array<char,Capacity+1> bytes_{};
 size_t size_=0,cursor_=0;
 uint32_t revision_=0;
 // Fits the existing modal state padding; no text buffer or allocation grows.
 static_assert(Capacity<=UINT16_MAX);
 uint16_t anchor_=0;
};
}
