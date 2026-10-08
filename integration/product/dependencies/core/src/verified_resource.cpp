#include "hanime/verified_resource.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace hanime {
namespace {
constexpr std::array<std::uint32_t,64> kRound{{
  0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
  0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
  0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
  0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
  0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
  0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
  0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
  0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
}};
std::uint32_t rotate(std::uint32_t v,unsigned n) {return (v>>n)|(v<<(32U-n));}
void le32(std::uint8_t* p,std::uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(v>>(8U*i));}
void le64(std::uint8_t* p,std::uint64_t v) {for(unsigned i=0;i<8;++i)p[i]=static_cast<std::uint8_t>(v>>(8U*i));}
bool nonzero(const Sha256Digest& d) {return std::any_of(d.begin(),d.end(),[](auto x){return x!=0;});}
template<class T> void saturated_add(T& a,T b) {
  a=b>std::numeric_limits<T>::max()-a ? std::numeric_limits<T>::max() : a+b;
}
void add_cost(WorkCost& a,const WorkCost& b) {
  saturated_add(a.read_calls,b.read_calls); saturated_add(a.read_bytes,b.read_bytes);
  saturated_add(a.work_items,b.work_items);
}
}

Sha256::Sha256() : state_{{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                          0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U}} {}
void Sha256::compress(const std::uint8_t* p) {
  std::array<std::uint32_t,64> w{};
  for(unsigned i=0;i<16;++i)w[i]=(std::uint32_t(p[i*4])<<24U)|(std::uint32_t(p[i*4+1])<<16U)|
      (std::uint32_t(p[i*4+2])<<8U)|p[i*4+3];
  for(unsigned i=16;i<64;++i) {
    const auto a=rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3U);
    const auto b=rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10U);
    w[i]=w[i-16]+a+w[i-7]+b;
  }
  auto a=state_[0],b=state_[1],c=state_[2],d=state_[3];
  auto e=state_[4],f=state_[5],g=state_[6],h=state_[7];
  for(unsigned i=0;i<64;++i) {
    const auto s1=rotate(e,6)^rotate(e,11)^rotate(e,25);
    const auto ch=(e&f)^((~e)&g);
    const auto t1=h+s1+ch+kRound[i]+w[i];
    const auto s0=rotate(a,2)^rotate(a,13)^rotate(a,22);
    const auto maj=(a&b)^(a&c)^(b&c);
    const auto t2=s0+maj;
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
  }
  state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;
  state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
}
void Sha256::update(const void* source,std::size_t n) {
  if(!n)return;
  const auto* p=static_cast<const std::uint8_t*>(source);
  bytes_+=n;
  while(n) {
    const auto amount=std::min(n,buffer_.size()-filled_);
    std::memcpy(buffer_.data()+filled_,p,amount);filled_+=amount;p+=amount;n-=amount;
    if(filled_==buffer_.size()){compress(buffer_.data());filled_=0;}
  }
}
Sha256Digest Sha256::final() const {
  auto x=*this; const auto bits=bytes_*8U;
  const std::uint8_t start=0x80; x.update(&start,1);
  const std::uint8_t zero=0;
  while(x.filled_!=56)x.update(&zero,1);
  std::array<std::uint8_t,8> length{};
  for(unsigned i=0;i<8;++i)length[i]=static_cast<std::uint8_t>(bits>>((7U-i)*8U));
  x.update(length.data(),length.size());
  Sha256Digest out{};
  for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)
    out[i*4+j]=static_cast<std::uint8_t>(x.state_[i]>>((3U-j)*8U));
  return out;
}
Sha256Digest sha256(const void* p,std::size_t n) {Sha256 s;s.update(p,n);return s.final();}
Sha256Digest merkle_leaf_hash(std::uint64_t block,const void* p,std::size_t n) {
  std::array<std::uint8_t,13> prefix{};le64(prefix.data()+1,block);le32(prefix.data()+9,static_cast<std::uint32_t>(n));
  Sha256 s;s.update(prefix.data(),prefix.size());s.update(p,n);return s.final();
}
Sha256Digest merkle_page_hash(std::uint32_t level,std::uint32_t page,const void* p) {
  std::array<std::uint8_t,9> prefix{};prefix[0]=1;le32(prefix.data()+1,level);le32(prefix.data()+5,page);
  Sha256 s;s.update(prefix.data(),prefix.size());s.update(p,kVerifiedIndexPageBytes);return s.final();
}
bool merkle_layout(std::uint64_t bytes,MerkleLayout* out) {
  if(!out||!bytes||bytes>std::numeric_limits<std::size_t>::max())return false;
  MerkleLayout r;r.data_blocks=(bytes-1)/kVerifiedBlockBytes+1;
  auto children=r.data_blocks;
  do {
    if(r.levels==kVerifiedMaxLevels)return false;
    const auto pages=(children-1)/kVerifiedFanout+1;
    if(pages>std::numeric_limits<std::uint32_t>::max()||
       pages>(std::numeric_limits<std::size_t>::max()-r.index_bytes)/kVerifiedIndexPageBytes)return false;
    r.page_counts[r.levels]=static_cast<std::uint32_t>(pages);
    r.offsets[r.levels]=r.index_bytes;r.index_bytes+=pages*kVerifiedIndexPageBytes;
    ++r.levels;children=pages;
  } while(children>1);
  *out=r;return true;
}

