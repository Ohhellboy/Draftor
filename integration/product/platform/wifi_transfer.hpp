// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ruotao Wang
#pragma once
#include "usb_export_owner.hpp"
#include "usb_document_import.hpp"
#include "wifi_qr.hpp"

namespace s3app {
namespace wx = draftor::exchange;
// All calls belong to the product loop. recv/send MUST be nonblocking;
// -2 means would-block, 0 EOF, -1 failure. No worker accesses App or TF.
class WifiTransport : public wx::Allocator {
 public:
 virtual bool start(char* ssid, std::size_t, char* password, std::size_t,
                    char* token, std::size_t) noexcept = 0;
 virtual void stop() noexcept = 0;
 // Published after successful start; immutable until stop. Changing service
 // configuration or credentials requires stop/start and revokes the old QR.
 virtual const char* address() const noexcept = 0;
 // Must remain valid after the transport is destroyed (the editor retains it).
 virtual const char* startError() const noexcept {return "Wi-Fi startup failed";}
 virtual bool accept() noexcept = 0;
 virtual int receive(char*,std::size_t) noexcept = 0;
 virtual int send(const char*,std::size_t) noexcept = 0;
 virtual void close() noexcept = 0;
 virtual void measure(const char*) noexcept {}
};
// The Alpha snapshot implementation is reused unchanged. This owner exports
// the live buffer, not a saved version: persistence is deliberately not part
// of its protocol. It copies within one serialized product-owner turn, without
// changing App focus or holding an editor gate across network I/O.
// After acquire, stop retires further readers; the existing lease owns E
// through Closing while editing resumes. USB keeps its original save policy.
class WifiSnapshotOwner final : public wx::OwnerPort {
 public:
 explicit WifiSnapshotOwner(App& app):app_(app),live_(app){}
 void observe(std::uint32_t now,TransferInputState input)noexcept{live_.observe(now,input);}
 wx::OwnerState probe()noexcept override {
  auto s=live_.probe();s.save_busy=s.body_dirty=s.position_dirty=false;return s;
 }
 bool freeze()noexcept override{if(frozen_)return false;frozen_=true;return true;}
 void unfreeze()noexcept override{frozen_=false;}
 wx::SaveReply beginAutoSave()noexcept override{return {wx::SaveState::Failed,0,false,false};}
 wx::SaveReply serviceSave()noexcept override{return {wx::SaveState::Failed,0,false,false};}
 wx::BorrowReply borrow(std::size_t o,std::size_t n,void* c,wx::BorrowConsumer f)noexcept override{
  const std::string_view text=app_.document.text();
  if(!frozen_||!f||!n||o>=text.size())return {};
  if(n>text.size()-o)n=text.size()-o;
  f(c,text.substr(o,n));return {true,n};
 }
 private: App& app_;UsbExportOwner live_;bool frozen_=false;
};
enum class WifiPhase { Off,Listening,Headers,Upload,Verify,Publish,Send };
class WifiTransfer final {
 public:
 WifiTransfer(App&,WifiTransport&,UsbDocumentImport&)noexcept;
 ~WifiTransfer(){stop();}
 bool start(TransferInputState)noexcept;
 void stop()noexcept;
 void tick(std::uint32_t,TransferInputState)noexcept;
 bool key(const s3hid::Event&,TransferInputState)noexcept;
 bool active()const noexcept{return phase_!=WifiPhase::Off;}
 bool panel()const noexcept{return panel_;}
 bool blocksInput()const noexcept{return panel_||import_gate_;}
 WifiPhase phase()const noexcept{return phase_;}
 const char* ssid()const noexcept{return ssid_;}
 const char* password()const noexcept{return password_;}
 const char* message()const noexcept{return message_;}
 const char* token()const noexcept{return token_;}
 const char* address()const noexcept{return net_.address();}
 const WifiQr& qr()const noexcept{return qr_;}
 wx::ImportResult lastImport()const noexcept{return last_import_;}
 private:
 void request(TransferInputState)noexcept;
 void response(unsigned,const char*,const char*,std::size_t,const char* extra="")noexcept;
 void error(unsigned,const char*)noexcept;
 void finishConnection()noexcept;
 void abortImport()noexcept;
 bool capture(TransferInputState)noexcept;
 bool enterGate(TransferInputState)noexcept;
 void leaveGate()noexcept;
 void imported(wx::Status)noexcept;
 void showQr(WifiQrPage page)noexcept;
 bool append(const wx::Step&)noexcept;
 App& app_;WifiTransport& net_;UsbDocumentImport& sink_;
 WifiQr qr_;
 WifiSnapshotOwner owner_;wx::ExportSession snapshot_;wx::Lease lease_{};
 wx::ImportWire text_{65536};wx::ImportResult last_import_{};
 WifiPhase phase_=WifiPhase::Off;
 wx::SessionError snapshot_status_=wx::SessionError::None;
 char ssid_[33]{},password_[17]{},token_[33]{};
 char headers_[2049]{},reply_[1024]{},body_[768]{},input_[512]{},output_[516]{};
 const char* payload_=nullptr;const char* message_="Wi-Fi off";
 std::size_t header_bytes_=0,reply_bytes_=0,reply_at_=0,payload_bytes_=0,payload_at_=0,remaining_=0,body_bytes_=0;
 std::uint64_t operation_=0;
 std::uint32_t now_=0,progress_=0,accepted_=0;
 bool panel_=false,import_gate_=false,publishing_=false,gated_=false;
};
inline bool wifiEntryKey(const App& app,const s3hid::Event& e)noexcept{
 return !app.renameOpen()&&e.down&&!e.mods&&(e.usage==67||(app.filesOpen()&&e.usage==26));
}
}
