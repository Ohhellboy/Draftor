// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core_memory.hpp"
#include <algorithm>
#include <cstring>
#ifndef DRAFTOR_TEXT_RESIDENT
#define DRAFTOR_TEXT_RESIDENT 0
#endif
static_assert(DRAFTOR_TEXT_RESIDENT==0||DRAFTOR_TEXT_RESIDENT==1,"research arm");
namespace ime_device {
// One immutable physical range below the unchanged authenticated reader.
// It grants no text/selection authority. Only a complete preload is readable;
// the normal verifier still checks every demanded packet under its fixed pin.
class StaticTextResidency {
 draftor::memory::ArrayPtr<unsigned char> bytes_;
 std::size_t offset_=0,size_=0,filled_=0;
 public:
 static constexpr std::size_t Limit=4*1024*1024;
 bool prepare(std::size_t offset,std::size_t size,std::size_t fileBytes) noexcept {
  // Never replace a live range. Resource destruction ends this mount lifetime.
  if(bytes_||!size||size>Limit||offset>fileBytes||size>fileBytes-offset)return false;
  bytes_.reset(draftor::memory::array<unsigned char>(size));
  if(!bytes_)return false;
  offset_=offset;size_=size;filled_=0;return true;
 }
 template<class Read> bool advance(Read read,std::size_t maximum) {
  if(!bytes_||!maximum)return false;
  if(filled_==size_)return true;
  const auto n=std::min(maximum,size_-filled_);
  if(!read(offset_+filled_,bytes_.get()+filled_,n))return false;
  filled_+=n;return true;
 }
 bool copy(std::size_t offset,void* destination,std::size_t size)const noexcept {
  if(!bytes_||filled_!=size_||offset<offset_||offset-offset_>size_||size>size_-(offset-offset_)||(!destination&&size))return false;
  if(size)std::memcpy(destination,bytes_.get()+offset-offset_,size);
  return true;
 }
 std::size_t requested()const noexcept{return size_;}
 std::size_t loaded()const noexcept{return filled_;}
 const void* allocation()const noexcept{return bytes_.get();}
 bool ready()const noexcept{return bytes_&&filled_==size_;}
};
}
