#include "../../core_memory.hpp"
#pragma once

#include "record_codec.hpp"
#include "record_store.hpp"
#include "cooperative_record_writer.hpp"

#include <cstddef>
#include <string_view>

namespace hanime::documents {

#ifndef HANIME_METADATA_CACHE_ENTRIES
#define HANIME_METADATA_CACHE_ENTRIES 128
#endif
class DocumentCatalog {
 public:
  DocumentCatalog(StoragePort& storage, IdSource& ids) noexcept
      : storage_(storage), ids_(ids), records_(storage) {}

  static constexpr size_t CacheEntries=HANIME_METADATA_CACHE_ENTRIES;
  static_assert(CacheEntries<=kMaxDocuments,"metadata cache exceeds document capacity");
  struct CacheStats {std::uint32_t builds=0,hits=0,invalidations=0;Status last_failure=Status::Ok;};
  mutable CacheStats cacheStats{};
  void invalidateCache() const noexcept;
  size_t cachedEntries() const noexcept { return cache_valid_?cache_.size():0; }
  size_t cacheBytes() const noexcept;
  Status initialize();
  bool initialized() const noexcept { return bound_.valid(); }
  const BoundSession& boundSession() const noexcept { return bound_; }
  const CollectionState& collection() const noexcept { return collection_; }

  Status validateSession() const noexcept;
  Status get(const DocumentId& id, DocumentMetadata* out);
  Status list(DocumentArea area, std::size_t offset,
              std::size_t page_size, ListPage* out);

  // Creation is deliberately split: reserve persists an id/name/order and an
  // intent; the session commits a verified blank AUTO before publish exposes
  // metadata. Repeating after interruption resumes the same intent.
  Status reserveCreate(std::string_view requested_name, CreateIntent* out);
  Status publishCreate(const CreateIntent& intent);
  Status allocateExternalOperation(std::uint64_t* operation);
  Status reserveExternalCreate(std::string_view name, const CreateIntent& identity, CreateIntent* out);
  Status readCreateIntent(CreateIntent* out, std::uint64_t* generation) { return loadIntent(out, generation); }
  Status clearExternalCreate(const CreateIntent&);
  Status republishExternalMetadata(const CreateIntent&);
  // The session owns one creation transaction; these prepare the existing
  // record format for the same cooperative writer used by background saves.
  enum class CreateRecord { Collection, Intent, Metadata, ClearIntent, Current };
  Status planCreate(std::string_view, CreateIntent*, std::uint64_t*, bool* resumed);
  Status startCreateRecord(CreateRecord, const CreateIntent&, std::uint64_t intent_generation,
                           CooperativeRecordWriter&, draftor::memory::String& payload);
  OperationResult serviceCreateRecord(CooperativeRecordWriter& writer) { return writer.service(storage_); }
  bool cooperativeCreateSupported() const noexcept { return storage_.cooperativeSupported(); }
  void abortCreateRecord() noexcept { (void)storage_.cooperativeClose();invalidateCache(); }
  void publishedCreate(CreateIntent& intent, std::uint64_t generation) {
    intent.metadata.metadata_generation=generation;cacheCommitted(intent.metadata);
  }

  Status rename(const DocumentId& id, std::string_view name);
  Status setArea(const DocumentId& id, DocumentArea area);
  Status touchOpened(const DocumentId& id);

  Status loadCurrent(CurrentRecord* out);
  Status commitCurrent(const CurrentRecord& value);

  static Status validateName(std::string_view name) noexcept;

 private:
  Status ensureCache();
  void cacheCommitted(const DocumentMetadata& value);
  static bool ordered(const DocumentMetadata& a,const DocumentMetadata& b) noexcept;
  mutable draftor::memory::Vector<DocumentMetadata> cache_;
  mutable bool cache_valid_=false,cache_overflow_=false;
  mutable Status cache_status_=Status::Ok;
  Status collectAfter(DocumentArea area, const DocumentMetadata* boundary,
                      std::size_t limit, draftor::memory::Vector<DocumentMetadata>* out,
                      bool* degraded, std::size_t* document_count = nullptr);
  Status nameExists(std::string_view name, bool* exists, const DocumentId& exclude = {});
  Status loadIntent(CreateIntent* out, std::uint64_t* generation);
  Status commitCollection();
  Status commitMetadata(DocumentMetadata value);
  Status planCreateOwned(std::string_view, CreateIntent*, std::uint64_t*, bool*, const CreateIntent&);

  StoragePort& storage_;
  IdSource& ids_;
  RecordStore records_;
  CollectionState collection_{};
  BoundSession bound_{};
};

}  // namespace hanime::documents
