#include "usb_document_import.hpp"
#include <cstring>

namespace s3app {
namespace ds=hanime::documents;namespace ex=draftor::exchange;
ex::Status UsbDocumentImport::translate(ds::Status status)noexcept{
  switch(status){
  case ds::Status::Ok:case ds::Status::OkDegraded:return ex::Status::Ok;
  case ds::Status::Busy:case ds::Status::CompositionActive:return ex::Status::Busy;
  case ds::Status::Partial:return ex::Status::Uncertain;
  case ds::Status::NotFound:return ex::Status::NotFound;
  case ds::Status::MediaUnavailable:case ds::Status::SessionChanged:case ds::Status::StaleToken:return ex::Status::Stale;
  case ds::Status::TooLarge:return ex::Status::TooLarge;
  case ds::Status::InvalidUtf8:return ex::Status::InvalidText;
  case ds::Status::CapacityReached:return ex::Status::Full;
  case ds::Status::ResourceExhausted:case ds::Status::InstallFailed:return ex::Status::Exhausted;
  case ds::Status::Conflict:case ds::Status::InvalidName:case ds::Status::NameExists:return ex::Status::Conflict;
  default:return ex::Status::Io;
  }
}
void UsbDocumentImport::receipt(const ds::ExternalCreateReceipt& source,ex::ImportResult& out){
  out={};out.status=ex::Status::Ok;out.operation=source.intent.external_operation;
  out.document=source.intent.metadata.id.bytes;out.collection=source.collection.bytes;out.bodyBytes=source.body_bytes;
  switch(source.state){
  case ds::ExternalCreateState::Reserved:out.outcome=ex::Outcome::Reserved;break;
  case ds::ExternalCreateState::Writing:out.outcome=ex::Outcome::Writing;break;
  case ds::ExternalCreateState::Verified:case ds::ExternalCreateState::Publishing:out.outcome=ex::Outcome::Verified;break;
  case ds::ExternalCreateState::Imported:out.outcome=ex::Outcome::Imported;break;
  case ds::ExternalCreateState::Aborted:out.outcome=ex::Outcome::Aborted;break;
  }
}
ex::Status UsbDocumentImport::allocateOperation(std::uint64_t& operation)noexcept{
  try{return translate(transaction_.allocateOperation(&operation));}catch(...){return ex::Status::Exhausted;}
}
ex::Status UsbDocumentImport::reserve(const ex::ImportIntent& intent,ex::ImportResult& out)noexcept{
  try{
    if(intent.name.back()!=0)return ex::Status::InvalidArgument;
    std::string_view name(intent.name.data(),std::strlen(intent.name.data()));
    if(name.size()>4){const auto suffix=name.substr(name.size()-4);if(suffix==".txt"||suffix==".TXT")name.remove_suffix(4);}
    if(ds::DocumentCatalog::validateName(name)!=ds::Status::Ok)name={};
    ds::ExternalCreateRequest request{intent.operation,intent.source.session,intent.source.entry,intent.source.size,name};
    ds::ExternalCreateReceipt result;auto status=transaction_.reserve(request,&result);
    if(status==ds::Status::NameExists){request.name={};status=transaction_.reserve(request,&result);}
    if(status==ds::Status::Ok||status==ds::Status::OkDegraded)receipt(result,out);
    return translate(status);
  }catch(...){transaction_.abort(intent.operation);return ex::Status::Exhausted;}
}
ex::Status UsbDocumentImport::begin(std::uint64_t operation)noexcept{
  try{return translate(transaction_.begin(operation));}catch(...){transaction_.abort(operation);return ex::Status::Exhausted;}
}
ex::IoResult UsbDocumentImport::append(std::uint64_t operation,std::string_view bytes)noexcept{
  try{const auto status=translate(transaction_.append(operation,bytes));return {status,status==ex::Status::Ok?bytes.size():0};}catch(...){transaction_.abort(operation);return {ex::Status::Exhausted,0};}
}
ex::Status UsbDocumentImport::finish(std::uint64_t operation,std::size_t bytes)noexcept{
  try{return translate(transaction_.finish(operation,bytes));}catch(...){transaction_.abort(operation);return ex::Status::Exhausted;}
}
ex::Status UsbDocumentImport::publish(std::uint64_t operation)noexcept{
  try{
    const auto status=translate(transaction_.publish(operation));
    // A terminal publication error retires the live writer/owner, while abort
    // preserves Publishing/META. The durable operation can then be queried.
    if(status!=ex::Status::Ok&&status!=ex::Status::Busy)transaction_.abort(operation);
    return status;
  }catch(...){transaction_.abort(operation);return ex::Status::Uncertain;}
}
void UsbDocumentImport::abort(std::uint64_t operation)noexcept{transaction_.abort(operation);}
ex::Status UsbDocumentImport::query(std::uint64_t operation,ex::ImportResult& out)noexcept{
  try{ds::ExternalCreateReceipt result;const auto status=transaction_.query(operation,&result);if(status==ds::Status::Ok)receipt(result,out);return translate(status);}catch(...){return ex::Status::Uncertain;}
}
}
