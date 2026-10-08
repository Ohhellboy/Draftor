#pragma once
#include "usb_export_owner.hpp"
#include "usb_document_import.hpp"
#include "draftor_usb_transfer/export.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include "draftor_usb_transfer/import.hpp"
#include "draftor_exchange/transfer_selection.hpp"

namespace s3app {
namespace usb=draftor::usb;
// Shared by the cold runtime gate and the live transfer controller.
// Rename owns its keys before file-page shortcuts (including first USB use).
inline bool usbEntryKey(const App& app,const s3hid::Event& e) noexcept {
 return !app.renameOpen()&&e.down&&!e.mods&&
   (e.usage==66||(app.filesOpen()&&(e.usage==24||e.usage==12)));
}
// Hardware lives behind this seam; the product controller also runs in Host tests.
class UsbPlatform : public draftor::exchange::Allocator {
 public:
 virtual usb::MediaPort& media()noexcept=0;
 virtual usb::InboxMediaPort& backing()noexcept=0;
 virtual usb::InboxFilesystemPort& files()noexcept=0;
 virtual usb::Status prepare(std::uint32_t now)noexcept=0;
 virtual void cancelPreparation()noexcept=0;
 virtual usb::Status allocateInbox()noexcept=0;
 virtual void freeInbox()noexcept=0;
};
enum class UsbUiPhase { Idle,Menu,PrepareExport,Exporting,PrepareInbox,Receiving,Selecting,Importing,Result,Closing,Opening };
class UsbTransfer final {
 public:
 UsbTransfer(App&,UsbPlatform&,UsbDocumentImport&)noexcept;
 bool key(const s3hid::Event&,TransferInputState);
 void tick(std::uint32_t,TransferInputState);
 void cancel()noexcept;
 bool active()const noexcept{return phase_!=UsbUiPhase::Idle;}
 UsbUiPhase phase()const noexcept{return phase_;}
 usb::Status request(bool importing,TransferInputState);
 const char* title()const noexcept;
 void line(unsigned,char*,std::size_t)const noexcept;
 usb::ImportResult imported()const noexcept{return receipt_;}
 std::uint32_t serial()const noexcept{return serial_;}
 usb::Status status()const noexcept{return status_;}
 private:
 enum class AfterClose : std::uint8_t { StayOnCurrent, OpenImported };
 void beginClose(AfterClose)noexcept;
 void change(UsbUiPhase,const char*)noexcept;
 void finish(const char*,usb::Status=usb::Status::Ok)noexcept;
 void releaseSource()noexcept;
 void startImport();
 void closeStep();
 App& app_;UsbPlatform& platform_;UsbDocumentImport& sink_;
 UsbExportOwner owner_;draftor::exchange::ExportSession snapshot_;
 usb::UsbExport export_;usb::InboxSession inbox_;usb::ImportJob job_;
 draftor::exchange::TransferSelection selection_;
 draftor::memory::Vector<usb::InboxCandidate> candidates_;
 usb::ImportResult receipt_{};
 UsbUiPhase phase_=UsbUiPhase::Idle;
 usb::Status status_=usb::Status::Ok;
 const char* message_="";
 std::uint64_t lease_=0,transfer_token_=0;std::uint32_t now_=0,serial_=0,closing_since_=0;
 std::size_t selected_=0;
 AfterClose after_close_=AfterClose::StayOnCurrent;
 bool import_mode_=false,enumerated_=false,inbox_memory_=false,report_after_close_=false;
};
}
