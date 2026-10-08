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
 bool selected()const noexcept{return selected_;}
 bool selectAll()noexcept{if(!size_||selected_)return false;selected_=true;cursor_=size_;++revision_;return true;}
 uint32_t revision()const noexcept{return revision_;}
 void clear()noexcept{bytes_.fill(0);size_=cursor_=0;selected_=false;++revision_;}
 bool assign(std::string_view value){clear();return insert(value);}
 bool insert(std::string_view value){
  if(value.size()>Capacity-(selected_?0:size_)||!hanime::documents::isValidUtf8(value))return false;
  if(value.empty())return true;
  if(selected_){erase(0,size_);cursor_=0;selected_=false;}
  std::memmove(bytes_.data()+cursor_+value.size(),bytes_.data()+cursor_,size_-cursor_+1);
  std::memcpy(bytes_.data()+cursor_,value.data(),value.size());size_+=value.size();cursor_+=value.size();++revision_;return true;
 }
 bool left()noexcept{if(selected_){selected_=false;cursor_=0;++revision_;return true;}if(!cursor_)return false;cursor_=previous(cursor_);++revision_;return true;}
 bool right()noexcept{if(selected_){selected_=false;cursor_=size_;++revision_;return true;}if(cursor_==size_)return false;cursor_=next(cursor_);++revision_;return true;}
 bool backspace()noexcept{if(selected_){clear();return true;}if(!cursor_)return false;auto start=previous(cursor_);erase(start,cursor_);cursor_=start;return true;}
 bool deleteForward()noexcept{if(selected_){clear();return true;}if(cursor_==size_)return false;erase(cursor_,next(cursor_));return true;}
 private:
 size_t previous(size_t at)const noexcept{do{--at;}while(at&&(static_cast<unsigned char>(bytes_[at])&0xc0)==0x80);return at;}
 size_t next(size_t at)const noexcept{++at;while(at<size_&&(static_cast<unsigned char>(bytes_[at])&0xc0)==0x80)++at;return at;}
 void erase(size_t begin,size_t end)noexcept{std::memmove(bytes_.data()+begin,bytes_.data()+end,size_-end+1);size_-=end-begin;std::fill(bytes_.begin()+size_+1,bytes_.end(),0);++revision_;}
 std::array<char,Capacity+1> bytes_{};
 size_t size_=0,cursor_=0;
 uint32_t revision_=0;
 bool selected_=false;
};
}