struct VerifiedResourceReader::Impl {
  struct DataSlot {std::uint64_t block{0};bool valid{false};std::uint64_t age{0};};
  struct IndexSlot {std::uint32_t level{0},page{0};bool valid{false};std::uint64_t age{0};};
  struct AnchorSlot {std::uint64_t node{0};Sha256Digest hash{};bool valid{false};};
  static constexpr std::uint32_t anchor_level=8;
  enum class Phase {Lookup,IndexStart,IndexLoad,IndexHash,DataLoad,DataHash,Copy,
                    PacketLoad,PacketPadding,PacketLeaf,PacketParent,PacketAnchor,PacketCache};
  std::shared_ptr<const ResourceReader> data,index;
  ResourceReleaseSpec spec;
  VerifiedResourceConfig config;
  MerkleLayout layout;
  PackedMerkleLayout packed_layout;
  std::size_t data_block_bytes{kVerifiedBlockBytes}, data_slot_count{0}, index_slot_count{0};
  std::size_t index_allocation_bytes{0};
  std::array<std::uint8_t,64> parent_bytes{};
  std::uint64_t packet_loads{0};
  std::unique_ptr<std::uint8_t[]> data_bytes,index_bytes;
  std::unique_ptr<DataSlot[]> data_slots;
  std::unique_ptr<IndexSlot[]> index_slots;
  std::unique_ptr<AnchorSlot[]> anchors;
  std::size_t anchor_count{0},packet_read_bytes{0};
  std::uint32_t packet_target_depth{0};
  Sha256Digest packet_root{},pending_anchor{};
  std::uint64_t anchor_hits{0},anchor_misses{0},anchor_inserts{0},proof_saved{0};
  VerifiedResourceStatus status{VerifiedResourceStatus::Pending};
  const char* error{""};
  std::uint64_t loads{0},index_loads{0};WorkCost total;
  std::uint64_t logical_requests{0},logical_bytes{0},cache_only_requests{0},cache_only_bytes{0};
  std::uint64_t data_hits{0},index_hits{0};
  std::uint64_t cache_clock{0};
  bool active{false};std::size_t offset{0},count{0},copied{0};std::uint8_t* destination{nullptr};
  ResourceReadContext context;
  Phase phase{Phase::Lookup};std::uint64_t block{0};
  std::uint32_t level{0};std::array<std::uint32_t,kVerifiedMaxLevels> path{};
  std::size_t data_slot{0},index_slot{0},filled{0},hashed{0},true_bytes{0},prefix_pos{0},prefix_size{0};
  std::array<std::uint8_t,13> prefix{};Sha256 hash;Sha256Digest expected{};
  bool bootstrap_started{false},boot_ready{false};std::size_t boot_position{0},boot_count{0};
  std::uint64_t boot_first_root_block{0};std::uint8_t boot_byte{0};

