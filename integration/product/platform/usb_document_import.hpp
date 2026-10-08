#pragma once
#include "draftor_exchange/document_transfer.hpp"
#include "src/hanime_document/external_create.hpp"

namespace s3app {
// Transport-independent adapter; neither Arduino nor USB/FAT types enter the
// product document transaction. The USB controller owns its input/storage gate.
class UsbDocumentImport final : public draftor::exchange::DocumentImportPort {
 public:
  UsbDocumentImport(hanime::documents::StoragePort& storage,
      hanime::documents::DocumentCatalog& catalog,
      hanime::documents::PersistenceService& persistence,
      hanime::documents::DocumentSession& session,
      hanime::documents::EditorPort& editor)
      :transaction_(storage,catalog,persistence,session,editor){}
  draftor::exchange::Status allocateOperation(std::uint64_t&) noexcept;
  draftor::exchange::Status reserve(const draftor::exchange::ImportIntent&,draftor::exchange::ImportResult&) noexcept override;
  draftor::exchange::Status begin(std::uint64_t) noexcept override;
  draftor::exchange::IoResult append(std::uint64_t,std::string_view) noexcept override;
  draftor::exchange::Status finish(std::uint64_t,std::size_t) noexcept override;
  draftor::exchange::Status publish(std::uint64_t) noexcept override;
  void abort(std::uint64_t) noexcept override;
  draftor::exchange::Status query(std::uint64_t,draftor::exchange::ImportResult&) noexcept override;
 private:
  static draftor::exchange::Status translate(hanime::documents::Status) noexcept;
  static void receipt(const hanime::documents::ExternalCreateReceipt&,draftor::exchange::ImportResult&);
  hanime::documents::ExternalCreateTransaction transaction_;
};
}
