#include "board_namespace.hpp"
#include <cstring>
#include <new>
namespace draftor::board {
static bool prepareNamespaceImpl(const s3fs::Fs& card){
 const auto kind=card.kind(NamespacePath);
 if(kind==s3fs::PathKind::Missing){
  if(!card.mkdir(NamespacePath))return false;
  auto marker=card.openFile(NamespaceMarker,s3fs::Write);
  if(!marker)return false;
  const auto bytes=sizeof(NamespaceIdentity)-1;
  const bool written=marker.write(reinterpret_cast<const std::uint8_t*>(NamespaceIdentity),bytes)==bytes;
  const bool synced=written&&marker.flush();const bool closed=marker.close();
  return written&&synced&&closed;
 }
 if(kind!=s3fs::PathKind::Directory)return false;
 auto marker=card.openFile(NamespaceMarker);
 if(!marker)return false;
 char value[sizeof(NamespaceIdentity)]{};
 const bool exact=marker.size()==sizeof(NamespaceIdentity)-1&&marker.read(reinterpret_cast<std::uint8_t*>(value),sizeof(NamespaceIdentity)-1)==int(sizeof(NamespaceIdentity)-1)&&!std::memcmp(value,NamespaceIdentity,sizeof(NamespaceIdentity)-1);
 return marker.close()&&exact;
}
NamespaceResult prepareNamespace(const char* cardRoot) noexcept{
 try{
  const s3fs::Fs card(cardRoot);
  return prepareNamespaceImpl(card)?NamespaceResult::Ready:NamespaceResult::Invalid;
 }catch(const std::bad_alloc&){
  return NamespaceResult::OutOfMemory;
 }
}
}