  void fail(const char* message) {status=VerifiedResourceStatus::Error;error=message;active=false;context={};boot_ready=false;}
  bool current() {
    if(status==VerifiedResourceStatus::Closed||status==VerifiedResourceStatus::Error)return false;
    if(!index || !index->healthy() ||
       (spec.verification_format==1 && (!data || !data->healthy())) ||
       (data && !data->healthy())) {fail("underlying resource unavailable");return false;}
    return true;
  }
  std::uint64_t touch() {
    // The clock is only replacement policy, never an identity. On the remote
    // possibility of wrap, keep verified bytes but forget historical recency.
    // Metadata work is one bounded unit: format 1 has <=64 data +64 index
    // slots; format 2 has <=128 data and no index slots. The wrap fallback is
    // <=128 slots in either format, not an exact CPU-instruction counter.
    if(cache_clock==std::numeric_limits<std::uint64_t>::max()) {
      for(std::size_t i=0;i<data_slot_count;++i)data_slots[i].age=0;
      for(std::size_t i=0;i<index_slot_count;++i)index_slots[i].age=0;
      cache_clock=0;
    }
    return ++cache_clock;
  }
  std::size_t find_data(std::uint64_t number) {
    for(std::size_t i=0;i<data_slot_count;++i)
      if(data_slots[i].valid&&data_slots[i].block==number){data_slots[i].age=touch();return i;}
    return data_slot_count;
  }
  std::size_t find_index(std::uint32_t l,std::uint32_t p) {
    for(std::size_t i=0;i<index_slot_count;++i)
      if(index_slots[i].valid&&index_slots[i].level==l&&index_slots[i].page==p){index_slots[i].age=touch();return i;}
    return index_slot_count;
  }
  std::size_t select_data() const {
    std::size_t oldest=0;
    for(std::size_t i=0;i<data_slot_count;++i) {
      if(!data_slots[i].valid)return i;
      if(data_slots[i].age<data_slots[oldest].age)oldest=i;
    }
    return oldest;
  }
  std::size_t select_index() const {
    std::size_t oldest=0;
    for(std::size_t i=0;i<index_slot_count;++i) {
      if(!index_slots[i].valid)return i;
      if(index_slots[i].age<index_slots[oldest].age)oldest=i;
    }
    return oldest;
  }
  bool work(const WorkBudget& b,WorkCost& c) {
    if(c.work_items==b.max_work_items)return false;
    ++c.work_items;return true;
  }
  bool load(const ResourceReader& r,std::uint64_t start,std::uint8_t* target,std::size_t bytes,
            const WorkBudget& b,WorkCost& c) {
    if(c.read_calls==b.max_read_calls||c.read_bytes==b.max_read_bytes)return false;
    const auto n=std::min({bytes-filled,config.max_single_read_bytes,b.max_read_bytes-c.read_bytes});
    if(!n)return false;
    ++c.read_calls;c.read_bytes+=n;
    if(!r.healthy()||!r.read(static_cast<std::size_t>(start)+filled,target+filled,n)||!r.healthy()) {
      fail("resource or verification index read failed");return false;
    }
    filled+=n;return true;
  }
  void start_hash(bool leaf) {
    hash=Sha256{};prefix.fill(0);prefix_pos=hashed=0;
    if(leaf) {prefix_size=13;le64(prefix.data()+1,block);le32(prefix.data()+9,static_cast<std::uint32_t>(true_bytes));}
    else {prefix_size=9;prefix[0]=1;le32(prefix.data()+1,level);le32(prefix.data()+5,path[level]);}
  }
  bool hash_more(const std::uint8_t* bytes,std::size_t size,const WorkBudget& b,WorkCost& c) {
    if(c.work_items==b.max_work_items)return false;
    if(prefix_pos<prefix_size) {
      const auto n=std::min(prefix_size-prefix_pos,b.max_work_items-c.work_items);
      hash.update(prefix.data()+prefix_pos,n);prefix_pos+=n;c.work_items+=n;return true;
    }
    if(hashed<size) {
      const auto n=std::min(size-hashed,b.max_work_items-c.work_items);
      // Padding entries are canonical zeroes; bound their check by hashed bytes.
      if(phase==Phase::IndexHash) {
        const std::uint64_t children=level ? layout.page_counts[level-1] : layout.data_blocks;
        const auto start=std::uint64_t(path[level])*kVerifiedFanout;
        const auto entries=std::min<std::uint64_t>(kVerifiedFanout,children-start);
        for(auto i=std::max<std::size_t>(hashed,static_cast<std::size_t>(entries)*32);i<hashed+n;++i)
          if(bytes[i]){fail("nonzero verification index padding");return false;}
      }
      hash.update(bytes+hashed,n);hashed+=n;c.work_items+=n;return true;
    }
    return false;
  }
  void descend() {
    const auto* bytes=index_bytes.get()+index_slot*kVerifiedIndexPageBytes;
    const auto child=level ? path[level-1] : block;
    std::copy_n(bytes+(child%kVerifiedFanout)*32,32,expected.begin());
    if(level){--level;phase=Phase::IndexStart;}
    else {
      data_slot=select_data();
      data_slots[data_slot]={block,false,0};filled=0;phase=Phase::DataLoad;
    }
  }
  // A packet is never a trusted cache entry while loading/hashing. The one
  // scratch buffer belongs to this reader's current operation, not the caller.
  void start_packet_leaf() {
    start_hash(true);
    prefix[0] = 2;
  }
  void start_packet_parent() {
    const auto* sibling = index_bytes.get() + kPackedDataBytes + 32 * level;
    const bool right = ((block >> level) & 1U) != 0;
    std::copy_n(right ? sibling : expected.data(), 32, parent_bytes.begin());
    std::copy_n(right ? expected.data() : sibling, 32, parent_bytes.begin() + 32);
    hash = Sha256{};
    prefix.fill(0);
    prefix[0] = 3;
    prefix_pos = hashed = 0;
    prefix_size = 13;
    le32(prefix.data() + 1, level);
    le64(prefix.data() + 5, block >> (level + 1));
  }
  bool finish_packet_path() {
    if (expected != packet_root) {
      fail("packed authentication root mismatch");
      return false;
    }
    filled = 0;
    phase = anchors && packet_target_depth==packed_layout.depth ? Phase::PacketAnchor : Phase::PacketCache;
    return true;
  }
  bool advance_packet(const WorkBudget& budget,WorkCost& cost) {
    if (phase == Phase::PacketLoad) {
      if (filled < packet_read_bytes)
        return load(*index, block * packed_layout.packet_bytes, index_bytes.get(),
                    packet_read_bytes, budget, cost);
      if (!work(budget, cost)) return false;
      filled = true_bytes;
      phase = Phase::PacketPadding;
    } else if (phase == Phase::PacketPadding) {
      if (filled < kPackedDataBytes) {
        const auto n = std::min(kPackedDataBytes - filled, budget.max_work_items - cost.work_items);
        if (!n) return false;
        for (std::size_t i = 0; i < n; ++i) {
          if (index_bytes[filled + i]) {
            fail("nonzero packed data padding");
            return false;
          }
        }
        filled += n;
        cost.work_items += n;
      } else {
        if (!work(budget, cost)) return false;
        start_packet_leaf();
        phase = Phase::PacketLeaf;
      }
    } else if (phase == Phase::PacketLeaf) {
      if (prefix_pos < prefix_size || hashed < true_bytes)
        return hash_more(index_bytes.get(), true_bytes, budget, cost);
      if (!work(budget, cost)) return false;
      expected = hash.final();
      level = 0;
      if (!packed_layout.depth) return finish_packet_path();
      start_packet_parent();
      phase = Phase::PacketParent;
    } else if (phase == Phase::PacketParent) {
      if (prefix_pos < prefix_size || hashed < parent_bytes.size())
        return hash_more(parent_bytes.data(), parent_bytes.size(), budget, cost);
      if (!work(budget, cost)) return false;
      expected = hash.final();
      ++level;
      if (anchors && level==anchor_level) pending_anchor=expected;
      if (level == packet_target_depth) return finish_packet_path();
      start_packet_parent();
    } else if (phase == Phase::PacketAnchor) {
      if (!work(budget,cost)) return false;
      // Publish only after the COMPLETE original root proof succeeded. The
      // copied digest has a fixed level and exact node position under this
      // reader's trusted immutable pin. Partial/cancelled proofs never enter.
      const auto node=block>>anchor_level;
      anchors[node%anchor_count]={node,pending_anchor,true};
      saturated_add(anchor_inserts,std::uint64_t{1});
      phase=Phase::PacketCache;
    } else {
      // Account the authenticated scratch-to-cache copy as work. Cancellation
      // at any partial copy leaves this slot invalid, and a replacement starts
      // with a fresh packet; no unverified suffix can become visible.
      if (filled < true_bytes) {
        const auto n = std::min(true_bytes - filled, budget.max_work_items - cost.work_items);
        if (!n) return false;
        std::memcpy(data_bytes.get() + data_slot * data_block_bytes + filled, index_bytes.get() + filled, n);
        filled += n;
        cost.work_items += n;
      } else {
        if (!work(budget, cost)) return false;
        data_slots[data_slot].valid = true;
        data_slots[data_slot].age = touch();
        saturated_add(loads, std::uint64_t{1});
        saturated_add(packet_loads, std::uint64_t{1});
        if (packet_target_depth<packed_layout.depth)
          saturated_add(proof_saved,std::uint64_t{32}*(packed_layout.depth-packet_target_depth));
        phase = Phase::Copy;
      }
    }
    return true;
  }
  ResourceReadStep advance(std::size_t requested_offset,void* dest,std::size_t bytes,const WorkBudget& b,
                          const ResourceReadContext& requested_context) {
    ResourceReadStep out;out.status=ResourceReadStatus::Pending;
    struct Scope {WorkCost& total;WorkCost& cost;~Scope(){add_cost(total,cost);}} scope{total,out.cost};
    if(!current()){out.status=ResourceReadStatus::Error;return out;}
    if(requested_offset>spec.file_bytes||bytes>spec.file_bytes-requested_offset||
       (bytes&&(!dest||reinterpret_cast<std::uintptr_t>(dest)>std::numeric_limits<std::uintptr_t>::max()-bytes))) {
      fail("authenticated read bounds invalid");out.status=ResourceReadStatus::Error;return out;
    }
    if(!bytes){active=false;context={};out.status=ResourceReadStatus::Ready;return out;}
    if(!active||context.owner!=requested_context.owner||context.revision!=requested_context.revision||
       offset!=requested_offset||count!=bytes||destination!=dest) {
      saturated_add(logical_requests,std::uint64_t{1});
      saturated_add(logical_bytes,static_cast<std::uint64_t>(bytes));
      offset=requested_offset;count=bytes;destination=static_cast<std::uint8_t*>(dest);
      context=requested_context;copied=0;phase=Phase::Lookup;active=true;
    }
    while(current()) {
      auto& c=out.cost;
      if(phase==Phase::Lookup) {
        if(!work(b,c))break;
        block=(offset+copied)/data_block_bytes;
        data_slot=find_data(block);
        if(data_slot<data_slot_count){saturated_add(data_hits,std::uint64_t{1});phase=Phase::Copy;continue;}
        true_bytes=static_cast<std::size_t>(std::min<std::uint64_t>(data_block_bytes,spec.file_bytes-block*data_block_bytes));
        if (spec.verification_format == 2) {
          data_slot = select_data();
          data_slots[data_slot] = {block, false, 0};
          filled = 0;
          packet_target_depth=packed_layout.depth; packet_root=spec.merkle_root;
          if (anchors) {
            const auto node=block>>anchor_level;
            const auto& anchor=anchors[node%anchor_count];
            if (anchor.valid && anchor.node==node) {
              packet_target_depth=anchor_level; packet_root=anchor.hash;
              saturated_add(anchor_hits,std::uint64_t{1});
            } else saturated_add(anchor_misses,std::uint64_t{1});
          }
          packet_read_bytes=kPackedDataBytes+32*packet_target_depth;
          phase = Phase::PacketLoad;
          continue;
        }
        auto p=block;
        for(std::uint32_t i=0;i<layout.levels;++i){p/=kVerifiedFanout;path[i]=static_cast<std::uint32_t>(p);}
        expected=spec.merkle_root;level=layout.levels-1;phase=Phase::IndexStart;
      } else if(phase>=Phase::PacketLoad) {
        if(!advance_packet(b,c))break;
      } else if(phase==Phase::IndexStart) {
        if(!work(b,c))break;
        index_slot=find_index(level,path[level]);
        if(index_slot<config.index_cache_pages){saturated_add(index_hits,std::uint64_t{1});descend();continue;}
        index_slot=select_index();
        index_slots[index_slot]={level,path[level],false,0};filled=0;phase=Phase::IndexLoad;
      } else if(phase==Phase::IndexLoad) {
        if(filled<kVerifiedIndexPageBytes) {
          if(!load(*index,layout.offsets[level]+std::uint64_t(path[level])*kVerifiedIndexPageBytes,
                   index_bytes.get()+index_slot*kVerifiedIndexPageBytes,kVerifiedIndexPageBytes,b,c))break;
        } else {if(!work(b,c))break;start_hash(false);phase=Phase::IndexHash;}
      } else if(phase==Phase::IndexHash) {
        if(prefix_pos<prefix_size||hashed<kVerifiedIndexPageBytes) {
          if(!hash_more(index_bytes.get()+index_slot*kVerifiedIndexPageBytes,kVerifiedIndexPageBytes,b,c))break;
        } else {
          if(!work(b,c))break;
          if(hash.final()!=expected){fail("verification index digest mismatch");break;}
          index_slots[index_slot].valid=true;index_slots[index_slot].age=touch();
          saturated_add(index_loads,std::uint64_t{1});descend();
        }
      } else if(phase==Phase::DataLoad) {
        if(filled<true_bytes) {
          if(!load(*data,block*kVerifiedBlockBytes,data_bytes.get()+data_slot*kVerifiedBlockBytes,true_bytes,b,c))break;
        } else {if(!work(b,c))break;start_hash(true);phase=Phase::DataHash;}
      } else if(phase==Phase::DataHash) {
        if(prefix_pos<prefix_size||hashed<true_bytes) {
          if(!hash_more(data_bytes.get()+data_slot*kVerifiedBlockBytes,true_bytes,b,c))break;
        } else {
          if(!work(b,c))break;
          if(hash.final()!=expected){fail("resource block digest mismatch");break;}
          data_slots[data_slot].valid=true;data_slots[data_slot].age=touch();
          saturated_add(loads,std::uint64_t{1});phase=Phase::Copy;
        }
      } else {
        if(c.work_items==b.max_work_items)break;
        const auto within=(offset+copied)%data_block_bytes;
        const auto n=std::min({count-copied,data_block_bytes-within,b.max_work_items-c.work_items});
        std::memcpy(destination+copied,data_bytes.get()+data_slot*data_block_bytes+within,n);
        copied+=n;c.work_items+=n;
        if(copied==count){active=false;context={};out.status=ResourceReadStatus::Ready;return out;}
        if((offset+copied)%data_block_bytes==0)phase=Phase::Lookup;
      }
    }
    if(status==VerifiedResourceStatus::Error||status==VerifiedResourceStatus::Closed)out.status=ResourceReadStatus::Error;
    return out;
  }
};

