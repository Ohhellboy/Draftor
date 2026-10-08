#pragma once
#include "usb_transfer.hpp"
#include "media_bridge.hpp"
namespace u=draftor::usb;
struct Platform final:s3app::UsbPlatform,u::InboxMediaPort,u::InboxFilesystemPort,u::BlockDevice,u::MediaPort {
 u::MediaBridge bridge;std::string wire="\xef\xbb\xbfhello\r\n世界\rnext\n";
 std::string filename="incoming.txt";
 std::uint64_t session=0;unsigned allocs=0;bool sealed=false,mounted=false,opened=false,enumerated=false,allocated=false,hold=false;
 u::MediaPort& media()noexcept override{return *this;}
 u::InboxMediaPort& backing()noexcept override{return *this;}
 u::InboxFilesystemPort& files()noexcept override{return *this;}
 u::Status prepare_result=u::Status::Ok,allocate_result=u::Status::Ok,backing_result=u::Status::Ok,publish_result=u::Status::Ok,mount_result=u::Status::Ok;
 unsigned inbox_alloc_calls=0,inbox_free_calls=0;bool unsafe_free=false;
 u::Status prepare(std::uint32_t)noexcept override{return prepare_result;}
 void cancelPreparation()noexcept override{}
 u::Status allocateInbox()noexcept override{++inbox_alloc_calls;if(allocate_result!=u::Status::Ok)return allocate_result;allocated=true;return u::Status::Ok;}
 void freeInbox()noexcept override{++inbox_free_calls;const auto s=state();unsafe_free|=opened||mounted||s.accepting||!s.drained;allocated=false;}
 char* allocate(std::size_t n)noexcept override{auto p=static_cast<char*>(std::malloc(n));if(p)++allocs;return p;}
 void deallocate(char* p,std::size_t)noexcept override{if(p){--allocs;std::free(p);}}
 u::Status publish(u::BlockDevice& b,u::Access a,std::uint32_t& g)noexcept override{return publish_result==u::Status::Ok?bridge.publish(b,a,g):publish_result;}
 void withdraw(u::EndReason e)noexcept override{bridge.withdraw(e);}
 u::MediaState state()const noexcept override{auto s=bridge.state();if(hold)s.drained=false;return s;}
 u::Status detach()noexcept override{return hold?u::Status::Busy:bridge.detach();}
 u::Status prepare(std::uint64_t s)noexcept override{session=s;sealed=mounted=opened=enumerated=false;if(backing_result!=u::Status::Ok)return backing_result;return allocated?u::Status::Ok:u::Status::NotReady;}
 u::BlockDevice& device()noexcept override{return *this;}
 u::Status seal(std::uint64_t s)noexcept override{if(s!=session)return u::Status::Stale;sealed=true;return u::Status::Ok;}
 bool stable(std::uint64_t s)const noexcept override{return sealed&&s==session;}
 void release()noexcept override{sealed=false;}
 u::Status mountReadOnly(std::uint64_t s)noexcept override{if(!stable(s))return u::Status::NotReady;if(mount_result!=u::Status::Ok)return mount_result;mounted=true;return u::Status::Ok;}
 u::Status next(u::InboxCandidate& c)noexcept override{
  if(enumerated)return u::Status::NotFound;
  enumerated=true;c={};c.source={session,1,wire.size()};std::strncpy(c.name.data(),filename.c_str(),c.name.size()-1);return u::Status::Ok;
 }
 u::Status open(const u::InboxCandidate& c)noexcept override{if(!mounted||c.source.session!=session)return u::Status::Stale;opened=true;return u::Status::Ok;}
 void closeFile()noexcept override{opened=false;}
 void unmount()noexcept override{mounted=false;}
 std::uint32_t sectorCount()const noexcept override{return 1024;}
 u::IoResult read(std::uint64_t at,void* b,std::size_t n)noexcept override{
  if(opened){if(at>wire.size()||n>wire.size()-at)return {u::Status::Bounds,0};std::memcpy(b,wire.data()+at,n);}
  else std::memset(b,0,n);
  return {u::Status::Ok,n};
 }
 u::IoResult write(std::uint64_t,const void*,std::size_t n)noexcept override{return {u::Status::Ok,n};}
 u::Status sync()noexcept override{return u::Status::Ok;}
};
