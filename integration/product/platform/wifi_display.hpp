// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "wifi_transfer.hpp"
#include "draftor_qr/render.hpp"
#include "secondary_menu.hpp"
namespace s3app {
inline constexpr draftor::qr::Rect WifiQrRect{8, 66, 284, 240};
// Shared by device draw() and the Host ST7306 preview. Label uses the product's
// normal small font. All text stays outside the QR rectangle and quiet zone.
template<class Fill, class Label>
void paintWifiPanel(const WifiTransfer& wifi, Fill fill, Label label) {
 fill(0, 0, 300, 400, false);
 const auto page=wifi.qr().page();
 if(page==WifiQrPage::Manual){
  label("Wi-Fi manual details",8,8);
  label("SSID:",8,44);label(wifi.ssid(),8,68);
  label("Password:",8,112);label(wifi.password(),8,136);
  label("Transfer address:",8,180);label(wifi.address(),8,204);
  label("P: return to QR",8,276);
 }else{
  const bool connect=page==WifiQrPage::Connect;
  label(connect?"1 Scan to join Wi-Fi":"2 Open transfer page",8,8);
  label(connect?wifi.ssid():wifi.address(),8,36);
  if(wifi.qr().status()!=draftor::qr::Status::Ok)
   label(draftor::qr::statusText(wifi.qr().status()),8,158);
  else if(!draftor::qr::paint(wifi.qr().matrix(),WifiQrRect,fill))label("QR unavailable",8,158);
  label(connect?"Confirm on your phone":"Join Wi-Fi first",8,316);
 }
 label("Tab: switch  P: manual",8,340);
 label("Esc: write   O: off",8,362);
 label("No internet is OK",8,384);
}
// R2 product path. QR matrix, fit rectangle, payload, and service state all come
// from the original controller; only labels and surrounding layout change.
template<class Fill,class Aux,class Content>
void paintWifiPanel(const WifiTransfer& wifi,const s3host::FontPort& contentFont,
                   Fill fill,Aux aux,Content content){
 using namespace display;
 const auto page=wifi.qr().page();
 secondary::header(page==WifiQrPage::Manual?"Wi-Fi 连接信息":
                   page==WifiQrPage::Connect?"扫码连接 Wi-Fi":"扫码打开传输页面",fill,aux);
 if(page==WifiQrPage::Manual){
  aux("热点名称",8,48,false);
  int y=secondary::wrapped(wifi.ssid(),contentFont,8,72,284,28,356,content)+28;
  aux("密码",8,y,false);y+=24;
  y=secondary::wrapped(wifi.password(),contentFont,8,y,284,28,356,content)+28;
  aux("传输地址",8,y,false);y+=24;
  secondary::wrapped(wifi.address(),contentFont,8,y,284,28,356,content);
 }else{
  secondary::preview(page==WifiQrPage::Connect?wifi.ssid():wifi.address(),contentFont,8,32,284,content);
  if(wifi.qr().status()==draftor::qr::Status::Ok){
   if(!draftor::qr::paint(wifi.qr().matrix(),WifiQrRect,fill))aux("二维码无法显示",8,158,false);
  }else{
   const char* error="二维码暂不可用";
   switch(wifi.qr().status()){
    case draftor::qr::Status::InvalidArgument:error="连接信息无效";break;
    case draftor::qr::Status::InvalidCharacter:error="连接信息含无效字符";break;
    case draftor::qr::Status::Capacity:error="连接信息过长";break;
    case draftor::qr::Status::AllocationFailed:error="二维码内存不足";break;
    case draftor::qr::Status::WorkspaceTooSmall:error="二维码空间不足";break;
    default:break;
   }
   aux(error,8,158,false);aux("P：查看完整连接信息",8,182,false);
  }
  aux("密码",8,310,false);secondary::preview(wifi.password(),contentFont,52,308,240,content);
  aux("地址",8,338,false);secondary::preview(wifi.address(),contentFont,52,336,240,content);
 }
 // Only F2/trash has an interactive help route in this integration. These are
 // the existing panel actions, not a second modal or a global key handler.
 aux(page==WifiQrPage::Manual?"Tab/P 连接码  Esc 返回  O 关热点":
                             "Tab 换码  P 详情  Esc 返回  O 关热点",8,372,false);
}
}
