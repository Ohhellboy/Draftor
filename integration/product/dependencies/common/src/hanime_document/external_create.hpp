#pragma once
#include "document_session.hpp"

namespace hanime::documents {
struct ExternalCreateRequest {
  std::uint64_t operation=0, source_session=0, source_entry=0, source_size=0;
  std::string_view name;
};
enum class ExternalCreateState : std::uint8_t { Reserved, Writing, Verified, Publishing, Imported, Aborted };
struct ExternalCreateReceipt {
  CreateIntent intent{};
  DocumentId collection{};
  ExternalCreateState state=ExternalCreateState::Reserved;
  std::size_t body_bytes=0;
  std::uint32_t body_crc=0;
};

// The DocumentSystem's one external initial-body transaction. This is not an
// editor installer: only DocumentSession::open may change the current document.
// The product serializes calls on its existing storage owner. append buffers at
// most B bytes; finish and publish service the existing 512-byte record writer.
class ExternalCreateTransaction {
 public:
  ExternalCreateTransaction(StoragePort& storage,DocumentCatalog& catalog,
      PersistenceService& persistence,DocumentSession& session,EditorPort& editor)
      :storage_(storage),catalog_(catalog),persistence_(persistence),session_(session),editor_(editor),records_(storage){}
  ~ExternalCreateTransaction();
  Status allocateOperation(std::uint64_t*);
  Status reserve(const ExternalCreateRequest&,ExternalCreateReceipt*);
  Status begin(std::uint64_t);
  Status append(std::uint64_t,std::string_view);
  Status finish(std::uint64_t,std::size_t);
  Status publish(std::uint64_t);
  void abort(std::uint64_t) noexcept;
  Status query(std::uint64_t,ExternalCreateReceipt*);
  bool active()const noexcept{return owned_;}
  std::size_t bufferedBytes()const noexcept{return body_.size();}
 private:
  enum class Phase { Idle,Reserved,Writing,BodyWriter,Verified,MetadataWriter };
  Status acquire();
  Status valid(std::uint64_t)const noexcept;
  void release()noexcept;
  Status loadReceipt(std::uint64_t,ExternalCreateReceipt*,std::uint64_t* generation=nullptr);
  Status saveReceipt(const ExternalCreateReceipt&);
  Status verifyBody(const ExternalCreateReceipt&);
  Status queryOwned(std::uint64_t,ExternalCreateReceipt*);
  StoragePort& storage_;DocumentCatalog& catalog_;PersistenceService& persistence_;
  DocumentSession& session_;EditorPort& editor_;RecordStore records_;
  ExternalCreateReceipt receipt_{};
  BoundSession bound_{};DocumentId original_{};
  std::uint64_t session_serial_=0,revision_=0;
  std::uint64_t active_operation_=0;
  bool owned_=false;
  Phase phase_=Phase::Idle;
  draftor::memory::String body_,prefix_;
  CooperativeRecordWriter writer_{};
};
}
