#include "usb_transfer.hpp"
#include <cstdio>
#include <algorithm>
namespace s3app {
UsbTransfer::UsbTransfer(App& app,UsbPlatform& platform,UsbDocumentImport& sink)noexcept
 :app_(app),platform_(platform),sink_(sink),owner_(app),snapshot_(owner_,platform,{65536,256}),
 export_(snapshot_,platform.media()),inbox_(platform.backing(),platform.files(),platform.media()),job_(sink){}
void UsbTransfer::change(UsbUiPhase p,const char* message)noexcept{
 if(phase_!=p)app_.transferBoundary();phase_=p;message_=message;++serial_;app_.dirtyDisplay=true;
 if(transfer_token_){
  if(p==UsbUiPhase::Receiving||p==UsbUiPhase::Exporting)selection_.stage(transfer_token_,draftor::exchange::TransferStage::Transferring);
  if(p==UsbUiPhase::Importing)selection_.stage(transfer_token_,draftor::exchange::TransferStage::Committing);
 }
}
void UsbTransfer::finish(const char* message,usb::Status status)noexcept{status_=status;change(UsbUiPhase::Result,message);}
usb::Status UsbTransfer::request(bool importing,TransferInputState input){
 if(active()&&phase_!=UsbUiPhase::Menu)return usb::Status::Busy;
 if(input.composition||input.semantic_pending||input.pending_body_edits)return usb::Status::Busy;
 if(!active()&&!app_.enterTransfer())return usb::Status::Busy;
 const auto admitted=selection_.acquire(importing?draftor::exchange::TransferChannel::UsbImport:draftor::exchange::TransferChannel::UsbExport,transfer_token_);
 if(admitted!=usb::Status::Ok){if(phase_==UsbUiPhase::Idle)app_.leaveTransfer();return admitted;}
 import_mode_=importing;
 receipt_={};
 status_=usb::Status::Ok;
 selected_=0;
 enumerated_=false;
 after_close_=AfterClose::StayOnCurrent;
 change(importing?UsbUiPhase::PrepareInbox:UsbUiPhase::PrepareExport,importing?"Preparing Inbox":"Saving current version");
 return usb::Status::Ok;
}
bool UsbTransfer::key(const s3hid::Event& e,TransferInputState input){
 if(!active()){
  const bool direct=app_.filesOpen()&&(e.usage==24||e.usage==12);
  if(!usbEntryKey(app_,e))return false;
  if(input.composition||input.semantic_pending||input.pending_body_edits||!app_.enterTransfer()){
   app_.notice="Finish composition / file action first";app_.dirtyDisplay=true;return true;
  }
  change(UsbUiPhase::Menu,"Choose one transfer");
  if(direct)request(e.usage==12,input);
  return true;
 }
 if(!e.down||e.mods)return true;
 if(e.usage==41){cancel();return true;}
 if(phase_==UsbUiPhase::Menu){if(e.usage==8||e.usage==24)request(false,input);else if(e.usage==12)request(true,input);}
 else if(phase_==UsbUiPhase::Selecting){
  if(e.usage==82&&selected_){--selected_;app_.dirtyDisplay=true;}
  if(e.usage==81&&selected_+1<candidates_.size()){++selected_;app_.dirtyDisplay=true;}
  if(e.usage==40&&enumerated_&&!candidates_.empty())startImport();
 }else if(phase_==UsbUiPhase::Result){
  // Default Esc continues the original. Opening is an explicit separate action.
  if(e.usage==40&&receipt_.outcome==usb::Outcome::Imported){
   beginClose(AfterClose::OpenImported);
  }
  if(e.usage==21&&receipt_.operation&&receipt_.status==usb::Status::Uncertain){
   usb::ImportResult recovered;const auto s=sink_.query(receipt_.operation,recovered);
   if(s==usb::Status::Ok&&recovered.outcome==usb::Outcome::Imported){receipt_=recovered;finish("Recovered published draft; original retained");}
   else{status_=s;app_.dirtyDisplay=true;}
  }
 }
 return true;
}
void UsbTransfer::releaseSource()noexcept{
 if(lease_){if(inbox_.release(lease_)==usb::Status::Ok)lease_=0;}
}
void UsbTransfer::cancel()noexcept{
 // Esc revokes a pending open, including while the outgoing save is draining.
 beginClose(AfterClose::StayOnCurrent);
}
void UsbTransfer::beginClose(AfterClose action)noexcept{
 if(!active())return;
 after_close_=action;
 if(transfer_token_){
  selection_.stage(transfer_token_,draftor::exchange::TransferStage::Closing);
 }
 const bool publishing=phase_==UsbUiPhase::Importing&&
                       job_.phase()==usb::ImportPhase::Publishing;
 job_.cancel();
 if(publishing){
  receipt_=job_.result();
  report_after_close_=true;
 }
 releaseSource();
 export_.close(usb::EndReason::Forced);
 inbox_.endHost(usb::EndReason::Forced);
 closing_since_=now_;
 change(UsbUiPhase::Closing,"Closing safely...");
}
void UsbTransfer::startImport(){
 if(inbox_.result().end!=usb::EndReason::Ejected){finish("Unconfirmed receipt; copy again",usb::Status::Uncertain);return;}
 usb::ImportSource* source=nullptr;auto s=inbox_.acquire(candidates_[selected_],source);
 if(s!=usb::Status::Ok){finish("TXT could not be opened",s);return;}
 lease_=inbox_.sourceLease();usb::ImportIntent intent;
 s=sink_.allocateOperation(intent.operation);
 if(s==usb::Status::Ok){intent.source=candidates_[selected_].source;intent.name=candidates_[selected_].name;s=job_.begin(*source,intent);}
 if(s!=usb::Status::Ok){releaseSource();finish("Import rejected; original retained",s);return;}
 change(UsbUiPhase::Importing,"Validating TXT / creating new draft");
}
void UsbTransfer::closeStep(){
 export_.tick();
 if(inbox_.phase()!=usb::InboxPhase::Preparing)inbox_.tick();
 if((export_.phase()!=usb::ExportPhase::Idle&&export_.phase()!=usb::ExportPhase::Closed)||lease_||inbox_.close()!=usb::Status::Ok){
  if(std::uint32_t(now_-closing_since_)>10000&&status_!=usb::Status::Busy){status_=usb::Status::Busy;change(UsbUiPhase::Closing,"Close stalled; resources retained");}
  return;
 }
 // Snapshot retirement is independent of the save owner. Keep the product
 // gate until any already-started persistence I/O has reached its own terminal.
 if(app_.persistence.backgroundSaveActive()){
  owner_.serviceSave();if(app_.persistence.backgroundSaveActive())return;
 }
 if(inbox_memory_){platform_.freeInbox();inbox_memory_=false;}
 platform_.cancelPreparation();
 draftor::memory::Vector<usb::InboxCandidate>().swap(candidates_);
 if(report_after_close_){report_after_close_=false;finish(receipt_.outcome==usb::Outcome::Imported?"Cancelled after publish; draft retained":"Publication uncertain; check operation",receipt_.status);return;}
 if(after_close_==AfterClose::OpenImported){
  change(UsbUiPhase::Opening,"Saving original before opening");
  return;
 }
 if(transfer_token_){selection_.release(transfer_token_,true);transfer_token_=0;}
 app_.leaveTransfer();change(UsbUiPhase::Idle,"");
}
void UsbTransfer::tick(std::uint32_t now,TransferInputState input){
 now_=now;owner_.observe(now,input);
 if(!active())return;
 try{
  switch(phase_){
  case UsbUiPhase::PrepareExport:case UsbUiPhase::PrepareInbox:{
   // Drain the existing persistence owner before reserving a new transaction.
   if(app_.persistence.backgroundSaveActive()){
    const auto r=owner_.serviceSave();if(r.state==draftor::exchange::SaveState::Failed||r.state==draftor::exchange::SaveState::Partial)finish("Save failed; original retained",usb::Status::Io);break;
   }
   const auto ready=platform_.prepare(now);
   if(ready==usb::Status::Busy)break;
   if(ready!=usb::Status::Ok){finish(ready==usb::Status::Io?
     "USB startup failed; original retained":"USB unavailable; reconnect data cable",ready);break;}
   if(import_mode_){
    auto s=platform_.allocateInbox();inbox_memory_=s==usb::Status::Ok;
    if(s==usb::Status::Ok)s=inbox_.begin();
    if(s==usb::Status::Ok)change(UsbUiPhase::Receiving,"Copy TXT to Inbox, then eject");else finish("Inbox allocation failed",s);
   }else{
    const auto s=export_.begin();if(s==usb::Status::Ok)change(UsbUiPhase::Exporting,"Preparing immutable DOCUMENT.TXT");else finish("Export admission failed",s);
   }
   break;
  }
  case UsbUiPhase::Exporting:{
   const auto before=export_.phase();const auto p=export_.tick();
   if(p!=before){++serial_;app_.dirtyDisplay=true;}
   if(p==usb::ExportPhase::Closed)finish(export_.error()==usb::Status::Ok?(platform_.media().state().end==usb::EndReason::Ejected?"Ejected; original retained":"Disconnected; copy not confirmed"):"Export failed; original retained",export_.error());
   break;
  }
  case UsbUiPhase::Receiving:{
   const auto before=inbox_.phase();const auto p=inbox_.tick();
   if(p!=before){++serial_;app_.dirtyDisplay=true;}
   if(p==usb::InboxPhase::Closed)finish("Inbox failed; original retained",inbox_.result().status);
   else if(p==usb::InboxPhase::Inspecting){
    if(inbox_.result().end!=usb::EndReason::Ejected)finish("Unconfirmed receipt; no draft created",usb::Status::Uncertain);
    else change(UsbUiPhase::Selecting,"Received; choose TXT to create new");
   }
   break;
  }
  case UsbUiPhase::Selecting:
   if(!enumerated_){
    usb::InboxCandidate item;const auto s=inbox_.next(item);
    if(s==usb::Status::Ok){
     if(candidates_.size()==32){finish("Too many TXT; maximum 32 per session",usb::Status::Full);break;}
     candidates_.push_back(item);app_.dirtyDisplay=true;
    }else if(s==usb::Status::NotFound){enumerated_=true;app_.dirtyDisplay=true;if(candidates_.empty())finish("No visible TXT in Inbox",usb::Status::NotFound);}
    else if(s!=usb::Status::Busy)finish("Inbox directory invalid",s);
   }
   break;
  case UsbUiPhase::Importing:{
   const auto p=job_.tick();
   if(p==usb::ImportPhase::Complete||p==usb::ImportPhase::Failed||p==usb::ImportPhase::Uncertain){
    receipt_=job_.result();releaseSource();
    finish(p==usb::ImportPhase::Complete?"New draft published; original retained":p==usb::ImportPhase::Uncertain?"Publication uncertain; keep operation":"Import failed; original retained",receipt_.status);
   }break;
  }
  case UsbUiPhase::Closing:closeStep();break;
  case UsbUiPhase::Opening:{
   if(app_.persistence.backgroundSaveActive()){
    const auto r=owner_.serviceSave();
    if(r.state==draftor::exchange::SaveState::Failed||
       r.state==draftor::exchange::SaveState::Partial){
     after_close_=AfterClose::StayOnCurrent;
     finish("Open failed; published draft retained",usb::Status::Io);
    }
    break;
   }
   if(app_.persistence.bodyDirty()){
    const auto r=app_.persistence.startAutoSave(now,app_.editorPort);
    if(r.status!=ds::Status::Busy&&!r){
     after_close_=AfterClose::StayOnCurrent;
     finish("Open save failed; new draft retained",usb::Status::Io);
    }
    break;
   }
   ds::DocumentId id;
   id.bytes=receipt_.document;
   const auto r=app_.openImported(id,now);
   after_close_=AfterClose::StayOnCurrent;
   if(r){
    receipt_.outcome=usb::Outcome::Opened;
    if(transfer_token_){
     selection_.release(transfer_token_,true);
     transfer_token_=0;
    }
    app_.leaveTransfer();
    change(UsbUiPhase::Idle,"");
   }else if(app_.session.currentId()==id){
    receipt_.outcome=usb::Outcome::Opened;
    finish("Opened; CURRENT update uncertain",usb::Status::Uncertain);
   }else{
    finish("Open failed; published draft retained",usb::Status::Io);
   }
   break;
  }
  default:break;
  }
 }catch(const std::bad_alloc&){
  if(phase_==UsbUiPhase::Opening){
   after_close_=AfterClose::StayOnCurrent;
   ds::DocumentId id;
   id.bytes=receipt_.document;
   if(app_.session.currentId()==id){
    receipt_.outcome=usb::Outcome::Opened;
    finish("Opened; memory low / state uncertain",usb::Status::Uncertain);
    return;
   }
  }
  const bool publishing=job_.phase()==usb::ImportPhase::Publishing;job_.cancel();if(publishing)receipt_=job_.result();releaseSource();
  finish(publishing?"Memory low; check publication receipt":"Memory low; original retained",publishing?receipt_.status:usb::Status::Full);
 }
}
const char* UsbTransfer::title()const noexcept{return phase_==UsbUiPhase::Menu?"USB TRANSFER":import_mode_?"USB IMPORT":"USB EXPORT";}
void UsbTransfer::line(unsigned row,char* out,std::size_t n)const noexcept{
 if(!n)return;out[0]=0;
 auto text=[&](const char* s){std::snprintf(out,n,"%s",s);};
 if(row==0){text(message_);return;}
 if(phase_==UsbUiPhase::Menu){if(row==2)text("E: USB export current TXT");if(row==3)text("I: USB import into new draft");if(row==6)text("Esc: return to writing");return;}
 if(phase_==UsbUiPhase::Exporting){if(row==2)text(export_.phase()==usb::ExportPhase::Published?"Finder: copy DOCUMENT.TXT":"Saving / preparing...");if(row==3)text("Read-only: no rename / delete");if(row==4)text("Eject in Finder when done");}
 if(phase_==UsbUiPhase::Receiving){if(row==2)text("Finder: drag TXT, then normal eject");if(row==3)text("Inbox is temporary RAM storage");if(row==4)text("Exit / power loss erases Inbox");}
 if(phase_==UsbUiPhase::Selecting){
  if(row==1)text(enumerated_?"Up/Down: choose  Enter: create":"Scanning received files...");
  if(row>=2&&row<=5){const auto first=selected_>=4?selected_-3:0;const auto i=first+row-2;if(i<candidates_.size())std::snprintf(out,n,"%c %s",i==selected_?'>':' ',candidates_[i].name.data());}
 }
 if(phase_==UsbUiPhase::Result){
  if(receipt_.outcome==usb::Outcome::Imported){if(row==2)std::snprintf(out,n,"Imported: %u bytes",unsigned(receipt_.bodyBytes));if(row==3)text("Enter: open new draft");if(row==4)text("Esc: continue original draft");}
  else if(row==2)std::snprintf(out,n,"Status %u  operation %llu",unsigned(status_),static_cast<unsigned long long>(receipt_.operation));
  if(row==3&&receipt_.status==usb::Status::Uncertain&&receipt_.operation)text("R: check same operation again");
  if(row==5)text("Computer source file is unchanged");
 }
 if(row==7)text(phase_==UsbUiPhase::Closing?"Wait for safe close":receipt_.outcome==usb::Outcome::Opened?"Esc: continue opened draft":"Esc: finish / return to original");
}
}
