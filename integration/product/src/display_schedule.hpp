#pragma once
#include "core_memory.hpp"
#include <cstdint>
#include <cstring>
namespace ime_device {
// Loop-owned, exact last-transferred pixels in PSRAM. Allocation failure keeps
// ordinary full transfers; a hash collision can never suppress an update.
class DisplaySchedule {
 draftor::memory::ArrayPtr<uint8_t> pixels_;
 size_t size_=0;
 bool known_=false;
 uint32_t last_done_=0;
 public:
 uint32_t sent=0,reused=0,failed=0;
 void prepare(size_t size){if(pixels_)return;pixels_.reset(draftor::memory::array<uint8_t>(size));size_=pixels_?size:0;}
 size_t memoryBytes()const{return size_;}
 bool due(uint32_t now,bool dirty,bool keys,bool foreground)const{
  if(!dirty&&!foreground)return false;
  if(foreground)return true;
  const auto age=uint32_t(now-last_done_);
  return age>=33&&(!keys||age>=50);
 }
 template<class Transfer> bool present(const uint8_t* data,size_t size,uint32_t now,Transfer transfer){
  if(known_&&size==size_&&std::memcmp(pixels_.get(),data,size)==0){++reused;last_done_=now;return true;}
  if(!transfer()){++failed;known_=false;return false;}
  ++sent;
  if(size==size_&&pixels_){std::memcpy(pixels_.get(),data,size);known_=true;}
  last_done_=now;return true;
 }
 void completed(uint32_t now){last_done_=now;}
};
}
