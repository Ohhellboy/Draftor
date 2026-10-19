// SPDX-License-Identifier: Apache-2.0
#ifdef ARDUINO_ARCH_ESP32
#include "wifi_platform_s3.hpp"
#include <Arduino.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <esp_netif_defaults.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
namespace s3app {
namespace {
bool netifReady=false; // IDF TCP/IP infrastructure has no supported deinit.
bool nonblock(int fd){return lwip_fcntl(fd,F_SETFL,O_NONBLOCK)==0;}
int ioResult(int n){return n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)?-2:n;}
}
bool S3WifiTransport::fail(const char* stage,esp_err_t code)noexcept{
 error_=code==ESP_ERR_NO_MEM?"Wi-Fi needs more internal memory":"Wi-Fi startup failed";
 Serial.printf("[WIFI_START_ERROR] stage=%s code=%d\n",stage,int(code));
 stop();return false;
}
bool S3WifiTransport::start(char* ssid,std::size_t sn,char* password,std::size_t pn,char* token,std::size_t tn)noexcept{
 error_="Wi-Fi startup failed";
 if(initialized_||sn<33||pn<17||tn<33)return false;
 constexpr auto internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 // Reject before retaining first-use TCP/IP infrastructure when the editor
 // has already consumed the driver headroom. A passing guard is not a promise
 // that SDK allocation succeeds: every startup call remains checked below.
 if(heap_caps_get_free_size(internal)<48*1024||heap_caps_get_largest_free_block(internal)<8*1024)
  return fail("admission",ESP_ERR_NO_MEM);
 if(!netifReady){const auto err=esp_netif_init();if(err!=ESP_OK)return fail("netif_init",err);netifReady=true;}
 const auto loop=esp_event_loop_create_default();
 if(loop!=ESP_OK&&loop!=ESP_ERR_INVALID_STATE)return fail("event_loop",loop);
 own_loop_=loop==ESP_OK;
 // IDF does not support esp_netif_deinit; its TCP/IP task remains after first
 // use. Release our default event loop, AP, DHCP, driver and sockets on stop.
 // The convenience create_default_wifi_ap helper asserts on OOM. Use its
 // public steps with checked results so a low-memory request can return to
 // the editor instead of resetting the product.
 const esp_netif_config_t netConfig=ESP_NETIF_DEFAULT_WIFI_AP();
 ap_=esp_netif_new(&netConfig);if(!ap_)return fail("netif_new",ESP_ERR_NO_MEM);
 auto err=esp_netif_attach_wifi_ap(ap_);if(err!=ESP_OK)return fail("attach_ap",err);
 err=esp_wifi_set_default_wifi_ap_handlers();if(err!=ESP_OK)return fail("ap_handlers",err);
 wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();cfg.nvs_enable=0;
 // Six DMA RX/TX buffers retain AMPDU and two-client AP operation, matching
 // IDF's PSRAM memory-saving configuration. Dynamic RX capacity stays at 32.
 cfg.static_rx_buf_num=6;cfg.static_tx_buf_num=6;cfg.rx_ba_win=6;
 measure("netif_ready");
 err=esp_wifi_init(&cfg);if(err!=ESP_OK)return fail("wifi_init",err);initialized_=true;
 measure("driver_ready");
 std::uint8_t mac[6]{},random[24]{};esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);esp_fill_random(random,sizeof(random));
 std::snprintf(ssid,sn,"Draftor-%02X%02X%02X",mac[3],mac[4],mac[5]);
 for(unsigned i=0;i<8;++i)std::snprintf(password+i*2,pn-i*2,"%02x",random[i]);
 for(unsigned i=0;i<16;++i)std::snprintf(token+i*2,tn-i*2,"%02x",random[i+8]);
 wifi_config_t config{};std::memcpy(config.ap.ssid,ssid,std::strlen(ssid));config.ap.ssid_len=std::strlen(ssid);
 std::memcpy(config.ap.password,password,16);config.ap.channel=1;config.ap.max_connection=2;
 config.ap.authmode=WIFI_AUTH_WPA2_PSK;config.ap.pairwise_cipher=WIFI_CIPHER_TYPE_CCMP;config.ap.pmf_cfg.capable=true;config.ap.pmf_cfg.required=false;
 err=esp_wifi_set_storage(WIFI_STORAGE_RAM);if(err!=ESP_OK)return fail("storage",err);
 err=esp_wifi_set_mode(WIFI_MODE_AP);if(err!=ESP_OK)return fail("ap_mode",err);
 err=esp_wifi_set_config(WIFI_IF_AP,&config);if(err!=ESP_OK)return fail("ap_config",err);
 err=esp_wifi_start();if(err!=ESP_OK)return fail("wifi_start",err);started_=true;
 listener_=lwip_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 if(listener_<0)return fail("socket",errno);
 int yes=1;lwip_setsockopt(listener_,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
 // AP-start/DHCP events are asynchronous. Bind the listener independently of
 // that event; this product starts only an AP and validates the HTTP Host.
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(80);address.sin_addr.s_addr=htonl(INADDR_ANY);
 if(!nonblock(listener_)||lwip_bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||lwip_listen(listener_,2)!=0)return fail("listen",errno);
 // Read the configured AP address and bound service port, rather than copying
 // a UI example. Neither the Wi-Fi password nor the HTTP token enters this URL.
 esp_netif_ip_info_t ip{};err=esp_netif_get_ip_info(ap_,&ip);if(err!=ESP_OK)return fail("ap_address",err);
 socklen_t size=sizeof(address);
 if(lwip_getsockname(listener_,reinterpret_cast<sockaddr*>(&address),&size)!=0)return fail("service_port",errno);
 const unsigned port=ntohs(address.sin_port);
 if(!ip.ip.addr||!port)return fail("service_address",ESP_ERR_INVALID_STATE);
 if(port==80)std::snprintf(address_,sizeof(address_),"http://" IPSTR,IP2STR(&ip.ip));
 else std::snprintf(address_,sizeof(address_),"http://" IPSTR ":%u",IP2STR(&ip.ip),port);
 return true;
}
void S3WifiTransport::close()noexcept{if(client_>=0){::lwip_shutdown(client_,SHUT_RDWR);::lwip_close(client_);client_=-1;}}
void S3WifiTransport::stop()noexcept{
 address_[0]=0;
 close();if(listener_>=0){::lwip_close(listener_);listener_=-1;}
 if(started_){esp_wifi_stop();started_=false;}
 if(initialized_){esp_wifi_deinit();initialized_=false;}
 if(ap_){esp_netif_destroy_default_wifi(ap_);ap_=nullptr;}
 if(own_loop_){esp_event_loop_delete_default();own_loop_=false;}
}
bool S3WifiTransport::accept()noexcept{
 if(listener_<0||client_>=0)return false;
 client_=::lwip_accept(listener_,nullptr,nullptr);if(client_<0)return false;
 if(!nonblock(client_)){close();return false;}return true;
}
int S3WifiTransport::receive(char* data,std::size_t n)noexcept{return client_<0?-1:ioResult(::lwip_recv(client_,data,n,MSG_DONTWAIT));}
int S3WifiTransport::send(const char* data,std::size_t n)noexcept{return client_<0?-1:ioResult(::lwip_send(client_,data,n,MSG_DONTWAIT));}
void S3WifiTransport::measure(const char* stage)noexcept{
 constexpr auto internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,psram=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
 Serial.printf("[WIFI_MEM] stage=%s internal_free=%u internal_largest=%u internal_min=%u psram_free=%u psram_largest=%u psram_min=%u\n",stage,unsigned(heap_caps_get_free_size(internal)),unsigned(heap_caps_get_largest_free_block(internal)),unsigned(heap_caps_get_minimum_free_size(internal)),unsigned(heap_caps_get_free_size(psram)),unsigned(heap_caps_get_largest_free_block(psram)),unsigned(heap_caps_get_minimum_free_size(psram)));
}
}
#endif
