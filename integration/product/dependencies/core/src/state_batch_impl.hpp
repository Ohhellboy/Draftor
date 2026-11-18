// Included inside namespace hanime, after the existing StateV3 wire helpers.
struct StateBatch::Impl {
 struct Member {StableLexemeKey key;std::uint32_t count{0};std::uint8_t state{0},ram{0};};
 struct Node {std::uint64_t lo{0},hi{0};std::uint32_t first{0},last{0};};
 CountBatchAllocator allocator;void* storage{nullptr};std::size_t bytes{0},size{0},node_cap{0};
 Member* members{nullptr};std::uint16_t *order{nullptr},*scratch{nullptr};Node* nodes{nullptr};
 Meta meta;std::shared_ptr<const StateIoIdentity> activation;std::uint64_t physical{0},revision{0};unsigned file{0};
 enum Phase {Reset,Collect,SortStart,SortMerge,TailRead,TailSearch,TailMatch,Compact,Pop,Empty,Read,Search,Equal,Push,StreamMatch,Done} phase{Reset};
 std::size_t scan{0},n{0},width{1},base{0},left{0},right{0},middle{0},end{0},out{0},lo{0},hi{0},split{0},equal_end{0},top{0};
 std::uint64_t tail{0},row{0};bool stream{false};Node node;Transfer transfer;Value pivot;
 std::uint64_t work{0},deadline_yields{0},zero_progress_returns{0};const char* stop="begin";
 Impl(const void* first,std::size_t stride,std::size_t count,CountBatchAllocator a):allocator(a),size(count) {
  if(!first||!size||size>StateBatch::Limit||stride<sizeof(StableLexemeKey)||stride>std::numeric_limits<std::size_t>::max()/size||bool(a.allocate)!=bool(a.release))throw std::invalid_argument("COUNT_BATCH_INPUT");
  node_cap=std::min<std::size_t>(size,64);
  const auto arrays=size*(sizeof(Member)+2*sizeof(std::uint16_t));
  const auto node_offset=(arrays+alignof(Node)-1)/alignof(Node)*alignof(Node);
  bytes=node_offset+node_cap*sizeof(Node);
  storage=a.allocate ? a.allocate(bytes) : ::operator new(bytes);
  if(!storage)throw std::bad_alloc();
  members=static_cast<Member*>(storage);
  order=reinterpret_cast<std::uint16_t*>(members+size);scratch=order+size;
  nodes=reinterpret_cast<Node*>(static_cast<unsigned char*>(storage)+node_offset);
  for(std::size_t i=0;i<node_cap;++i)new(nodes+i) Node;
  for(std::size_t i=0;i<size;++i){new(members+i) Member;std::memcpy(members[i].key.bytes.data(),static_cast<const unsigned char*>(first)+i*stride,32);}
 }
 ~Impl(){if(allocator.release)allocator.release(storage);else ::operator delete(storage);}
 void restart(){phase=Reset;scan=n=top=0;transfer.reset();}
 int compare(std::size_t i,const StableLexemeKey& k)const{return std::memcmp(members[order[i]].key.bytes.data(),k.bytes.data(),32);}
};
StateBatch::StateBatch(const void* keys,std::size_t stride,std::size_t n,CountBatchAllocator a):p_(std::make_unique<Impl>(keys,stride,n,a)) {
 for(std::size_t i=0;i<n;++i)if(p_->members[i].key.empty())throw std::invalid_argument("COUNT_BATCH_EMPTY_KEY");
}
StateBatch::~StateBatch()=default;
std::size_t StateBatch::size()const{return p_->size;}
std::size_t StateBatch::memory_bytes()const{return sizeof(StateBatch)+sizeof(Impl)+p_->bytes;}
CountBatchProgress StateBatch::progress()const{
 const auto& m=*p_;using M=Impl;
 static constexpr const char* names[]={"reset","collect","sort_start","sort_merge","tail_read","tail_search","tail_match","compact","pop","empty","snapshot_read","snapshot_search","equal","push","stream_match","done"};
 const auto cursor=m.phase==M::SortMerge?m.out:m.phase==M::SortStart?m.base:m.phase==M::TailRead?m.tail:m.phase==M::Read?m.row:m.scan;
 return {names[m.phase],m.stop,cursor,m.size,m.work,m.deadline_yields,m.zero_progress_returns};
}
const StableLexemeKey& StateBatch::key(std::size_t i)const{return p_->members[i].key;}
void StateBatch::ram(std::size_t i,std::uint32_t v){auto& m=p_->members[i];m.count=v;m.state=1;m.ram=1;}
bool StateBatch::result(std::size_t i,std::uint32_t& v,bool& found)const{
 if(i>=p_->size||p_->phase!=Impl::Done||!p_->members[i].state)return false;
 v=p_->members[i].count;found=p_->members[i].state==1;return true;
}
