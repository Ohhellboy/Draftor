// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifdef ARDUINO_ARCH_ESP32
#include "wifi_transfer.hpp"
#include <esp_netif.h>
namespace s3app {
class S3WifiTransport final : public WifiTransport {
 public:
 ~S3WifiTransport(){stop();}
 bool start(char*,std::size_t,char*,std::size_t,char*,std::size_t)noexcept override;
 const char* startError()const noexcept override{return error_;}
 void stop()noexcept override;
 const char* address()const noexcept override{return address_;}
 bool accept()noexcept override;
 int receive(char*,std::size_t)noexcept override;
 int send(const char*,std::size_t)noexcept override;
 void close()noexcept override;
 void measure(const char*)noexcept override;
 char* allocate(std::size_t n)noexcept override{return static_cast<char*>(draftor::memory::allocate(n));}
 void deallocate(char* p,std::size_t)noexcept override{draftor::memory::release(p);}
 private:
 bool fail(const char*,esp_err_t)noexcept;
 esp_netif_t* ap_=nullptr;int listener_=-1,client_=-1;bool initialized_=false,started_=false,own_loop_=false;
 const char* error_="Wi-Fi startup failed";
 char address_[64]{};
};
}
#endif
