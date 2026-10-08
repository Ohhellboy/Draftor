#include "external_create.hpp"
#include "paths.hpp"
#include <cstring>
#include <cstdio>
#include <limits>

namespace hanime::documents {
namespace {
bool good(Status s){return s==Status::Ok||s==Status::OkDegraded;}
draftor::memory::String receiptPath(std::uint64_t operation){
  char name[40];std::snprintf(name,sizeof(name),"/imports/%016llX",static_cast<unsigned long long>(operation));
  return draftor::memory::String(paths::kRoot)+name;
}
void put64(draftor::memory::String& bytes,std::uint64_t value){for(unsigned i=0;i<8;++i)bytes.push_back(char(value>>(i*8)));}
std::uint64_t get64(std::string_view bytes,std::size_t offset){std::uint64_t value=0;for(unsigned i=0;i<8;++i)value|=std::uint64_t(static_cast<unsigned char>(bytes[offset+i]))<<(i*8);return value;}
draftor::memory::String encode(const ExternalCreateReceipt& value){
  draftor::memory::String bytes;bytes.push_back(1);bytes.push_back(char(value.state));
  bytes.append(reinterpret_cast<const char*>(value.collection.bytes.data()),16);
  put64(bytes,value.body_bytes);put64(bytes,value.body_crc);bytes+=encodeCreateIntent(value.intent);return bytes;
}
Status decode(std::string_view bytes,ExternalCreateReceipt* out){
  if(!out||bytes.size()<36||bytes[0]!=1||static_cast<unsigned char>(bytes[1])>unsigned(ExternalCreateState::Aborted))return Status::Corrupt;
  ExternalCreateReceipt value;value.state=ExternalCreateState(static_cast<unsigned char>(bytes[1]));
  std::memcpy(value.collection.bytes.data(),bytes.data()+2,16);
  const auto count=get64(bytes,18),crc=get64(bytes,26);
  if(value.collection.empty()||count>kMaxDocumentBytes||crc>UINT32_MAX)return Status::Corrupt;
  value.body_bytes=std::size_t(count);value.body_crc=std::uint32_t(crc);
  const auto status=decodeCreateIntent(bytes.substr(34),&value.intent);
  if(status!=Status::Ok||!value.intent.active||!value.intent.external_operation)return Status::Corrupt;
  *out=std::move(value);return Status::Ok;
}
bool sourceMatches(const CreateIntent& intent,const ExternalCreateRequest& request){
  return intent.external_operation==request.operation&&intent.source_session==request.source_session&&intent.source_entry==request.source_entry&&intent.source_size==request.source_size;
}
}
ExternalCreateTransaction::~ExternalCreateTransaction(){if(owned_)abort(active_operation_);}
Status ExternalCreateTransaction::allocateOperation(std::uint64_t* operation){
  if(owned_||session_.busy()||persistence_.backgroundSaveActive())return Status::Busy;
  CreateIntent intent;std::uint64_t generation=0;
  const auto pending=catalog_.readCreateIntent(&intent,&generation);
  if(!good(pending))return pending;
  if(intent.active&&intent.external_operation){
    ExternalCreateReceipt prior;const auto recovered=query(intent.external_operation,&prior);
    if(recovered!=Status::Ok&&recovered!=Status::NotFound)return recovered;
    if(recovered==Status::NotFound){prior.intent=intent;prior.collection=catalog_.boundSession().collection_id;}
    if(prior.state!=ExternalCreateState::Aborted&&prior.state!=ExternalCreateState::Imported){
      // A new user request may abandon an old, detached, unpublished staging
      // operation. A durable Publishing operation was reconciled by query;
      // never reuse its ID or overwrite an already visible document.
      DocumentMetadata visible;const auto found=catalog_.get(intent.metadata.id,&visible);
      if(found!=Status::NotFound)return good(found)?Status::Conflict:found;
      prior.state=ExternalCreateState::Aborted;
      const auto marked=saveReceipt(prior);if(marked!=Status::Ok)return marked;
      const auto cleared=catalog_.clearExternalCreate(intent);if(cleared!=Status::Ok)return cleared;
      const auto base=paths::lane(intent.metadata.id,SaveLane::Auto);
      (void)storage_.remove(base+".A");(void)storage_.remove(base+".B");
    }
  }
  return catalog_.allocateExternalOperation(operation);
}
Status ExternalCreateTransaction::acquire(){
  if(owned_)return Status::Busy;
  const auto valid=catalog_.validateSession();if(valid!=Status::Ok)return valid;
  const auto status=session_.acquireExternalCreate();if(status!=Status::Ok)return status;
  owned_=true;bound_=catalog_.boundSession();original_=session_.currentId();
  session_serial_=session_.sessionSerial();revision_=editor_.revision();return Status::Ok;
}
Status ExternalCreateTransaction::valid(std::uint64_t operation)const noexcept{
  if(!owned_||operation!=active_operation_||operation!=receipt_.intent.external_operation)return Status::StaleToken;
  const auto status=catalog_.validateSession();if(status!=Status::Ok)return status;
  if(bound_.collection_id!=catalog_.boundSession().collection_id||bound_.mount_epoch!=catalog_.boundSession().mount_epoch||session_serial_!=session_.sessionSerial()||original_!=session_.currentId()||revision_!=editor_.revision())return Status::StaleToken;
  if(persistence_.backgroundSaveActive())return Status::Busy;
  return Status::Ok;
}
void ExternalCreateTransaction::release()noexcept{
  if(owned_&&writer_.active())(void)storage_.cooperativeClose();
  writer_={};draftor::memory::String{}.swap(body_);draftor::memory::String{}.swap(prefix_);
  phase_=Phase::Idle;if(owned_)session_.releaseExternalCreate();owned_=false;active_operation_=0;
}
Status ExternalCreateTransaction::loadReceipt(std::uint64_t operation,ExternalCreateReceipt* out,std::uint64_t* generation){
  if(!operation||!out)return Status::Conflict;
  const auto valid=catalog_.validateSession();if(valid!=Status::Ok)return valid;
  const auto loaded=records_.loadPair(receiptPath(operation),RecordKind::ExternalCreateReceipt,512);
  if(!good(loaded.status))return loaded.status;
  const auto parsed=decode(loaded.record.payload,out);if(parsed!=Status::Ok)return parsed;
  if(out->intent.external_operation!=operation||out->collection!=catalog_.boundSession().collection_id)return Status::SessionChanged;
  if(generation)*generation=loaded.record.generation;return Status::Ok;
}
Status ExternalCreateTransaction::saveReceipt(const ExternalCreateReceipt& receipt){
  ExternalCreateReceipt previous;std::uint64_t generation=0;
  const auto loaded=loadReceipt(receipt.intent.external_operation,&previous,&generation);
  if(loaded!=Status::NotFound&&loaded!=Status::Ok)return loaded;
  if(generation==UINT64_MAX)return Status::CapacityReached;
  return records_.commitPair(receiptPath(receipt.intent.external_operation),RecordKind::ExternalCreateReceipt,generation+1,encode(receipt));
}
Status ExternalCreateTransaction::reserve(const ExternalCreateRequest& request,ExternalCreateReceipt* out){
  if(!out||!request.operation||request.name.size()>kMaxNameBytes)return Status::Conflict;
  if(owned_)return Status::Busy;
  ExternalCreateReceipt stored;const auto loaded=loadReceipt(request.operation,&stored);
  if(loaded==Status::Ok){
    if(!sourceMatches(stored.intent,request))return Status::Conflict;
    if(stored.state==ExternalCreateState::Imported||stored.state==ExternalCreateState::Publishing)return query(request.operation,out);
    if(stored.state==ExternalCreateState::Verified){
      const auto checked=query(request.operation,out);if(checked!=Status::Ok)return checked;
      if(out->state==ExternalCreateState::Imported)return Status::Ok;
    }
    if(stored.state==ExternalCreateState::Aborted)return Status::Conflict;
    CreateIntent owner;std::uint64_t generation=0;const auto current=catalog_.readCreateIntent(&owner,&generation);
    if(!good(current))return current;
    // A fallback older receipt cannot allocate a replacement document after
    // its original intent was retired. Resolve or report the original ID.
    if(!owner.active||owner.external_operation!=request.operation||owner.metadata.id!=stored.intent.metadata.id)return Status::Conflict;
  }else if(loaded!=Status::NotFound)return loaded;
  const auto lock=acquire();if(lock!=Status::Ok)return lock;
  active_operation_=request.operation;
  CreateIntent identity;identity.external_operation=request.operation;identity.source_session=request.source_session;identity.source_entry=request.source_entry;identity.source_size=request.source_size;
  CreateIntent reserved;const auto status=catalog_.reserveExternalCreate(request.name,identity,&reserved);
  if(!good(status)){release();return status;}
  if(loaded==Status::Ok&&stored.intent.metadata.id!=reserved.metadata.id){release();return Status::Conflict;}
  receipt_=loaded==Status::Ok?stored:ExternalCreateReceipt{};
  receipt_.intent=reserved;receipt_.collection=bound_.collection_id;
  if(loaded==Status::NotFound){const auto saved=saveReceipt(receipt_);if(saved!=Status::Ok){release();return saved;}}
  phase_=Phase::Reserved;*out=receipt_;
  return Status::Ok;
}
Status ExternalCreateTransaction::begin(std::uint64_t operation){
  const auto status=valid(operation);if(status!=Status::Ok)return status;
  if(phase_!=Phase::Reserved)return Status::Conflict;
  body_.clear();body_.reserve(persistence_.bodyCapacity());phase_=Phase::Writing;return Status::Ok;
}
Status ExternalCreateTransaction::append(std::uint64_t operation,std::string_view bytes){
  const auto status=valid(operation);if(status!=Status::Ok)return status;
  if(phase_!=Phase::Writing)return Status::Conflict;
  if(bytes.size()>persistence_.bodyCapacity()-body_.size())return Status::TooLarge;
  body_.append(bytes.data(),bytes.size());return Status::Ok;
}
Status ExternalCreateTransaction::verifyBody(const ExternalCreateReceipt& receipt){
  auto loaded=persistence_.loadLatest(receipt.intent.metadata.id);
  if(!good(loaded.status))return loaded.status;
  if(loaded.snapshot.text.size()!=receipt.body_bytes||crc32(loaded.snapshot.text)!=receipt.body_crc)return Status::Corrupt;
  return Status::Ok;
}
Status ExternalCreateTransaction::finish(std::uint64_t operation,std::size_t bytes){
  const auto status=valid(operation);if(status!=Status::Ok)return status;
  if(phase_==Phase::Verified)return bytes==receipt_.body_bytes?Status::Ok:Status::Conflict;
  if(phase_==Phase::Writing){
    if(bytes!=body_.size())return Status::Conflict;
    if(!isValidUtf8(body_))return Status::InvalidUtf8;
    DocumentMetadata meta;const auto got=catalog_.get(receipt_.intent.metadata.id,&meta);
    if(got!=Status::NotFound)return good(got)?Status::Conflict:got;
    if((receipt_.state==ExternalCreateState::Verified||receipt_.state==ExternalCreateState::Publishing)&&
       (bytes!=receipt_.body_bytes||crc32(body_)!=receipt_.body_crc))return Status::Conflict;
    receipt_.body_bytes=bytes;receipt_.body_crc=crc32(body_);
    const auto reset=records_.resetUnrecoverablePair(paths::lane(receipt_.intent.metadata.id,SaveLane::Auto),RecordKind::Snapshot,kMaxDocumentBytes+128);
    if(reset!=Status::Ok)return reset;
    prefix_=encodeSnapshotPrefix(bytes,receipt_.body_crc,0,{});
    const std::string_view parts[]={prefix_,body_};
    const auto started=writer_.start(paths::lane(receipt_.intent.metadata.id,SaveLane::Auto),RecordKind::Snapshot,0,parts,2,storage_.mediumSession());
    if(started!=Status::Ok&&started!=Status::Busy)return started;
    phase_=Phase::BodyWriter;return Status::Busy;
  }
  if(phase_!=Phase::BodyWriter)return Status::Conflict;
  const auto progress=writer_.service(storage_);if(writer_.active())return Status::Busy;
  if(progress.status!=Status::Ok)return progress.status;
  // The cooperative writer verifies every stored byte against the immutable
  // supplied prefix/body; a second allocated full-body read is unnecessary.
  writer_={};receipt_.state=ExternalCreateState::Verified;
  const auto saved=saveReceipt(receipt_);if(saved!=Status::Ok)return saved;
  draftor::memory::String{}.swap(body_);prefix_.clear();phase_=Phase::Verified;return Status::Ok;
}
Status ExternalCreateTransaction::publish(std::uint64_t operation){
  const auto status=valid(operation);if(status!=Status::Ok)return status;
  if(phase_==Phase::Verified){
    receipt_.state=ExternalCreateState::Publishing;
    const auto marked=saveReceipt(receipt_);if(marked!=Status::Ok)return marked;
    prefix_=encodeMetadata(receipt_.intent.metadata);const std::string_view payload=prefix_;
    const auto started=writer_.start(paths::metadata(receipt_.intent.metadata.id),RecordKind::Metadata,0,&payload,1,storage_.mediumSession());
    if(started!=Status::Ok&&started!=Status::Busy)return started;
    phase_=Phase::MetadataWriter;return Status::Busy;
  }
  if(phase_!=Phase::MetadataWriter)return Status::Conflict;
  const auto progress=writer_.service(storage_);if(writer_.active())return Status::Busy;
  if(progress.status!=Status::Ok){catalog_.invalidateCache();release();return Status::Partial;}
  catalog_.publishedCreate(receipt_.intent,writer_.generation());writer_={};
  receipt_.state=ExternalCreateState::Imported;
  const auto saved=saveReceipt(receipt_);
  if(saved!=Status::Ok){release();return Status::Partial;}
  const auto cleared=catalog_.clearExternalCreate(receipt_.intent);release();
  return cleared==Status::Ok?Status::Ok:Status::Partial;
}
Status ExternalCreateTransaction::query(std::uint64_t operation,ExternalCreateReceipt* out){
  if(!out)return Status::Conflict;
  if(writer_.active())return Status::Busy;
  if(owned_){const auto status=valid(operation);return status==Status::Ok?queryOwned(operation,out):status;}
  const auto acquired=acquire();if(acquired!=Status::Ok)return acquired;
  try{const auto result=queryOwned(operation,out);release();return result;}
  catch(...){release();throw;}
}
Status ExternalCreateTransaction::queryOwned(std::uint64_t operation,ExternalCreateReceipt* out){
  const auto status=loadReceipt(operation,out);if(status!=Status::Ok)return status;
  if(out->state==ExternalCreateState::Verified){
    // A damaged newer receipt slot can fall back to Verified after META was
    // already published. The authenticated META is evidence of that step;
    // preserve and reconcile its existing ID instead of retrying creation.
    DocumentMetadata meta;catalog_.invalidateCache();const auto visible=catalog_.get(out->intent.metadata.id,&meta);
    if(good(visible))out->state=ExternalCreateState::Publishing;
    else if(visible!=Status::NotFound)return visible;
  }
  if(out->state==ExternalCreateState::Publishing){
    // Never mistake a currently visible but failed flush for a durable publish.
    // Re-write and verify the same metadata before issuing its durable receipt.
    catalog_.invalidateCache();
    DocumentMetadata metadata;const auto visible=catalog_.get(out->intent.metadata.id,&metadata);
    if(good(visible)){
      // A published draft can be opened/edited through the normal catalog
      // after reboot, before receipt recovery. Its valid later body does not
      // invalidate the original import; the receipt describes that operation.
      const auto body=persistence_.loadLatest(out->intent.metadata.id);if(!good(body.status))return body.status;
    }else{
      if(visible!=Status::NotFound&&visible!=Status::Corrupt)return visible;
      const auto body=verifyBody(*out);if(body!=Status::Ok)return body;
    }
    const auto published=catalog_.republishExternalMetadata(out->intent);if(published!=Status::Ok)return published;
    out->state=ExternalCreateState::Imported;const auto saved=saveReceipt(*out);if(saved!=Status::Ok)return saved;
  }
  if(out->state==ExternalCreateState::Imported||out->state==ExternalCreateState::Aborted){
    const auto clear=catalog_.clearExternalCreate(out->intent);
    // A later operation owns the intent; its ownership must remain untouched.
    if(clear!=Status::Ok&&clear!=Status::Conflict)return clear;
  }
  return Status::Ok;
}
void ExternalCreateTransaction::abort(std::uint64_t operation)noexcept{
  if(!owned_||operation!=active_operation_)return;
  try{
    if(writer_.active())(void)storage_.cooperativeClose();writer_={};
    if(catalog_.validateSession()==Status::Ok&&bound_.collection_id==catalog_.boundSession().collection_id&&bound_.mount_epoch==catalog_.boundSession().mount_epoch){
      // reserve may throw while allocating its metadata/receipt. The owner ID
      // is captured before those allocations, so cleanup still releases the
      // session and can only touch a matching durable external intent.
      if(receipt_.intent.external_operation!=operation||receipt_.intent.metadata.id.empty()){
        CreateIntent intent;std::uint64_t generation=0;
        if(!good(catalog_.readCreateIntent(&intent,&generation))||!intent.active||intent.external_operation!=operation){release();return;}
        receipt_={};receipt_.intent=std::move(intent);receipt_.collection=bound_.collection_id;
      }
      if(receipt_.state!=ExternalCreateState::Publishing&&receipt_.state!=ExternalCreateState::Imported){
        DocumentMetadata visible;catalog_.invalidateCache();
        if(catalog_.get(receipt_.intent.metadata.id,&visible)==Status::NotFound){
          receipt_.state=ExternalCreateState::Aborted;
          if(saveReceipt(receipt_)==Status::Ok)(void)catalog_.clearExternalCreate(receipt_.intent);
          // Only our unpublished AUTO slots are eligible for removal. A
          // visible or uncertain META always prevents destructive cleanup.
          const auto base=paths::lane(receipt_.intent.metadata.id,SaveLane::Auto);
          (void)storage_.remove(base+".A");(void)storage_.remove(base+".B");
        }
      }
    }
  }catch(...){/* Failure remains invisible or queryable; never touch CURRENT. */}
  release();
}
}
