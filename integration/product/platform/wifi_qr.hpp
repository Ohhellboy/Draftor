// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "draftor_qr/qr.hpp"
#include "draftor_exchange/export_session.hpp"
#include <string_view>
namespace s3app {
enum class WifiQrPage { Connect, Address, Manual };
// Current product profile, not restrictions on the common QR encoder.
inline constexpr std::size_t WifiQrContentBytes = 115;
inline constexpr int WifiQrMaxVersion = 7;
inline constexpr std::size_t WifiQrBufferBytes = draftor::qr::bufferBytes(WifiQrMaxVersion);
draftor::qr::Status wifiQrContent(std::string_view ssid, std::string_view password,
                                char* out, std::size_t capacity, std::size_t& length) noexcept;
class WifiQr final {
 public:
 explicit WifiQr(draftor::exchange::Allocator& allocator) noexcept : allocator_(allocator) {}
 ~WifiQr() { clear(); }
 WifiQr(const WifiQr&) = delete;
 WifiQr& operator=(const WifiQr&) = delete;
 void clear() noexcept;
 void show(WifiQrPage page, std::string_view ssid, std::string_view password, std::string_view address) noexcept;
 draftor::qr::Matrix matrix() const noexcept { return matrix_; }
 draftor::qr::Status status() const noexcept { return status_; }
 WifiQrPage page() const noexcept { return page_; }
 private:
 draftor::exchange::Allocator& allocator_;
 char* storage_ = nullptr;
 draftor::qr::Matrix matrix_{};
 draftor::qr::Status status_ = draftor::qr::Status::Ok;
 WifiQrPage page_ = WifiQrPage::Connect;
};
}