VerifiedResourceReader::VerifiedResourceReader(std::shared_ptr<const ResourceReader> data,
    std::shared_ptr<const ResourceReader> index,const ResourceReleaseSpec& spec,const VerifiedResourceConfig& config)
    : impl_(std::make_unique<Impl>()) {
  auto& p=*impl_;p.data=std::move(data);p.index=std::move(index);p.spec=spec;p.config=config;
  const bool packed=spec.verification_format==2;
  const bool valid_layout=packed ? packed_merkle_layout(spec.file_bytes,&p.packed_layout) : merkle_layout(spec.file_bytes,&p.layout);
  if(!valid_layout || !p.index || (!packed && !p.data) ||
     (spec.verification_format!=1 && !packed) ||
     spec.rule_version!=kResourceVerificationRule||
     (spec.role==ResourceReleaseRole::BaseOrder ?
        (spec.resource_format!=1U && spec.resource_format!=2U) : spec.resource_format!=2U)||spec.key_format!=kStableKeyFormat||
     !spec.registry_namespace||!spec.shared_bundle_id||!spec.runtime_ref_count||!spec.text_capacity||!spec.name_capacity||
     !nonzero(spec.file_sha256)||!nonzero(spec.merkle_root)||
     config.data_cache_blocks<3||config.data_cache_blocks>64||!config.index_cache_pages||config.index_cache_pages>64||
     !config.max_single_read_bytes||config.max_single_read_bytes>1024||
     config.packed_anchor_slots>64||(!packed && config.packed_anchor_slots)||
     (p.data && p.data->size()!=spec.file_bytes)||
     p.index->size()!=(packed ? p.packed_layout.file_bytes : p.layout.index_bytes)||
     (packed && p.packed_layout.packet_bytes>config.index_cache_pages*kVerifiedIndexPageBytes)) {
    p.fail("release version, dimensions, capacity, pin or file size mismatch");return;
  }
  p.data_block_bytes=packed ? kPackedDataBytes : kVerifiedBlockBytes;
  p.data_slot_count=config.data_cache_blocks*(packed ? 2 : 1);
  p.index_slot_count=packed ? 0 : config.index_cache_pages;
  p.index_allocation_bytes=packed ? p.packed_layout.packet_bytes : config.index_cache_pages*kVerifiedIndexPageBytes;
  const auto zero=std::find(spec.scheme_id.begin(),spec.scheme_id.end(),'\0');
  if(zero==spec.scheme_id.end()||std::any_of(zero,spec.scheme_id.end(),[](char c){return c!=0;})) {
    p.fail("release scheme name is not bounded canonical text");return;
  }
  if(spec.role==ResourceReleaseRole::Shared) {
    if(zero!=spec.scheme_id.begin()||spec.bootstrap_root_offset||spec.bootstrap_root_bytes||nonzero(spec.required_shared_sha256)) {
      p.fail("shared release contains scheme bootstrap data");return;
    }
  } else if(spec.role==ResourceReleaseRole::Scheme) {
    if(!nonzero(spec.required_shared_sha256)||zero==spec.scheme_id.begin()||!is_valid_utf8({spec.scheme_id.data(),static_cast<std::size_t>(zero-spec.scheme_id.begin())})||
       !spec.bootstrap_root_bytes||spec.bootstrap_root_offset>spec.file_bytes||
       spec.bootstrap_root_bytes>spec.file_bytes-spec.bootstrap_root_offset) {
      p.fail("scheme release bootstrap bounds invalid");return;
    }
    const auto first=spec.bootstrap_root_offset/p.data_block_bytes;
    const auto last=(spec.bootstrap_root_offset+spec.bootstrap_root_bytes-1)/p.data_block_bytes;
    if(last-first+1+(first!=0)>p.data_slot_count) {
      p.fail("bootstrap root needs more resident data blocks");return;
    }
  } else if(spec.role==ResourceReleaseRole::BaseOrder) {
    if(!nonzero(spec.required_shared_sha256)||!nonzero(spec.required_scheme_sha256)||
       spec.ranking_rule_version!=1||zero==spec.scheme_id.begin()||
       !is_valid_utf8({spec.scheme_id.data(),static_cast<std::size_t>(zero-spec.scheme_id.begin())})||
       spec.bootstrap_root_offset||spec.bootstrap_root_bytes) {
      p.fail("base-order release dependencies or rule invalid");return;
    }
  } else {p.fail("unknown release resource role");return;}
  if(spec.role!=ResourceReleaseRole::BaseOrder &&
     (nonzero(spec.required_scheme_sha256)||spec.ranking_rule_version)) {
    p.fail("unexpected ranking dependency on primary resource");return;
  }
  p.data_bytes=std::make_unique<std::uint8_t[]>(config.data_cache_blocks*kVerifiedBlockBytes);
  p.index_bytes=std::make_unique<std::uint8_t[]>(p.index_allocation_bytes);
  p.data_slots=std::make_unique<Impl::DataSlot[]>(p.data_slot_count);
  if(p.index_slot_count)p.index_slots=std::make_unique<Impl::IndexSlot[]>(p.index_slot_count);
  if (packed && p.packed_layout.depth>Impl::anchor_level && config.packed_anchor_slots) {
    p.anchor_count=config.packed_anchor_slots;
    p.anchors=std::make_unique<Impl::AnchorSlot[]>(p.anchor_count);
  }
  p.current();
}
VerifiedResourceReader::~VerifiedResourceReader()=default;
std::size_t VerifiedResourceReader::size() const{return static_cast<std::size_t>(impl_->spec.file_bytes);}
bool VerifiedResourceReader::healthy() const{return impl_->current();}
bool VerifiedResourceReader::read(std::size_t offset,void* destination,std::size_t n) const {
  auto& p=*impl_;
  saturated_add(p.cache_only_requests,std::uint64_t{1});
  saturated_add(p.cache_only_bytes,static_cast<std::uint64_t>(n));
  if(!p.current()||offset>p.spec.file_bytes||n>p.spec.file_bytes-offset||
     (n&&(!destination||reinterpret_cast<std::uintptr_t>(destination)>std::numeric_limits<std::uintptr_t>::max()-n)))return false;
  if(!n)return true;
  const auto first=offset/p.data_block_bytes,last=(offset+n-1)/p.data_block_bytes;
  if(last-first+1>p.data_slot_count)return false;
  for(auto i=first;i<=last;++i)if(p.find_data(i)==p.data_slot_count)return false;
  auto* dest=static_cast<std::uint8_t*>(destination);std::size_t done=0;
  while(done<n) {
    const auto block=(offset+done)/p.data_block_bytes,within=(offset+done)%p.data_block_bytes;
    const auto amount=std::min(n-done,p.data_block_bytes-within);
    std::memcpy(dest+done,p.data_bytes.get()+p.find_data(block)*p.data_block_bytes+within,amount);done+=amount;
  }
  return true;
}
ResourceReadStep VerifiedResourceReader::read_budgeted(std::size_t offset,void* dest,std::size_t n,
    const WorkBudget& budget,const ResourceReadContext& context) const {
  if(!context.owner||!context.revision)return {};
  return impl_->advance(offset,dest,n,budget,context);
}
void VerifiedResourceReader::cancel_read_budgeted(const ResourceReadContext& context) const {
  auto& p=*impl_;
  if(context.owner&&context.revision&&p.active&&context.owner==p.context.owner&&context.revision==p.context.revision) {
    p.active=false;p.context={};
  }
}
bool VerifiedResourceReader::prepare_bootstrap() {
  auto& p=*impl_;
  if(!p.current())return false;
  if(p.boot_ready)return true;
  p.active=false;p.context={};p.boot_position=0;p.boot_count=1;p.bootstrap_started=true;
  if(p.spec.role==ResourceReleaseRole::Scheme) {
    const auto first=p.spec.bootstrap_root_offset/p.data_block_bytes;
    const auto last=(p.spec.bootstrap_root_offset+p.spec.bootstrap_root_bytes-1)/p.data_block_bytes;
    p.boot_first_root_block=std::max<std::uint64_t>(1,first);
    if(last>=p.boot_first_root_block)p.boot_count+=static_cast<std::size_t>(last-p.boot_first_root_block+1);
  }
  return true;
}
ResourceReadStep VerifiedResourceReader::advance_bootstrap(const WorkBudget& b) {
  auto& p=*impl_;ResourceReadStep out;out.status=ResourceReadStatus::Error;
  if(!p.current()||!p.bootstrap_started)return out;
  if(p.boot_ready){out.status=ResourceReadStatus::Ready;return out;}
  const auto block=p.boot_position ? p.boot_first_root_block+p.boot_position-1 : 0;
  out=p.advance(static_cast<std::size_t>(block*p.data_block_bytes),&p.boot_byte,1,b,{});
  if(out.status==ResourceReadStatus::Ready) {
    if(++p.boot_position==p.boot_count){p.boot_ready=true;p.status=VerifiedResourceStatus::Ready;}
    else out.status=ResourceReadStatus::Pending;
  }
  return out;
}
bool VerifiedResourceReader::bootstrap_ready() const{return impl_->current()&&impl_->boot_ready;}
const ResourceReleaseSpec& VerifiedResourceReader::spec() const{return impl_->spec;}
const MerkleLayout& VerifiedResourceReader::layout() const{return impl_->layout;}
VerifiedResourceSnapshot VerifiedResourceReader::snapshot() const {
  auto& p=*impl_;VerifiedResourceSnapshot out;
  const bool current=p.current();out.status=p.status;
  out.total_blocks=p.spec.verification_format==2 ? p.packed_layout.data_blocks : p.layout.data_blocks;
  out.data_block_bytes=p.data_block_bytes;out.authenticated_packet_loads=p.packet_loads;
  out.anchor_hits=p.anchor_hits;out.anchor_misses=p.anchor_misses;
  out.anchor_inserts=p.anchor_inserts;out.proof_bytes_saved=p.proof_saved;
  out.logical_requests=p.logical_requests; out.logical_bytes=p.logical_bytes;
  out.cache_only_requests=p.cache_only_requests;out.cache_only_bytes=p.cache_only_bytes;
  out.data_cache_hits=p.data_hits;out.index_cache_hits=p.index_hits;
  out.authenticated_loads=p.loads;out.authenticated_index_loads=p.index_loads;out.cost=p.total;
  if(current) {
    for(std::size_t i=0;i<p.data_slot_count;++i)if(p.data_slots[i].valid)++out.resident_verified;
    for(std::size_t i=0;i<p.index_slot_count;++i)if(p.index_slots[i].valid)++out.resident_verified_index_pages;
  }
  return out;
}
VerifiedResourceMemory VerifiedResourceReader::memory_usage() const {
  const auto& p=*impl_;VerifiedResourceMemory m;m.owner_fixed_bytes=sizeof(*this)+sizeof(Impl);m.hash_state_bytes=sizeof(Sha256);
  if(p.data_bytes)m.data_payload_bytes=p.config.data_cache_blocks*kVerifiedBlockBytes;
  if(p.index_bytes)m.index_payload_bytes=p.index_allocation_bytes;
  if(p.data_slots)m.data_metadata_bytes=p.data_slot_count*sizeof(Impl::DataSlot);
  if(p.index_slots)m.index_metadata_bytes=p.index_slot_count*sizeof(Impl::IndexSlot);
  if(p.anchors)m.anchor_bytes=p.anchor_count*sizeof(Impl::AnchorSlot);
  return m;
}
const char* VerifiedResourceReader::error() const {return impl_->error;}
void VerifiedResourceReader::deactivate() {
  auto& p=*impl_;p.status=VerifiedResourceStatus::Closed;p.active=p.boot_ready=false;p.context={};p.error="resource deactivated";
  p.data_bytes.reset();p.index_bytes.reset();p.data_slots.reset();p.index_slots.reset();p.data.reset();p.index.reset();
  p.anchors.reset();
}
} // namespace hanime
