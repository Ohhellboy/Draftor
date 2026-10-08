#pragma once
#include "writing_app.hpp"
#include "draftor_exchange/export_session.hpp"

namespace s3app {
// Product-owned admission facts; the BLE owner still owns physical key state.
struct TransferInputState { bool composition=false,semantic_pending=false,pending_body_edits=false; };
class UsbExportOwner final : public draftor::exchange::OwnerPort {
 public:
 explicit UsbExportOwner(App& app) noexcept : app_(app) {}
 void observe(std::uint32_t now,TransferInputState state)noexcept{now_=now;input_=state;}
 draftor::exchange::OwnerState probe()noexcept override;
 bool freeze()noexcept override;
 void unfreeze()noexcept override{frozen_=false;}
 draftor::exchange::SaveReply beginAutoSave()noexcept override;
 draftor::exchange::SaveReply serviceSave()noexcept override;
 draftor::exchange::BorrowReply borrow(std::size_t,std::size_t,void*,draftor::exchange::BorrowConsumer)noexcept override;
 private:
 draftor::exchange::SaveReply saved(ds::OperationResult)noexcept;
 App& app_;TransferInputState input_{};std::uint32_t now_=0;bool frozen_=false;
};
}
