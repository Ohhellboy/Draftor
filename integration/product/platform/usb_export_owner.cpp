#include "usb_export_owner.hpp"
#include <algorithm>
#include <new>
namespace s3app {
namespace ex=draftor::exchange;
ex::OwnerState UsbExportOwner::probe()noexcept{
 ex::OwnerState s;const auto bound=app_.catalog.boundSession();
 s.identity.collection=bound.collection_id.bytes;s.identity.document=app_.session.currentId().bytes;
 s.identity.mount=bound.mount_epoch;s.identity.document_session=app_.session.sessionSerial();
 s.identity.revision_epoch=app_.document.textRevisionEpoch();s.identity.body_revision=app_.document.textRevision();
 s.has_current=app_.session.hasCurrent()&&app_.catalog.validateSession()==ds::Status::Ok;
 s.body_bytes=app_.document.text().size();s.composition=input_.composition;s.semantic_pending=input_.semantic_pending;
 s.file_operation=app_.commandBarrier()||app_.session.busy();s.pending_body_edits=input_.pending_body_edits;
 s.revision_reliable=s.identity.revision_epoch!=0;s.save_busy=app_.persistence.backgroundSaveActive();
 s.body_dirty=app_.persistence.bodyDirty();s.position_dirty=app_.persistence.positionDirty();return s;
}
bool UsbExportOwner::freeze()noexcept{
 if(frozen_||!app_.transferActive())return false;
 frozen_=true;return true;
}
ex::SaveReply UsbExportOwner::saved(ds::OperationResult r)noexcept{
 if(r.status==ds::Status::Busy&&app_.persistence.backgroundSaveActive())return {ex::SaveState::Pending,int(r.status),r.body_committed,false};
 app_.transferSaveResult(r);
 return {r.status==ds::Status::Partial?ex::SaveState::Partial:r?ex::SaveState::Complete:ex::SaveState::Failed,
         int(r.status),r.body_committed,!app_.persistence.positionDirty()};
}
ex::SaveReply UsbExportOwner::beginAutoSave()noexcept{
 try{
  if(!frozen_||!app_.transferActive())return {ex::SaveState::Failed,int(ds::Status::Busy),false,false};
  return saved(app_.persistence.bodyDirty()?app_.persistence.startAutoSave(now_,app_.editorPort):app_.persistence.flushCurrentPosition(app_.editorPort));
 }catch(const std::bad_alloc&){app_.persistence.abortForMemoryFailure();return {ex::SaveState::Failed,int(ds::Status::ResourceExhausted),false,false};}
}
ex::SaveReply UsbExportOwner::serviceSave()noexcept{
 try{return saved(app_.session.tick(now_,{true,false,false}));}
 catch(const std::bad_alloc&){app_.persistence.abortForMemoryFailure();return {ex::SaveState::Failed,int(ds::Status::ResourceExhausted),false,false};}
}
ex::BorrowReply UsbExportOwner::borrow(std::size_t offset,std::size_t max,void* context,ex::BorrowConsumer consumer)noexcept{
 const std::string_view body=app_.document.text();
 if(!frozen_||!app_.transferActive()||!consumer||!max||offset>=body.size())return {};
 const auto n=std::min(max,body.size()-offset);consumer(context,body.substr(offset,n));return {true,n};
}
}
