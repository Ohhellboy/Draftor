#include "../../core_memory.hpp"
#include "document_catalog.hpp"

#include "paths.hpp"

#include <algorithm>
#include <cstdio>

namespace hanime::documents {

Status DocumentCatalog::validateName(const std::string_view name) noexcept {
  if (name.empty() || name.size() > kMaxNameBytes || !isValidUtf8(name)) return Status::InvalidName;
  for (const unsigned char ch : name) {
    if (ch < 0x20U || ch == 0x7FU || ch == '/' || ch == '\\') return Status::InvalidName;
  }
  return Status::Ok;
}

Status DocumentCatalog::validateSession() const noexcept {
  const MediumSession now = storage_.mediumSession();
  if (!now.available) {cacheStats.last_failure=Status::MediaUnavailable;invalidateCache();return Status::MediaUnavailable;}
  if (!bound_.valid() || now.mount_epoch != bound_.mount_epoch) {cacheStats.last_failure=Status::SessionChanged;invalidateCache();return Status::SessionChanged;}
  return Status::Ok;
}

Status DocumentCatalog::initialize() {
  invalidateCache();
  const MediumSession medium = storage_.mediumSession();
  if (!medium.available) return Status::MediaUnavailable;
  PairLoad loaded = records_.loadPair(paths::kCollection, RecordKind::Collection, 128U);
  if (loaded.status == Status::NotFound) {
    CollectionState fresh{};
    fresh.collection_id = ids_.next({}, 1U);
    if (fresh.collection_id.empty()) return Status::Conflict;
    fresh.id_counter = 1U;
    fresh.generation = 1U;
    const Status saved = records_.commitPair(paths::kCollection, RecordKind::Collection,
                                             fresh.generation, encodeCollection(fresh));
    if (saved != Status::Ok) return saved;
    collection_ = fresh;
  } else if (loaded.status == Status::Ok || loaded.status == Status::OkDegraded) {
    const Status decoded = decodeCollection(loaded.record.payload, &collection_);
    if (decoded != Status::Ok) return decoded;
    collection_.generation = loaded.record.generation;
  } else {
    return loaded.status;
  }
  if (storage_.mediumSession().mount_epoch != medium.mount_epoch) return Status::SessionChanged;
  bound_.collection_id = collection_.collection_id;
  bound_.mount_epoch = medium.mount_epoch;
  return loaded.status == Status::OkDegraded ? Status::OkDegraded : Status::Ok;
}

Status DocumentCatalog::get(const DocumentId& id, DocumentMetadata* out) {
  if (out == nullptr || id.empty()) return Status::NotFound;
  const Status session = validateSession(); if (session != Status::Ok) return session;
  const PairLoad loaded = records_.loadPair(paths::metadata(id), RecordKind::Metadata, 256U);
  if (loaded.status != Status::Ok && loaded.status != Status::OkDegraded) {
    if(loaded.status!=Status::NotFound||std::any_of(cache_.begin(),cache_.end(),[&](const auto& meta){return meta.id==id;}))invalidateCache();
    return loaded.status;
  }
  DocumentMetadata value{};
  const Status decoded = decodeMetadata(loaded.record.payload, &value);
  if (decoded != Status::Ok || value.id != id) {invalidateCache();return decoded == Status::Ok ? Status::Corrupt : decoded;}
  value.metadata_generation = loaded.record.generation;
  *out = std::move(value);
  return loaded.status;
}

bool DocumentCatalog::ordered(const DocumentMetadata& a,const DocumentMetadata& b) noexcept {
 return a.opened_order!=b.opened_order?a.opened_order>b.opened_order:a.id<b.id;
}
void DocumentCatalog::invalidateCache() const noexcept {
 if(cache_valid_||!cache_.empty())++cacheStats.invalidations;
 draftor::memory::Vector<DocumentMetadata> empty;cache_.swap(empty);cache_valid_=cache_overflow_=false;cache_status_=Status::Ok;
}
size_t DocumentCatalog::cacheBytes() const noexcept {
 size_t bytes=cache_.capacity()*sizeof(DocumentMetadata);
 for(const auto& meta:cache_)if(meta.name.capacity()>draftor::memory::String{}.capacity())bytes+=meta.name.capacity()+1;
 return bytes;
}
Status DocumentCatalog::ensureCache(){
 auto valid=validateSession();if(valid!=Status::Ok)return valid;
 if(cache_valid_){++cacheStats.hits;return cache_status_;}
 if(!CacheEntries||cache_overflow_||!storage_.documentEnumerationSupported())return Status::TooLarge;
 ++cacheStats.builds;
 // Local builder remains separate from published cache. A failed scan publishes nothing.
 struct Build {DocumentCatalog* owner;draftor::memory::Vector<DocumentMetadata> items;bool overflow=false,degraded=false;};
 Build build{this,{},false,false};build.items.reserve(CacheEntries);
 auto status=storage_.visitDocumentIds(&build,[](void* context,const DocumentId& id){
  auto& b=*static_cast<Build*>(context);DocumentMetadata meta;auto got=b.owner->get(id,&meta);
  if(got==Status::NotFound)return Status::Ok; // abandoned unpublished create directory
  if(got!=Status::Ok&&got!=Status::OkDegraded)return got;
  b.degraded|=got==Status::OkDegraded;
  if(b.items.size()==CacheEntries){b.overflow=true;return Status::CapacityReached;}
  b.items.push_back(std::move(meta));return Status::Ok;
 });
 if(build.overflow){invalidateCache();cache_overflow_=true;return Status::TooLarge;}
 if(status!=Status::Ok&&status!=Status::OkDegraded){cacheStats.last_failure=status;invalidateCache();return status;}
 std::sort(build.items.begin(),build.items.end(),ordered);cache_=std::move(build.items);cache_valid_=true;cache_status_=build.degraded?Status::OkDegraded:Status::Ok;return cache_status_;
}
void DocumentCatalog::cacheCommitted(const DocumentMetadata& value){
 if(!cache_valid_)return;
 try {
 auto it=std::find_if(cache_.begin(),cache_.end(),[&](const auto& m){return m.id==value.id;});
 if(it!=cache_.end())*it=value;
 else if(cache_.size()<CacheEntries)cache_.push_back(value);
 else {invalidateCache();return;}
 std::sort(cache_.begin(),cache_.end(),ordered);
 }catch(const std::bad_alloc&){invalidateCache();} // optional cache must not undo a committed META result
}

Status DocumentCatalog::list(const DocumentArea area,
                             const std::size_t offset,
                             const std::size_t page_size,
                             ListPage* out) {
  if (out == nullptr || page_size == 0U || page_size > kMaxListPage) return Status::Conflict;
  const Status session = validateSession(); if (session != Status::Ok) return session;
  const auto cached=ensureCache();
  if(cached==Status::Ok||cached==Status::OkDegraded){
   ListPage page;page.offset=offset;page.degraded=cached==Status::OkDegraded;size_t skipped=0;
   for(const auto& meta:cache_)if(meta.area==area){
    if(skipped++<offset)continue;
    if(page.items.size()==page_size){page.has_more=true;break;}
    page.items.push_back(meta);
   }
   *out=std::move(page);return cached;
  }
  if(cached!=Status::TooLarge)return cached;
  bool degraded = false;
  DocumentMetadata boundary{};
  bool has_boundary = false;
  std::size_t remaining = offset;
  while (remaining != 0U) {
    const std::size_t step = std::min(remaining, kMaxListPage);
    draftor::memory::Vector<DocumentMetadata> skipped;
    const Status scanned = collectAfter(area, has_boundary ? &boundary : nullptr,
                                        step, &skipped, &degraded);
    if (scanned != Status::Ok && scanned != Status::OkDegraded) return scanned;
    if (skipped.size() < step) {
      ListPage empty{}; empty.offset = offset; empty.degraded = degraded;
      *out = std::move(empty);
      return degraded ? Status::OkDegraded : Status::Ok;
    }
    boundary = skipped.back(); has_boundary = true; remaining -= step;
  }
  draftor::memory::Vector<DocumentMetadata> selected;
  const Status scanned = collectAfter(area, has_boundary ? &boundary : nullptr,
                                      page_size + 1U, &selected, &degraded);
  if (scanned != Status::Ok && scanned != Status::OkDegraded) return scanned;
  ListPage page{}; page.offset = offset; page.degraded = degraded;
  page.has_more = selected.size() > page_size;
  if (page.has_more) selected.resize(page_size);
  page.items = std::move(selected);
  *out = std::move(page);
  return degraded ? Status::OkDegraded : Status::Ok;
}

Status DocumentCatalog::collectAfter(const DocumentArea area,
                                     const DocumentMetadata* boundary,
                                     const std::size_t limit,
                                     draftor::memory::Vector<DocumentMetadata>* out,
                                     bool* degraded,
                                     std::size_t* document_count) {
  if (!out || !degraded || limit == 0U) return Status::Conflict;
  out->clear(); out->reserve(limit);
  auto before = [](const DocumentMetadata& a, const DocumentMetadata& b) {
    if (a.opened_order != b.opened_order) return a.opened_order > b.opened_order;
    return a.id < b.id;
  };
  if(storage_.documentEnumerationSupported()){
   struct Scan {DocumentCatalog* owner;DocumentArea area;const DocumentMetadata* boundary;size_t limit,count=0;draftor::memory::Vector<DocumentMetadata>* out;bool* degraded;};
   Scan scan{this,area,boundary,limit,0,out,degraded};
   auto status=storage_.visitDocumentIds(&scan,[](void* context,const DocumentId& id){
    auto& x=*static_cast<Scan*>(context);DocumentMetadata meta;auto got=x.owner->get(id,&meta);
    if(got==Status::NotFound)return Status::Ok;
    if(got!=Status::Ok&&got!=Status::OkDegraded)return got;
    if(++x.count>kMaxDocuments)return Status::CapacityReached;
    *x.degraded|=got==Status::OkDegraded;
    if(meta.area!=x.area||(x.boundary&&!ordered(*x.boundary,meta)))return Status::Ok;
    auto at=std::lower_bound(x.out->begin(),x.out->end(),meta,ordered);x.out->insert(at,std::move(meta));if(x.out->size()>x.limit)x.out->pop_back();return Status::Ok;
   });
   if(document_count)*document_count=scan.count;
   if(status!=Status::Ok&&status!=Status::OkDegraded){cacheStats.last_failure=status;invalidateCache();return status;}
   return *degraded?Status::OkDegraded:Status::Ok;
  }
  draftor::memory::String cursor;
  DocumentId last_id{};
  bool have_last_id = false;
  std::size_t count = 0U;
  for (;;) {
    const ListResult listed = storage_.list(paths::kDocsPrefix, cursor, 32U);
    if (listed.status != Status::Ok) return listed.status;
    for (const draftor::memory::String& path : listed.paths) {
      cursor = path;
      if (path.size() < paths::kDocsPrefix.size() + 38U ||
          path.compare(0U, paths::kDocsPrefix.size(), paths::kDocsPrefix) != 0) continue;
      const std::string_view tail(path.data() + paths::kDocsPrefix.size(),
                                  path.size() - paths::kDocsPrefix.size());
      if (tail.size() < 38U || tail[32] != '/' ||
          (tail.substr(33U) != "META.A" && tail.substr(33U) != "META.B")) continue;
      DocumentId id{};
      if (!DocumentId::fromHex(tail.substr(0U, 32U), &id)) { *degraded = true; continue; }
      if (have_last_id && id == last_id) continue;
      last_id = id; have_last_id = true; ++count;
      if (count > kMaxDocuments) return Status::CapacityReached;
      DocumentMetadata meta{};
      const Status got = get(id, &meta);
      if (got != Status::Ok && got != Status::OkDegraded) return got;
      *degraded = *degraded || got == Status::OkDegraded;
      if (meta.area != area || (boundary && !before(*boundary, meta))) continue;
      const auto at = std::lower_bound(out->begin(), out->end(), meta, before);
      out->insert(at, std::move(meta));
      if (out->size() > limit) out->pop_back();
    }
    if (!listed.has_more) break;
    if (listed.paths.empty()) return Status::IoError;
  }
  if (document_count) *document_count = count;
  return *degraded ? Status::OkDegraded : Status::Ok;
}

Status DocumentCatalog::nameExists(const std::string_view name, bool* exists, const DocumentId& exclude) {
  if (!exists) return Status::Conflict;
  *exists = false;
  const auto cached=ensureCache();
  if(cached==Status::Ok||cached==Status::OkDegraded){
   for(const auto& meta:cache_)if(meta.area==DocumentArea::Normal&&meta.id!=exclude&&meta.name==name){*exists=true;break;}
   return cached;
  }
  if(cached!=Status::TooLarge&&!(cached==Status::Corrupt&&!exclude.empty()))return cached;
  if(storage_.documentEnumerationSupported()){
   struct Names {DocumentCatalog* owner;std::string_view name;DocumentId exclude;bool* exists;};Names names{this,name,exclude,exists};
   auto status=storage_.visitDocumentIds(&names,[](void* context,const DocumentId& id){
    auto& x=*static_cast<Names*>(context);if(id==x.exclude)return Status::Ok;DocumentMetadata meta;auto got=x.owner->get(id,&meta);if(got==Status::NotFound)return Status::Ok;
    if(got!=Status::Ok&&got!=Status::OkDegraded)return got;
    if(meta.area==DocumentArea::Normal&&id!=x.exclude&&meta.name==x.name)*x.exists=true;
    return Status::Ok;
   });
   if(status!=Status::Ok&&status!=Status::OkDegraded) { invalidateCache(); }
   return status;
  }
  for (const DocumentArea area : {DocumentArea::Normal}) {
    std::size_t offset = 0U;
    for (;;) {
      ListPage page{};
      const Status listed = list(area, offset, kMaxListPage, &page);
      if (listed != Status::Ok && listed != Status::OkDegraded) return listed;
      for (const DocumentMetadata& meta : page.items) {
        if (meta.id != exclude && meta.name == name) { *exists = true; return Status::Ok; }
      }
      if (!page.has_more) break;
      offset += page.items.size();
    }
  }
  return Status::Ok;
}

Status DocumentCatalog::commitCollection() {
  if (collection_.generation == UINT64_MAX) return Status::Conflict;
  ++collection_.generation;
  return records_.commitPair(paths::kCollection, RecordKind::Collection,
                             collection_.generation, encodeCollection(collection_));
}

Status DocumentCatalog::loadIntent(CreateIntent* out, std::uint64_t* generation) {
  if (!out || !generation) return Status::Conflict;
  const PairLoad loaded = records_.loadPair(paths::kCreateIntent, RecordKind::CreateIntent, 320U);
  if (loaded.status == Status::NotFound) { *out = {}; *generation = 0U; return Status::Ok; }
  if (loaded.status != Status::Ok && loaded.status != Status::OkDegraded) return loaded.status;
  const Status decoded = decodeCreateIntent(loaded.record.payload, out);
  if (decoded != Status::Ok) return decoded;
  *generation = loaded.record.generation;
  return loaded.status;
}

Status DocumentCatalog::planCreate(const std::string_view requested_name, CreateIntent* out, std::uint64_t* generation, bool* resumed) {
  return planCreateOwned(requested_name, out, generation, resumed, {});
}
Status DocumentCatalog::planCreateOwned(const std::string_view requested_name, CreateIntent* out, std::uint64_t* generation, bool* resumed, const CreateIntent& owner) {
  if (!out || !generation || !resumed) return Status::Conflict;
  *resumed=false;
  const Status session = validateSession(); if (session != Status::Ok) return session;
  CreateIntent existing{}; std::uint64_t intent_generation = 0;
  const Status intent_status = loadIntent(&existing, &intent_generation);
  if (intent_status != Status::Ok && intent_status != Status::OkDegraded) return intent_status;
  *generation=intent_generation;
  if (existing.active) {
    if (existing.external_operation != owner.external_operation) return Status::Busy;
    if (owner.external_operation && (existing.source_session != owner.source_session || existing.source_entry != owner.source_entry || existing.source_size != owner.source_size)) return Status::Conflict;
    DocumentMetadata published;const auto got=get(existing.metadata.id,&published);
    if(got==Status::Ok||got==Status::OkDegraded)existing.metadata=std::move(published);
    else {
      if(got!=Status::NotFound&&got!=Status::Corrupt)return got;
      bool duplicate=false;const auto checked=nameExists(existing.metadata.name,&duplicate,existing.metadata.id);
      if(checked!=Status::Ok&&checked!=Status::OkDegraded)return checked;
      if(duplicate)return Status::NameExists;
    }
    *out=existing;*resumed=true;return Status::OkDegraded;
  }

  draftor::memory::Vector<DocumentMetadata> scratch;
  bool degraded = false;
  std::size_t document_count = 0U;
  auto counted=ensureCache();
  if(counted==Status::Ok||counted==Status::OkDegraded)document_count=cache_.size();
  else if(counted==Status::TooLarge)counted=collectAfter(DocumentArea::Normal,nullptr,1U,&scratch,&degraded,&document_count);
  if (counted != Status::Ok && counted != Status::OkDegraded) return counted;
  if (document_count >= kMaxDocuments) return Status::CapacityReached;

  draftor::memory::String name(requested_name);
  if (name.empty()) {
    bool duplicate = false;
    do {
      char buffer[40]{};
      const std::uint64_t number = collection_.temp_name_counter + 1U;
      std::snprintf(buffer, sizeof(buffer), "Draft %03llu", static_cast<unsigned long long>(number));
      name = buffer;
      collection_.temp_name_counter = number;
      const Status checked = nameExists(name, &duplicate);
      if (checked != Status::Ok && checked != Status::OkDegraded) return checked;
    } while (duplicate);
  }
  const Status name_status = validateName(name); if (name_status != Status::Ok) return name_status;

  bool duplicate=false;const auto checked=nameExists(name,&duplicate);
  if(checked!=Status::Ok&&checked!=Status::OkDegraded)return checked;
  if(duplicate)return Status::NameExists;
  DocumentId id{};
  for (unsigned attempt = 0; attempt < 32U; ++attempt) {
    ++collection_.id_counter;
    id = ids_.next(collection_.collection_id, collection_.id_counter);
    if (id.empty()) continue;
    const ListResult collision = storage_.list(paths::documentRoot(id), {}, 1U);
    if (collision.status != Status::Ok) return collision.status;
    if (collision.paths.empty()) break;
    id = {};
  }
  if (id.empty()) return Status::Conflict;
  ++collection_.opened_order_counter;

  CreateIntent intent{}; intent.active = true; intent.metadata.id = id; intent.metadata.name = std::move(name);
  intent.external_operation=owner.external_operation; intent.source_session=owner.source_session;
  intent.source_entry=owner.source_entry; intent.source_size=owner.source_size;
  intent.metadata.area = DocumentArea::Normal; intent.metadata.opened_order = collection_.opened_order_counter;
  *out = intent;
  return Status::Ok;
}

Status DocumentCatalog::reserveCreate(std::string_view name, CreateIntent* out) {
  std::uint64_t generation=0;bool resumed=false;
  const auto planned=planCreate(name,out,&generation,&resumed);
  if(planned!=Status::Ok&&planned!=Status::OkDegraded)return planned;
  if(resumed)return planned;
  const auto saved=commitCollection();if(saved!=Status::Ok)return saved;
  return records_.commitPair(paths::kCreateIntent,RecordKind::CreateIntent,generation+1,encodeCreateIntent(*out));
}

Status DocumentCatalog::allocateExternalOperation(std::uint64_t* operation) {
  if (!operation) return Status::Conflict;
  const auto valid=validateSession(); if(valid!=Status::Ok)return valid;
  if(collection_.id_counter==UINT64_MAX)return Status::CapacityReached;
  ++collection_.id_counter;
  const auto saved=commitCollection(); if(saved==Status::Ok)*operation=collection_.id_counter;
  return saved;
}
Status DocumentCatalog::reserveExternalCreate(std::string_view name,const CreateIntent& identity,CreateIntent* out) {
  if(!identity.external_operation)return Status::Conflict;
  std::uint64_t generation=0;bool resumed=false;
  const auto planned=planCreateOwned(name,out,&generation,&resumed,identity);
  if(planned!=Status::Ok&&planned!=Status::OkDegraded)return planned;
  if(resumed)return planned;
  const auto saved=commitCollection();if(saved!=Status::Ok)return saved;
  return records_.commitPair(paths::kCreateIntent,RecordKind::CreateIntent,generation+1,encodeCreateIntent(*out));
}
Status DocumentCatalog::clearExternalCreate(const CreateIntent& intent) {
  const auto valid=validateSession();if(valid!=Status::Ok)return valid;
  CreateIntent current;std::uint64_t generation=0;
  const auto loaded=loadIntent(&current,&generation);if(loaded!=Status::Ok&&loaded!=Status::OkDegraded)return loaded;
  if(!current.active)return Status::Ok;
  if(!intent.external_operation||current.external_operation!=intent.external_operation||current.metadata.id!=intent.metadata.id)return Status::Conflict;
  return records_.commitPair(paths::kCreateIntent,RecordKind::CreateIntent,generation+1,encodeCreateIntent({}));
}
Status DocumentCatalog::republishExternalMetadata(const CreateIntent& intent) {
  const auto valid=validateSession();if(valid!=Status::Ok)return valid;
  if(!intent.active||!intent.external_operation)return Status::Conflict;
  DocumentMetadata existing;const auto got=get(intent.metadata.id,&existing);
  if(got==Status::Corrupt||got==Status::NotFound){
    CreateIntent current;std::uint64_t generation=0;const auto loaded=loadIntent(&current,&generation);
    if((loaded!=Status::Ok&&loaded!=Status::OkDegraded)||!current.active||current.external_operation!=intent.external_operation||current.metadata.id!=intent.metadata.id)return Status::Conflict;
    if(got==Status::Corrupt){
      const auto reset=records_.resetUnrecoverablePair(paths::metadata(intent.metadata.id),RecordKind::Metadata,256);
      if(reset!=Status::Ok)return reset;
    }
  }else if(got!=Status::Ok&&got!=Status::OkDegraded&&got!=Status::NotFound)return got;
  // Once META is visible, the normal file manager may rename, open, or trash
  // the draft before a lost import receipt is recovered. Re-acknowledge that
  // authenticated current metadata, never overwrite it with the initial name.
  auto meta=(got==Status::Ok||got==Status::OkDegraded)?existing:intent.metadata;
  if(got==Status::Corrupt){
    const auto header=records_.peekPair(paths::metadata(intent.metadata.id),RecordKind::Metadata,256);
    if(header.status==Status::Ok||header.status==Status::OkDegraded)meta.metadata_generation=header.generation;
  }
  return commitMetadata(std::move(meta));
}

Status DocumentCatalog::startCreateRecord(CreateRecord record,const CreateIntent& intent,
    std::uint64_t intent_generation,CooperativeRecordWriter& writer,draftor::memory::String& payload) {
  const auto valid=validateSession();if(valid!=Status::Ok)return valid;
  draftor::memory::String base;RecordKind kind=RecordKind::Metadata;std::uint64_t generation=0;
  switch(record){
  case CreateRecord::Collection:
    if(collection_.generation==UINT64_MAX)return Status::Conflict;
    generation=++collection_.generation;kind=RecordKind::Collection;base=paths::kCollection;payload=encodeCollection(collection_);break;
  case CreateRecord::Intent:
    generation=intent_generation+1;kind=RecordKind::CreateIntent;base=paths::kCreateIntent;payload=encodeCreateIntent(intent);break;
  case CreateRecord::Metadata:{
    DocumentMetadata existing;auto got=get(intent.metadata.id,&existing);
    if(got==Status::Ok||got==Status::OkDegraded)return Status::Ok; // resumed published intent, preserve metadata
    if(got==Status::Corrupt){
      // Only the durable active create intent authorizes repairing its own
      // unfinished metadata pair. Never reset an established document here.
      const auto raw=records_.loadPair(paths::metadata(intent.metadata.id),RecordKind::Metadata,256);
      if(raw.status!=Status::Corrupt) {
        return raw.status==Status::Ok||raw.status==Status::OkDegraded ? got : raw.status;
      }
      CreateIntent owner;std::uint64_t owner_generation=0;
      const auto owned=loadIntent(&owner,&owner_generation);
      if(owned!=Status::Ok&&owned!=Status::OkDegraded)return owned;
      if(!intent.active||!owner.active||owner.metadata.id!=intent.metadata.id||
         owner.metadata.name!=intent.metadata.name||owner.metadata.area!=intent.metadata.area||
         owner.metadata.opened_order!=intent.metadata.opened_order)return Status::Conflict;
      const auto reset=records_.resetUnrecoverablePair(paths::metadata(intent.metadata.id),RecordKind::Metadata,256);
      if(reset!=Status::Ok)return reset;
    }else if(got!=Status::NotFound)return got;
    // Zero requests the writer's existing max-header-generation + 1 rule.
    // A valid header does not prove a complete META payload: keep its generation
    // occupied and publish to the alternate slot. An absent pair still starts at 1.
    generation=0;kind=RecordKind::Metadata;base=paths::metadata(intent.metadata.id);payload=encodeMetadata(intent.metadata);break;}
  case CreateRecord::ClearIntent:
    generation=intent_generation+2;kind=RecordKind::CreateIntent;base=paths::kCreateIntent;payload=encodeCreateIntent({});break;
  case CreateRecord::Current:{
    CurrentRecord current;current.has_current=true;current.document_id=intent.metadata.id;
    kind=RecordKind::Current;base=paths::kCurrent;payload=encodeCurrent(current);break;}
  }
  const std::string_view part=payload;
  return writer.start(std::move(base),kind,generation,&part,1,storage_.mediumSession());
}

Status DocumentCatalog::publishCreate(const CreateIntent& intent) {
  if (!intent.active || intent.metadata.id.empty()) return Status::Conflict;
  const Status session = validateSession(); if (session != Status::Ok) return session;
  DocumentMetadata existing{}; const Status got = get(intent.metadata.id, &existing);
  if (got != Status::Ok && got != Status::OkDegraded && got != Status::NotFound) return got;
  if (got == Status::NotFound) {
    const Status meta = commitMetadata(intent.metadata); if (meta != Status::Ok) return meta;
  }
  CreateIntent current{}; std::uint64_t generation = 0; const Status loaded = loadIntent(&current, &generation);
  if (loaded != Status::Ok && loaded != Status::OkDegraded) return loaded;
  if (current.active && current.metadata.id != intent.metadata.id) return Status::Conflict;
  CreateIntent cleared{};
  const Status clear = records_.commitPair(paths::kCreateIntent, RecordKind::CreateIntent,
                                           generation + 1U, encodeCreateIntent(cleared));
  return clear == Status::Ok ? Status::Ok : Status::Partial;
}

Status DocumentCatalog::commitMetadata(DocumentMetadata value) {
  if (validateName(value.name) != Status::Ok || value.id.empty()) return Status::InvalidName;
  std::uint64_t generation = value.metadata_generation;
  if (generation == 0U) {
    const PairLoad loaded = records_.loadPair(paths::metadata(value.id), RecordKind::Metadata, 256U);
    if (loaded.status == Status::Ok || loaded.status == Status::OkDegraded) generation = loaded.record.generation;
    else if (loaded.status != Status::NotFound) return loaded.status;
  }
  const auto payload = encodeMetadata(value);
  const Status written = records_.commitPair(paths::metadata(value.id), RecordKind::Metadata,
                             generation + 1U, payload);
  if (written == Status::Ok) {value.metadata_generation=generation+1U;cacheCommitted(value);return written;}
  invalidateCache();
  // A flush/close error may follow a visible commit. Reconcile metadata only;
  // Partial means visible now, durability was not fully acknowledged.
  const auto check = records_.loadPair(paths::metadata(value.id), RecordKind::Metadata, 256U);
  if ((check.status == Status::Ok || check.status == Status::OkDegraded) &&
      check.record.generation == generation+1U && check.record.payload == payload) return Status::Partial;
  return written;
}

Status DocumentCatalog::rename(const DocumentId& id, const std::string_view name) {
  const Status valid = validateName(name); if (valid != Status::Ok) return valid;
  DocumentMetadata meta{}; const Status got = get(id, &meta); if (got != Status::Ok && got != Status::OkDegraded) return got;
  if (meta.name == name) return Status::Ok;
  bool duplicate=false;const Status checked=nameExists(name,&duplicate,id);
  if (checked!=Status::Ok&&checked!=Status::OkDegraded)return checked;
  if (duplicate)return Status::NameExists;
  meta.name.assign(name.data(), name.size()); return commitMetadata(std::move(meta));
}

Status DocumentCatalog::setArea(const DocumentId& id, const DocumentArea area) {
  DocumentMetadata meta{}; const Status got = get(id, &meta); if (got != Status::Ok && got != Status::OkDegraded) return got;
  if (meta.area == area) return Status::Ok;
  if(area==DocumentArea::Normal){
    const draftor::memory::String original=meta.name;
    bool duplicate=false;
    for(std::size_t suffix=0;suffix<=kMaxDocuments;++suffix){
      if(suffix){
        char tail[24];std::snprintf(tail,sizeof(tail),"-%02u",unsigned(suffix));
        const auto tail_size=std::char_traits<char>::length(tail);
        std::size_t prefix=std::min(original.size(),kMaxNameBytes-tail_size);
        while(prefix&&!isUtf8Boundary(original,prefix))--prefix;
        meta.name.assign(original.data(),prefix);meta.name+=tail;
      }
      const auto checked=nameExists(meta.name,&duplicate,id);
      if(checked!=Status::Ok&&checked!=Status::OkDegraded)return checked;
      if(!duplicate)break;
      if(suffix==kMaxDocuments)return Status::NameExists;
    }
  }
  meta.area = area;
  return commitMetadata(std::move(meta));
}

Status DocumentCatalog::touchOpened(const DocumentId& id) {
  DocumentMetadata meta{}; const Status got = get(id, &meta); if (got != Status::Ok && got != Status::OkDegraded) return got;
  ++collection_.opened_order_counter; const Status saved = commitCollection(); if (saved != Status::Ok) return saved;
  meta.opened_order = collection_.opened_order_counter; return commitMetadata(std::move(meta));
}

Status DocumentCatalog::loadCurrent(CurrentRecord* out) {
  if (!out) return Status::Conflict;
  const Status session = validateSession();
  if (session != Status::Ok) return session;
  const PairLoad loaded = records_.loadPair(paths::kCurrent, RecordKind::Current, 64U);
  if (loaded.status != Status::Ok && loaded.status != Status::OkDegraded) return loaded.status;
  const Status decoded = decodeCurrent(loaded.record.payload, out); if (decoded == Status::Ok) out->generation = loaded.record.generation; return decoded;
}

Status DocumentCatalog::commitCurrent(const CurrentRecord& value) {
  const Status session = validateSession(); if (session != Status::Ok) return session;
  CurrentRecord old{}; const Status loaded = loadCurrent(&old); std::uint64_t generation = 0U;
  if (loaded == Status::Ok || loaded == Status::OkDegraded) generation = old.generation;
  else if (loaded != Status::NotFound) return loaded;
  return records_.commitPair(paths::kCurrent, RecordKind::Current,
                             generation + 1U, encodeCurrent(value));
}

}  // namespace hanime::documents
