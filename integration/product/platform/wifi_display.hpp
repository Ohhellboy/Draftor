// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "wifi_transfer.hpp"
#include "draftor_qr/render.hpp"
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
}
