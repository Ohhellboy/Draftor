// SPDX-License-Identifier: Apache-2.0
#include "wifi_qr.hpp"
#include <cstring>
namespace s3app {
namespace q = draftor::qr;
namespace {
bool special(char c) noexcept { return c == '\\' || c == ';' || c == ':' || c == ',' || c == '"'; }
bool text(std::string_view s) noexcept {
 for (unsigned char c : s) if (c < 32 || c == 127) return false;
 return true;
}
}
q::Status wifiQrContent(std::string_view ssid, std::string_view password,
                       char* out, std::size_t capacity, std::size_t& length) noexcept {
 length = 0;
 if (!out || ssid.empty() || ssid.size() > 32 || password.size() < 8 || password.size() > 16 ||
     !text(ssid) || !text(password)) return q::Status::InvalidArgument;
 std::size_t required = 18 + ssid.size() + password.size();
 for (char c : ssid) required += special(c);
 for (char c : password) required += special(c);
 if (capacity <= required) return q::Status::WorkspaceTooSmall;
 // ZXing Barcode Contents / WifiResultParser: escape each reserved byte once.
 // Literal double quotes are escaped, not introduced as password delimiters.
 auto append = [&](std::string_view s) { for (char c : s) out[length++] = c; };
 auto escaped = [&](std::string_view s) { for (char c : s) { if (special(c)) out[length++] = '\\'; out[length++] = c; } };
 append("WIFI:T:WPA;S:"); escaped(ssid); append(";P:"); escaped(password); append(";;");
 out[length] = 0;
 return q::Status::Ok;
}
void WifiQr::clear() noexcept {
 matrix_ = {};
 if (storage_) { q::erase(storage_, WifiQrBufferBytes); allocator_.deallocate(storage_, WifiQrBufferBytes); storage_ = nullptr; }
}
void WifiQr::show(WifiQrPage page, std::string_view ssid, std::string_view password, std::string_view address) noexcept {
 clear(); page_ = page; status_ = q::Status::Ok;
 if (page == WifiQrPage::Manual) return;
 // One owner on the product loop. The scratch allocation includes the bounded
 // payload; it is erased and freed before returning. No cross-consumer borrow.
 constexpr auto temporaryBytes = WifiQrBufferBytes + WifiQrContentBytes;
 char* temporary = allocator_.allocate(temporaryBytes);
 if (!temporary) { status_ = q::Status::AllocationFailed; return; }
 char* content = temporary + WifiQrBufferBytes;
 std::size_t length = 0;
 if (page == WifiQrPage::Connect) status_ = wifiQrContent(ssid, password, content, WifiQrContentBytes, length);
 else if (page == WifiQrPage::Address) {
  if (address.empty() || address.size() > 63 || !text(address) || address.substr(0, 7) != "http://" ||
      address.find_first_of("?@#") != address.npos) status_ = q::Status::InvalidArgument;
  else { length = address.size(); std::memcpy(content, address.data(), length); }
 } else status_ = q::Status::InvalidArgument;
 if (status_ == q::Status::Ok) {
  storage_ = allocator_.allocate(WifiQrBufferBytes);
  if (!storage_) status_ = q::Status::AllocationFailed;
  else status_ = q::encode(reinterpret_cast<const std::uint8_t*>(content), length, q::Mode::Bytes,
      {1, WifiQrMaxVersion, q::Ecc::Medium}, {reinterpret_cast<std::uint8_t*>(temporary), WifiQrBufferBytes},
      {reinterpret_cast<std::uint8_t*>(storage_), WifiQrBufferBytes}, matrix_);
 }
 q::erase(temporary, temporaryBytes); allocator_.deallocate(temporary, temporaryBytes);
 if (status_ != q::Status::Ok) clear();
}
}
