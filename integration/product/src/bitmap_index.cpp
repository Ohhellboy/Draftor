#include "personal_index.hpp"
#include "hanime/score_policy.hpp"
#include "r2_storage.hpp"
#include <cerrno>
#include "bitmap_dimensions.hpp"
#include "tf_resources.hpp"
#include "relation_pin.hpp"
#include "base_order_wire.hpp"
#include <algorithm>
#include <array>
#include <bitset>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#ifndef BGIME_COUNT_SUPPLY
#define BGIME_COUNT_SUPPLY 1
#endif
namespace r1 {
using namespace hanime;using namespace ime_device;
using detail::bo32;using detail::bo64;using detail::put32;using detail::put64;
namespace {
constexpr uint64_t fnv0=14695981039346656037ULL;
uint64_t fnv(uint64_t h,const uint8_t* p,size_t n){for(size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ULL;}return h;}
bool better(const RankItem&a,const RankItem&b){return a.score>b.score||(a.score==b.score&&a.key<b.key);}
bool same(const StateView&a,const StateView&b){return a.owner==b.owner&&a.count_revision==b.count_revision&&a.availability_revision==b.availability_revision&&a.registry_namespace==b.registry_namespace;}
uint32_t allowed_mask(size_t word){for(size_t s=0;s<4;++s)if(word+1==BitmapOffsets[s+1]&&BitmapCounts[s]%32)return (uint32_t(1)<<(BitmapCounts[s]%32))-1;return UINT32_MAX;}
}
struct PersonalIndex::Impl : r2::ExternalObject {
 r2::FileOps storage;
 BoundedUserState& authority;Resources& resources;std::string file;size_t limit,range_limit;
 IndexCost stats;std::string failure;bool loaded=false,loading=false,imported=false,saving=false,save_checkpoint=false,save_rewrite=false,checkpoint_needed=false,file_present=false;
 r2::Vector<uint32_t> bits,frozen_bits;
 std::array<uint32_t,DirtyWordLimit> dirty_words{};std::bitset<BitmapWords> dirty_set;size_t dirty_count=0;
 std::array<uint8_t,128> header{},save_header{},last_header{};size_t last_offset=0;
 std::array<uint8_t,1024> chunk{};
 std::array<uint8_t,DirtyWordLimit*8> frame{};size_t frame_bytes=0;
 int fd=-1;unsigned io_phase=0,save_phase=0;size_t io_at=0,file_size=0,base_size=0,journal_bytes=0,frame_at=0,frame_size=0;
 uint64_t hash=fnv0,expected_hash=0,token=0,saved_sequence=UINT64_MAX,saved_generation=0,saved_fingerprint=0,saved_keys=0;
 uint64_t save_sequence=0,save_generation=0,save_fingerprint=0,save_keys=0,save_token=0;
 size_t save_bytes=0,save_at=0,save_base=0,marked_refs=0,marked_members=0,save_refs=0,save_members=0;
 std::shared_ptr<TfReader> data,proof;std::shared_ptr<VerifiedResourceReader> reader;
 ResourceReadContext read_context{std::make_shared<const int>(0),1};
 std::array<uint8_t,128> relation_header{};std::array<uint8_t,8> span{};std::array<uint8_t,64*8> association_bytes{};
 uint32_t edge_begin=0,edge_count=0;size_t edge_at=0;
 struct Pending {RuntimeLexemeRef ref;StableLexemeKey key;};std::deque<Pending,r2::ExternalAllocator<Pending>> todo;
 enum class Add {Idle,SharedBegin,SharedRead,OpenData,OpenProof,Reader,Boot,Span,Rows,Mark,Publish} add=Add::Idle;
 SharedReadCursor shared_cursor;SharedActivationHandle add_activation;
 StateView view;SharedActivationHandle activation;BaseOrderIndex* order=nullptr;BaseOrderCursor* cursor=nullptr;
 std::string code;uint32_t scope=0,start=0,N=0,sid=0;RankRule rule;uint64_t rev=1;bool active=false,local_fallback=false;
 enum class Query {Words,Fact,CountStart,Count,BatchStart,BatchRun,BatchFill,SortLoad,SortMove,Anchor,Merge,Output,Done} query=Query::Done;
 size_t word_at=0,word_end=0,di=0,sort_i=0,sort_j=0;uint32_t pending_word=0,pending_base=0;
 r2::Vector<RankItem> corrections,prefix;r2::Vector<uint32_t> positions;
 RankItem held,base;CountTicket ticket;bool base_ready=false;size_t base_pos=0,skip_pos=0,dpos=0;
 RangeDirection direction=RangeDirection::First;StableLexemeKey anchor;int64_t anchor_score=0;
 size_t q=0,page_start=0,page_end=0,output=0,eligible=0,anchor_search=0;
 explicit Impl(BoundedUserState&a,Resources&r,std::string f,size_t cap,size_t rc):storage(r.mount()),authority(a),resources(r),file(std::move(f)),limit(cap),range_limit(std::min<size_t>(512,rc)){}
 ~Impl(){if(fd>=0)storage.close(fd);}
 bool fail(const char* e){failure=e;if(fd>=0){const int old=fd;fd=-1;storage.close(old);}r2::Vector<uint32_t>().swap(frozen_bits);r2::Vector<uint32_t>().swap(bits);r2::Vector<uint32_t>().swap(positions);r2::Vector<RankItem>().swap(corrections);r2::Vector<RankItem>().swap(prefix);todo.clear();reader.reset();data.reset();proof.reset();return false;}
 size_t bytes()const{return (bits.capacity()+frozen_bits.capacity()+positions.capacity())*4+(corrections.capacity()+prefix.capacity())*sizeof(RankItem)+65536+sizeof(Impl)+authority.counts_memory();}
 bool memory(){stats.memory=bytes();stats.peak=std::max(stats.peak,stats.memory);return stats.memory<=limit||fail("R11_MEMORY_LIMIT");}
 template<class T>bool reserve(r2::Vector<T>& v,size_t n){if(n<=v.capacity())return true;if(bytes()>limit||(n-v.capacity())>(limit-bytes())/sizeof(T))return fail("R11_MEMORY_LIMIT");try{v.reserve(n);}catch(const std::bad_alloc&){return fail("R11_ALLOCATION_FAILED");}return memory();}
 bool ensure_bits(){if(!bits.empty())return true;if(!reserve(bits,BitmapWords))return false;bits.resize(BitmapWords,0);return true;}
 bool state_idle()const{auto h=authority.health();auto i=authority.state_backend().info();return !h.dirty&&!h.reserved&&!h.intent_pending&&!h.frozen_write&&!h.batches_pending&&!i.maintaining&&!i.write_pending;}
 bool bound()const{return storage.current()&&resources.shared.ready()&&resources.shared.info().shared_bundle_id==relation_pin.shared_bundle_id&&resources.shared.verified_release()&&resources.shared.verified_release()->file_sha256==relation_pin.required_shared_sha256;}
 bool state_saved()const{auto i=authority.state_backend().info();return i.sequence==saved_sequence&&i.generation==saved_generation&&i.content_fingerprint==saved_fingerprint&&i.snapshot_keys==saved_keys;}
 bool needed()const{return !loaded||!todo.empty()||add!=Add::Idle||saving||checkpoint_needed||dirty_count||!state_saved();}
 void reset_query(){authority.cancel_count(ticket);ticket={};corrections.clear();positions.clear();prefix.clear();base_pos=skip_pos=dpos=0;base_ready=false;di=0;word_at=start/32;word_end=(uint64_t(start)+N+31)/32;pending_word=0;query=Query::Words;}
 bool header_ok(const std::array<uint8_t,128>& h,const char* magic){return !std::memcmp(h.data(),magic,6)&&bo32(h.data()+8)==11&&bo64(h.data()+16)==relation_pin.registry_namespace&&bo64(h.data()+24)==relation_pin.shared_bundle_id&&std::equal(relation_pin.file_sha256.begin(),relation_pin.file_sha256.end(),h.begin()+80)&&fnv(fnv0,h.data(),120)==bo64(h.data()+120);}
 void take_certificate(size_t offset){saved_sequence=bo64(header.data()+32);saved_generation=bo64(header.data()+40);saved_fingerprint=bo64(header.data()+64);saved_keys=bo64(header.data()+72);token=bo64(header.data()+120);last_header=header;last_offset=offset;}
 bool read_at(void* dst,size_t n,size_t at){auto got=storage.read(fd,dst,n,at);++stats.reads;stats.read_bytes+=std::max<ssize_t>(0,got);return got==ssize_t(n)||fail("BITMAP_READ_SHORT");}
 bool load_step(){
  if(loaded)return true;if(!ensure_bits())return false;
  if(!loading){++stats.controls;fd=storage.open(file,O_RDONLY);if(fd<0){if(errno!=ENOENT)return fail("INDEX_OPEN_ERROR");struct stat pending{};if(storage.stat(file+".pending",&pending)==0||errno!=ENOENT)return fail("INDEX_ORPHAN_PENDING");auto i=authority.state_backend().info();if(i.snapshot_keys||i.tail_frames)return fail("INDEX_COVERAGE_MISSING");loaded=true;checkpoint_needed=true;return true;}loading=true;io_phase=0;io_at=0;hash=fnv0;}
  if(io_phase==0){if(!read_at(header.data(),128,0))return false;if(!header_ok(header,"BGP311"))return fail("BITMAP_HEADER_BINDING");
   const auto size=bo32(header.data()+12);if(size!=0&&size!=BitmapBytes)return fail("BITMAP_DIMENSIONS");base_size=128+size;
   struct stat st{};++stats.controls;if(storage.size(fd,&st)||st.st_size<ssize_t(base_size)||st.st_size>ssize_t(base_size+JournalLimit))return fail("BITMAP_FILE_SIZE");file_size=size_t(st.st_size);expected_hash=bo64(header.data()+56);io_phase=1;return true;}
  if(io_phase==1){if(io_at<base_size-128){size_t n=std::min<size_t>(1024,base_size-128-io_at);if(!read_at(chunk.data(),n,128+io_at))return false;hash=fnv(hash,chunk.data(),n);for(size_t x=0;x<n;x+=4){size_t w=(io_at+x)/4;auto v=bo32(chunk.data()+x);if(v&~allowed_mask(w))return fail("BITMAP_PADDING");bits[w]=v;++stats.load_words;}io_at+=n;return true;}
   if(hash!=expected_hash)return fail("BITMAP_BODY_CHECKSUM");take_certificate(0);frame_at=base_size;io_phase=2;return true;}
  if(io_phase==2){if(frame_at==file_size){io_phase=4;io_at=0;marked_refs=marked_members=0;return true;}
   if(file_size-frame_at<128)return fail("JOURNAL_INCOMPLETE");if(!read_at(header.data(),128,frame_at))return false;
   if(!header_ok(header,"BGC311")||bo64(header.data()+112)!=token)return fail("JOURNAL_HEADER_CHAIN");
   const size_t n=bo32(header.data()+48);frame_size=bo32(header.data()+12);if(n>DirtyWordLimit||frame_size!=128+n*8||frame_size>file_size-frame_at)return fail("JOURNAL_INCOMPLETE");
   expected_hash=bo64(header.data()+56);hash=fnv0;io_at=0;io_phase=3;return true;}
  if(io_phase==3){if(io_at<frame_size-128){auto n=std::min<size_t>(1024,frame_size-128-io_at);if(!read_at(chunk.data(),n,frame_at+128+io_at))return false;hash=fnv(hash,chunk.data(),n);
    for(size_t x=0;x<n;x+=8){auto w=bo32(chunk.data()+x),v=bo32(chunk.data()+x+4);if(w>=BitmapWords||(v&~allowed_mask(w))||(bits[w]&~v))return fail("JOURNAL_WORD");bits[w]=v;++stats.load_words;}io_at+=n;return true;}
   if(hash!=expected_hash)return fail("JOURNAL_BODY_CHECKSUM");take_certificate(frame_at);frame_at+=frame_size;io_phase=2;return true;}
  if(io_phase==4){for(size_t n=0;n<64&&io_at<BitmapWords;++n,++io_at){auto count=__builtin_popcount(bits[io_at]);if(io_at<BitmapOffsets[3])marked_members+=count;else marked_refs+=count;}
   if(io_at<BitmapWords)return true;if(!state_saved())return fail("INDEX_STATE_GAP");
   ++stats.controls;{const int old=fd;fd=-1;if(storage.close(old))return fail("BITMAP_LOAD_CLOSE");}loaded=true;loading=false;file_present=true;journal_bytes=file_size-base_size;if(active&&!local_fallback)reset_query();return memory();}
  return false;
 }
 void mark_bit(size_t word,uint32_t bit,bool ref){if(bits[word]&bit)return;bits[word]|=bit;if(ref){++marked_refs;++stats.ref_bit_updates;}else{++marked_members;++stats.bit_updates;++stats.relations_added;}
  if(!dirty_set[word]){dirty_set[word]=true;if(dirty_count<DirtyWordLimit)dirty_words[dirty_count++]=uint32_t(word);else checkpoint_needed=true;}}
 void make_header(const char* magic,uint32_t length,uint32_t n,uint64_t body_hash,uint64_t previous){save_header={};std::memcpy(save_header.data(),magic,6);put32(save_header.data()+8,11);put32(save_header.data()+12,length);put64(save_header.data()+16,relation_pin.registry_namespace);put64(save_header.data()+24,relation_pin.shared_bundle_id);put64(save_header.data()+32,save_sequence);put64(save_header.data()+40,save_generation);put32(save_header.data()+48,n);put32(save_header.data()+52,uint32_t(save_members));put64(save_header.data()+56,body_hash);put64(save_header.data()+64,save_fingerprint);put64(save_header.data()+72,save_keys);std::copy(relation_pin.file_sha256.begin(),relation_pin.file_sha256.end(),save_header.begin()+80);put64(save_header.data()+112,previous);put64(save_header.data()+120,fnv(fnv0,save_header.data(),120));save_token=bo64(save_header.data()+120);}
 bool begin_save(){if(!state_idle()||!todo.empty()||imported)return true;const auto i=authority.state_backend().info();save_sequence=i.sequence;save_generation=i.generation;save_fingerprint=i.content_fingerprint;save_keys=i.snapshot_keys;
  save_refs=marked_refs;save_members=marked_members;frame_bytes=dirty_count*8;
  save_rewrite=file_present&&!checkpoint_needed&&!dirty_count;
  save_checkpoint=!save_rewrite&&(checkpoint_needed||!file_present||journal_bytes+128+frame_bytes>JournalLimit);
  if(save_rewrite){
   // Coverage and its payload are unchanged. Replace only the terminal
   // certificate; its predecessor link and payload digest remain unchanged.
   // A torn replacement is rejected on reopen, never treated as Count0.
   save_header=last_header;put64(save_header.data()+32,save_sequence);put64(save_header.data()+40,save_generation);put64(save_header.data()+64,save_fingerprint);put64(save_header.data()+72,save_keys);put64(save_header.data()+120,fnv(fnv0,save_header.data(),120));save_token=bo64(save_header.data()+120);save_bytes=0;save_base=last_offset;save_phase=1;save_at=0;
  }
  else if(save_checkpoint){save_bytes=marked_refs||marked_members?BitmapBytes:0;if(save_bytes){if(!reserve(frozen_bits,BitmapWords))return false;frozen_bits=bits;stats.snapshot_copy_bytes+=BitmapBytes;}save_base=0;hash=fnv0;save_at=0;save_phase=0;}
  else{for(size_t j=0;j<dirty_count;++j){auto w=dirty_words[j];put32(frame.data()+j*8,w);put32(frame.data()+j*8+4,bits[w]);}save_bytes=frame_bytes;save_base=base_size+journal_bytes;make_header("BGC311",uint32_t(128+frame_bytes),uint32_t(dirty_count),fnv(fnv0,frame.data(),frame_bytes),token);save_phase=1;save_at=0;}
  // A later Accepted changes live bits/dirty words, never this frozen version.
  dirty_count=0;dirty_set.reset();checkpoint_needed=false;saving=true;return memory();
 }
 void serialize_chunk(size_t at,size_t n){for(size_t x=0;x<n;x+=4)put32(chunk.data()+x,frozen_bits[(at+x)/4]);}
 bool write_at(const void* p,size_t n,size_t at){auto got=storage.write(fd,p,n,at);++stats.writes;stats.write_bytes+=std::max<ssize_t>(0,got);return got==ssize_t(n)||fail("BITMAP_SAVE_SHORT");}
 bool save_step(){
  if(!saving)return begin_save();
  if(save_phase==0){if(save_at<save_bytes){auto n=std::min<size_t>(1024,save_bytes-save_at);serialize_chunk(save_at,n);hash=fnv(hash,chunk.data(),n);save_at+=n;return true;}make_header("BGP311",uint32_t(save_bytes),uint32_t(save_refs),hash,0);save_phase=1;save_at=0;return true;}
  if(save_phase==1){++stats.controls;fd=storage.open((save_checkpoint?file+".pending":file),O_WRONLY|(save_checkpoint?O_CREAT|O_TRUNC:0),0600);if(fd<0)return fail("BITMAP_SAVE_OPEN");save_phase=2;return true;}
  if(save_phase==2){if(!write_at(save_header.data(),128,save_base))return false;save_phase=3;return true;}
  if(save_phase==3){if(save_at<save_bytes){auto n=std::min<size_t>(1024,save_bytes-save_at);const uint8_t* p=frame.data()+save_at;if(save_checkpoint){serialize_chunk(save_at,n);p=chunk.data();}if(!write_at(p,n,save_base+128+save_at))return false;save_at+=n;return true;}save_phase=4;return true;}
  if(save_phase==4){++stats.syncs;++stats.controls;if(storage.sync(fd))return fail("BITMAP_SYNC");save_phase=5;return true;}
  if(save_phase==5){++stats.controls;{const int old=fd;fd=-1;if(storage.close(old))return fail("BITMAP_CLOSE");}
   if(save_checkpoint){++stats.controls;if(storage.replace(file+".pending",file))return fail("BITMAP_RENAME");base_size=128+save_bytes;journal_bytes=0;file_present=true;++stats.checkpoints;r2::Vector<uint32_t>().swap(frozen_bits);}
   else if(save_rewrite)++stats.certificate_rewrites;
   else{journal_bytes+=128+save_bytes;++stats.journal_commits;}
   saved_sequence=save_sequence;saved_generation=save_generation;saved_fingerprint=save_fingerprint;saved_keys=save_keys;token=save_token;last_header=save_header;last_offset=save_base;saving=false;return memory();}
  return false;
 }
 bool add_step(){
  if(todo.empty())return true;auto& item=todo.front();
  if(!bound())return fail("RELATION_RESOURCE_BINDING");
  if(add!=Add::Idle&&!resources.shared.activation_is_current(add_activation))return fail("RELATION_ACTIVATION_CHANGED");
  if(add==Add::Idle){if(!item.ref||item.ref>BitmapCounts[3])return fail("RELATION_REF_BOUNDS");auto p=item.ref-1;if(bits[BitmapOffsets[3]+p/32]&(uint32_t(1)<<(p%32))){++stats.existing_updates;todo.pop_front();return true;}add_activation=resources.shared.activation_handle();add=Add::SharedBegin;}
  if(add==Add::SharedBegin){if(!bound())return fail("RELATION_RESOURCE_BINDING");if(resources.shared.begin_read(SharedRecordKind::Lexeme,item.ref,&shared_cursor)!=SharedReadStatus::InProgress)return fail("RELATION_SHARED_BEGIN");add=Add::SharedRead;return true;}
  if(add==Add::SharedRead){auto s=resources.shared.advance_read(&shared_cursor,{1,1024,1024});if(s.status==SharedReadStatus::InProgress)return true;if(s.status!=SharedReadStatus::Ready||s.lexeme.identity.stable_key!=item.key)return fail("RELATION_SHARED_IDENTITY");add=reader?Add::Span:Add::OpenData;return true;}
  if(add==Add::OpenData){data=std::make_shared<TfReader>(resources.mount(),std::string(R2_RELATION_ROOT)+"/relations.bgr",0);if(!data->valid())return fail("RELATION_DATA_OPEN");add=Add::OpenProof;return true;}
  if(add==Add::OpenProof){proof=std::make_shared<TfReader>(resources.mount(),std::string(R2_RELATION_ROOT)+"/relations.hnv",0);if(!proof->valid())return fail("RELATION_PROOF_OPEN");add=Add::Reader;return true;}
  if(add==Add::Reader){reader=std::make_shared<VerifiedResourceReader>(data,proof,relation_pin,VerifiedResourceConfig{4,8,1024});if(!reader->prepare_bootstrap())return fail("RELATION_READER");add=Add::Boot;return true;}
  if(add==Add::Boot){auto s=reader->advance_bootstrap({1,1024,1024});if(s.status==ResourceReadStatus::Pending)return true;if(s.status!=ResourceReadStatus::Ready||!reader->read(0,relation_header.data(),128)||std::memcmp(relation_header.data(),"BGR301",6))return fail("RELATION_BOOTSTRAP");add=Add::Span;return true;}
  if(add==Add::Span){auto s=reader->read_budgeted(1024+size_t(item.ref-1)*4,span.data(),8,{1,1024,1024},read_context);if(s.status==ResourceReadStatus::Pending)return true;if(s.status!=ResourceReadStatus::Ready)return fail("RELATION_SPAN_AUTH");edge_begin=bo32(span.data());auto end=bo32(span.data()+4);if(end<edge_begin||end>326795||end-edge_begin>64)return fail("RELATION_SPAN_BOUNDS");edge_count=end-edge_begin;add=Add::Rows;return true;}
  if(add==Add::Rows){if(edge_count){auto s=reader->read_budgeted(1024+971920+size_t(edge_begin)*8,association_bytes.data(),edge_count*8,{1,1024,1024},read_context);if(s.status==ResourceReadStatus::Pending)return true;if(s.status!=ResourceReadStatus::Ready)return fail("RELATION_ROWS_AUTH");}
   for(size_t j=0;j<edge_count;++j){auto scheme=bo32(association_bytes.data()+j*8)>>30,fact=bo32(association_bytes.data()+j*8+4);if(scheme>=3||fact>=BitmapCounts[scheme])return fail("RELATION_POSITION_BOUNDS");}edge_at=0;add=Add::Mark;return true;}
  if(add==Add::Mark){for(size_t n=0;n<16&&edge_at<edge_count;++n,++edge_at){auto scheme=bo32(association_bytes.data()+edge_at*8)>>30,fact=bo32(association_bytes.data()+edge_at*8+4);mark_bit(BitmapOffsets[scheme]+fact/32,uint32_t(1)<<(fact%32),false);}if(edge_at==edge_count)add=Add::Publish;return true;}
  if(add==Add::Publish){auto p=item.ref-1;mark_bit(BitmapOffsets[3]+p/32,uint32_t(1)<<(p%32),true);todo.pop_front();add=Add::Idle;++stats.new_identities;if(active&&!local_fallback)reset_query();return memory();}
  return false;
 }
 bool background(){if(!failure.empty())return false;if(!storage.current())return fail("INDEX_MOUNT_RETIRED");if(!loaded)return load_step();if(!todo.empty())return add_step();if(saving)return save_step();if(!imported&&needed())return begin_save();return true;}
 bool query_current()const{return storage.current()&&active&&authority.view_is_current(view)&&resources.shared.activation_is_current(activation)&&order&&order->ready();}
 SparseStep next(const WorkBudget& b);
};

SparseStep PersonalIndex::Impl::next(const WorkBudget& b){
 SparseStep out;if(!query_current()){out.status=SparseStatus::Stale;return out;}if(!failure.empty()||local_fallback){out.status=SparseStatus::Fallback;return out;}if(!b.max_work_items||b.should_stop())return out;
 if(!loaded||!todo.empty()){
  ++stats.coverage_waits;if(b.max_work_items<1024||b.max_read_bytes<1024||!b.max_read_calls)return out;
  auto before=stats;auto phys=metrics.io();if(!loaded)load_step();else add_step();auto after=metrics.io();out.cost={size_t(after.reads-phys.reads+stats.reads-before.reads),size_t(after.bytes-phys.bytes+stats.read_bytes-before.read_bytes),1024};if(!failure.empty())out.status=SparseStatus::Fallback;return out;
 }
 // A frozen persistence version is independent of this complete RAM view.
 for(size_t n=0;n<b.max_work_items&&!b.should_stop();++n){if(!query_current()){out.status=SparseStatus::Stale;return out;}++out.cost.work_items;
  if(query==Query::Words){
   if(pending_word){auto bit=uint32_t(__builtin_ctz(pending_word));pending_word&=pending_word-1;if(!reserve(positions,positions.size()+1)){out.status=SparseStatus::Fallback;return out;}positions.push_back(pending_base+bit-start);continue;}
   if(word_at<word_end){pending_base=uint32_t(word_at*32);pending_word=bits[BitmapOffsets[sid]+word_at];++stats.bitmap_words;
    if(pending_base<start)pending_word&=UINT32_MAX<<(start-pending_base);auto end=uint64_t(start)+N;if(end<pending_base+32)pending_word&=(uint32_t(1)<<(end-pending_base))-1;++word_at;continue;}
   if(!reserve(corrections,positions.size())||!reserve(prefix,N)){out.status=SparseStatus::Fallback;return out;}di=0;query=Query::Fact;continue;
  }
  if(query==Query::Fact){if(di==positions.size()){sort_i=1;query=BGIME_COUNT_SUPPLY&&positions.size()>1?Query::BatchStart:Query::SortLoad;continue;}--out.cost.work_items;auto s=order->read_item(*cursor,positions[di],{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,b.max_work_items-out.cost.work_items});out.cost.read_calls+=s.cost.read_calls;out.cost.read_bytes+=s.cost.read_bytes;out.cost.work_items+=s.cost.work_items;if(s.status==BaseOrderStatus::Ready){held=s.item;++stats.facts;++stats.personal_members;if(BGIME_COUNT_SUPPLY&&positions.size()>1){corrections.push_back(held);++di;}else query=Query::CountStart;}else if(s.status!=BaseOrderStatus::Pending){fail("PERSONAL_FACT_AUTH");out.status=SparseStatus::Stale;}return out;}
  if(query==Query::CountStart){ticket=authority.begin_count(view,held.key);++stats.count_queries;query=Query::Count;continue;}
  if(query==Query::Count){--out.cost.work_items;auto s=authority.advance_count(ticket,{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,b.max_work_items-out.cost.work_items});out.cost.read_calls+=s.cost.read_calls;out.cost.read_bytes+=s.cost.read_bytes;out.cost.work_items+=s.cost.work_items;if(s.status==CountStatus::Found||s.status==CountStatus::NotFound){held.effective_count=s.count;held.score=rank_score_from_raw(held.score,s.count,rule);if(s.count)++stats.positive_members;corrections.push_back(held);authority.cancel_count(ticket);ticket={};++di;query=Query::Fact;}else if(s.status!=CountStatus::Pending)out.status=SparseStatus::Stale;return out;}
  if(query==Query::BatchStart){
   ticket=authority.begin_counts(view,&corrections[0].key,sizeof(RankItem),corrections.size(),{draftor::memory::allocate,draftor::memory::release});
   if(!ticket.revision){local_fallback=true;++stats.local_fallbacks;out.status=SparseStatus::Fallback;return out;}
   stats.count_queries+=corrections.size();stats.memory=bytes();stats.peak=std::max(stats.peak,stats.memory);
   if(stats.memory>limit){authority.cancel_count(ticket);ticket={};local_fallback=true;++stats.local_fallbacks;out.status=SparseStatus::Fallback;return out;}query=Query::BatchRun;continue;
  }
  if(query==Query::BatchRun){--out.cost.work_items;auto s=authority.advance_counts(ticket,{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,b.max_work_items-out.cost.work_items,b.stop_requested,b.stop_context});out.cost.read_calls+=s.cost.read_calls;out.cost.read_bytes+=s.cost.read_bytes;out.cost.work_items+=s.cost.work_items;
   if(s.status==CountStatus::Found){di=0;query=Query::BatchFill;}else if(s.status!=CountStatus::Pending)out.status=SparseStatus::Stale;return out;
  }
  if(query==Query::BatchFill){if(di==corrections.size()){authority.cancel_count(ticket);ticket={};sort_i=1;query=Query::SortLoad;continue;}
   auto s=authority.counts_result(ticket,di);if(s.status!=CountStatus::Found&&s.status!=CountStatus::NotFound){out.status=SparseStatus::Stale;return out;}
   auto& item=corrections[di++];item.effective_count=s.count;item.score=rank_score_from_raw(item.score,s.count,rule);if(s.count)++stats.positive_members;continue;
  }
  if(query==Query::SortLoad){if(sort_i==corrections.size()||corrections.empty()){query=Query::Anchor;continue;}held=corrections[sort_i];sort_j=sort_i;query=Query::SortMove;continue;}
  if(query==Query::SortMove){++stats.sort_steps;if(sort_j&&better(held,corrections[sort_j-1])){corrections[sort_j]=corrections[sort_j-1];--sort_j;}else{corrections[sort_j]=held;++sort_i;query=Query::SortLoad;}continue;}
  if(query==Query::Anchor){if(direction==RangeDirection::First){page_start=0;page_end=std::min<size_t>(q,N);eligible=N;query=Query::Merge;continue;}
   if(anchor_search==prefix.size()){fail("ANCHOR_NOT_IN_CURRENT_PREFIX");out.status=SparseStatus::Stale;return out;}++stats.anchor_steps;
   const auto& x=prefix[anchor_search];if(x.key==anchor&&x.score==anchor_score){if(direction==RangeDirection::Before){page_end=anchor_search;page_start=page_end>q?page_end-q:0;eligible=page_end;}else{page_start=anchor_search+1;eligible=N-page_start;page_end=page_start+std::min(q,eligible);}query=Query::Merge;}else ++anchor_search;continue;}
  if(query==Query::Merge){if(prefix.size()>=page_end){output=page_start;query=Query::Output;continue;}
   if(skip_pos<positions.size()&&base_pos==positions[skip_pos]){++base_pos;++skip_pos;continue;}
   if(!base_ready&&base_pos<N){--out.cost.work_items;auto s=order->read_item(*cursor,uint32_t(base_pos),{b.max_read_calls-out.cost.read_calls,b.max_read_bytes-out.cost.read_bytes,b.max_work_items-out.cost.work_items});out.cost.read_calls+=s.cost.read_calls;out.cost.read_bytes+=s.cost.read_bytes;out.cost.work_items+=s.cost.work_items;if(s.status==BaseOrderStatus::Ready){base=s.item;base.score=rank_score_from_raw(base.score,0,rule);base_ready=true;++stats.facts;}else if(s.status!=BaseOrderStatus::Pending)out.status=SparseStatus::Stale;return out;}
   if(dpos<corrections.size()&&(!base_ready||better(corrections[dpos],base)))prefix.push_back(corrections[dpos++]);else if(base_ready){prefix.push_back(base);base_ready=false;++base_pos;}else{fail("MERGE_INCOMPLETE");out.status=SparseStatus::Stale;return out;}continue;}
  if(query==Query::Output){if(output<page_end){out.item=prefix[output++];out.status=SparseStatus::Item;return out;}query=Query::Done;out.status=SparseStatus::End;return out;}
  out.status=SparseStatus::End;return out;
 }return out;
}

PersonalIndex::PersonalIndex(BoundedUserState&a,Resources&r,std::string f,size_t n,size_t rc):p(std::make_unique<Impl>(a,r,std::move(f),n,rc)){p->memory();}
PersonalIndex::~PersonalIndex()=default;
void PersonalIndex::releaseClosed(){cancel();r2::Vector<uint32_t>().swap(p->bits);r2::Vector<uint32_t>().swap(p->frozen_bits);r2::Vector<uint32_t>().swap(p->positions);r2::Vector<RankItem>().swap(p->corrections);r2::Vector<RankItem>().swap(p->prefix);p->reader.reset();p->data.reset();p->proof.reset();p->memory();}
size_t PersonalIndex::implementationBytes(){return sizeof(Impl);}
const uint32_t PersonalIndex::TargetAbi[10]={sizeof(PersonalIndex),sizeof(Impl),sizeof(Impl::Pending),sizeof(RankItem),sizeof(Impl::dirty_set),sizeof(Impl::dirty_words),sizeof(Impl::frame),sizeof(Impl::chunk),BitmapBytes,720896};
bool PersonalIndex::service(){if(!pending())return false;auto t=clockUs();bool ok=p->background();++p->stats.services;p->stats.max_step_us=std::max(p->stats.max_step_us,clockUs()-t);return ok;}
bool PersonalIndex::pending()const{return p->failure.empty()&&p->needed();}
bool PersonalIndex::healthy()const{return p->failure.empty();}const char* PersonalIndex::error()const{return p->failure.c_str();}
void PersonalIndex::start_import(){if(p->loaded)throw std::runtime_error("import must precede open");if(!p->ensure_bits())throw std::runtime_error(error());p->failure.clear();p->imported=p->loaded=true;p->checkpoint_needed=true;}
void PersonalIndex::import_identity(RuntimeLexemeRef ref,const StableLexemeKey&key){if(!p->imported)throw std::runtime_error("explicit import required");p->todo.push_back({ref,key});while(!p->todo.empty()&&healthy())service();if(!healthy())throw std::runtime_error(error());}
void PersonalIndex::finish_import(){p->imported=false;while(pending())service();if(!healthy())throw std::runtime_error(error());}
void PersonalIndex::accepted(const CompositionLearningTicket&t){if(!healthy())return;if(t.receipt().members>64||p->todo.size()+t.receipt().members>256){p->fail("ACCEPTED_CAPACITY");return;}for(size_t i=0;i<t.receipt().members;++i){auto m=t.member(i);if(m.runtime_ref)p->todo.push_back({m.runtime_ref,m.key});}}
bool PersonalIndex::accepts(const CountSource& c)const{return same(c.view(),p->authority.view());}
SparseStatistics PersonalIndex::statistics()const{SparseStatistics s;s.members=p->stats.personal_members;s.corrections=p->stats.count_queries;s.allocated_bytes=p->bytes();s.entries=p->marked_members;return s;}
void PersonalIndex::begin_direct(const StateView&v,std::string_view c,BaseOrderIndex&o,BaseOrderCursor&cur,RankRule rule,RangeDirection d,const StableLexemeKey&key,int64_t score,size_t q){
 ++p->rev;if(!p->bound())p->fail("RELATION_RESOURCE_BINDING");const auto id=p->resources.scheme.info().scheme_id;const uint32_t sid=id=="xiaohe"?0:id=="full_pinyin"?1:id=="wubi86"?2:3;
 const auto scope=(sid<<30)|cur.range_start();bool reuse=p->active&&same(v,p->view)&&p->scope==scope&&p->rule.lambda==rule.lambda&&p->rule.revision==rule.revision&&p->resources.shared.activation_is_current(p->activation)&&p->code==c&&!p->local_fallback;
 p->view=v;p->order=&o;p->cursor=&cur;p->activation=p->resources.shared.activation_handle();p->code=std::string(c);p->scope=scope;p->start=cur.range_start();p->N=cur.count();p->sid=sid;p->rule=rule;p->direction=d;p->anchor=key;p->anchor_score=score;p->q=q;p->anchor_search=0;p->active=true;
 p->local_fallback=sid>2||p->N>p->range_limit;if(p->local_fallback){++p->stats.local_fallbacks;p->authority.cancel_count(p->ticket);p->ticket={};p->query=Impl::Query::Done;return;}
 if(uint64_t(p->start)+p->N>BitmapCounts[sid])p->fail("RANGE_RESOURCE_BOUNDS");
 p->reserve(p->positions,p->N);
 if(p->saving&&p->loaded&&p->todo.empty())++p->stats.query_during_save;
 if(reuse&&p->query==Impl::Query::Done)p->query=Impl::Query::Anchor;else p->reset_query();
}
SparseStep PersonalIndex::direct_next(const WorkBudget&b){return p->next(b);}
uint32_t PersonalIndex::direct_start()const{return uint32_t(p->page_start);}uint32_t PersonalIndex::direct_eligible()const{return uint32_t(p->eligible);}
bool PersonalIndex::current()const{return p->query_current();}uint64_t PersonalIndex::revision()const{return p->rev;}
void PersonalIndex::cancel(){p->active=false;++p->rev;p->authority.cancel_count(p->ticket);}
IndexCost PersonalIndex::cost()const{auto x=p->stats;x.refs=p->marked_refs;x.relations=p->marked_members;x.memory=p->bytes();return x;}
VerifiedResourceSnapshot PersonalIndex::relation_snapshot()const{return p->reader?p->reader->snapshot():VerifiedResourceSnapshot{};}
VerifiedResourceMemory PersonalIndex::relation_memory()const{return p->reader?p->reader->memory_usage():VerifiedResourceMemory{};}
PersistenceView PersonalIndex::persistence()const{return {p->loaded,p->loaded&&p->todo.empty()&&p->failure.empty(),p->saving,p->save_checkpoint,p->save_phase,p->dirty_count,p->journal_bytes,p->saved_sequence,p->save_sequence};}
}
