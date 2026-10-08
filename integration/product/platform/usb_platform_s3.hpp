#pragma once
#ifdef ARDUINO_ARCH_ESP32
#include "usb_transfer.hpp"
#include "tinyusb_msc.hpp"
#include "fatfs_inbox.hpp"
#include <esp_heap_caps.h>
namespace s3app {
class S3UsbPlatform final : public UsbPlatform {
 public:
 S3UsbPlatform()noexcept:fat_(disk_){}
 usb::MediaPort& media()noexcept override{return usb::s3::TinyUsbMsc::instance();}
 usb::InboxMediaPort& backing()noexcept override{return fat_;}
 usb::InboxFilesystemPort& files()noexcept override{return fat_;}
 usb::Status prepare(std::uint32_t now)noexcept override{
  auto& endpoint=usb::s3::TinyUsbMsc::instance();
  if(!endpoint.start("Draftor USB v1.2 build236"))return usb::Status::Io;
  return endpoint.prepareSession(now);
 }
 void cancelPreparation()noexcept override{usb::s3::TinyUsbMsc::instance().cancelPreparation();}
 usb::Status allocateInbox()noexcept override{
  // The controller owns this disk until its session and leases have drained.
  // An already-owned disk incurs no second allocation or headroom charge.
  if(disk_.bytes())return usb::Status::Ok;
  if(!disk_.validCapacity())return usb::Status::InvalidArgument;
  // Leave room for the bounded import body, verification read and document open.
  // These are live admission checks, not a claim about measured device headroom.
  constexpr auto caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
  constexpr std::size_t reserve=256*1024;
  const auto requested=disk_.requestedBytes();
  const auto available=heap_caps_get_free_size(caps);
  if(heap_caps_get_largest_free_block(caps)<requested||available<requested||available-requested<reserve)return usb::Status::Full;
  const auto status=disk_.allocate();
  if(status!=usb::Status::Ok)return status;
  // Still private: FatFs and the host have not received this allocation yet.
  // Account for allocator overhead and intervening use without locking the heap.
  if(heap_caps_get_free_size(caps)<reserve){disk_.release();return usb::Status::Full;}
  return usb::Status::Ok;
 }
 void freeInbox()noexcept override{disk_.release();}
 char* allocate(std::size_t n)noexcept override{return static_cast<char*>(draftor::memory::allocate(n));}
 void deallocate(char* p,std::size_t)noexcept override{draftor::memory::release(p);}
 private:
 usb::s3::PsramDisk disk_;usb::s3::FatFsInbox fat_;
};
}
#endif
