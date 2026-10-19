#pragma once
#include "base_order_internal.hpp"
#include "base_order_wire.hpp"
#include <limits>
namespace hanime::detail {
constexpr std::uint32_t kBlockNone=0xffffffffU;
inline std::uint16_t block16(const std::uint8_t* p) {return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);}
inline void block_charge(WorkCost& a,const WorkCost& b) {a.read_calls+=b.read_calls;a.read_bytes+=b.read_bytes;a.work_items+=b.work_items;}
inline WorkBudget block_remaining(const WorkBudget& b,const WorkCost& c) {return {b.max_read_calls-c.read_calls,b.max_read_bytes-c.read_bytes,b.max_work_items-c.work_items};}
inline bool block_work(const WorkBudget& b,WorkCost& c) {if(c.work_items==b.max_work_items)return false;++c.work_items;return true;}
struct BlockState {
  enum class Init { HeaderRead, HeaderParse, Allocate, AuthRead, FenceOffset, FenceLength, FenceChars, Ready, Error, Cancelled } init{Init::HeaderRead};
  std::array<std::uint8_t,128> raw{};
  std::array<std::uint32_t,32> h{};
  // Only the active index owns these bytes; no pointer escapes to a cursor.
  std::array<std::uint8_t,1024> bytes;
  std::uint8_t* nav{nullptr};
  void (*release)(void*){nullptr};
  ResourceReadContext load_context;
  std::size_t pos{0};
  std::uint32_t fence{0},offset{0},expected{0},length{0},previous_length{0},character{0};
  int comparison{0};
  bool copying{false},valid{false};
  std::array<std::uint8_t,128> previous{};
  std::uint32_t block_id{kBlockNone},records{0},restarts{0},first{0};
  std::uint64_t generation{0};
  BlockInitializationStats stats;
  ~BlockState(){if(nav && release)release(nav);}
  void clear_nav() {if(nav && release){release(nav);nav=nullptr;++stats.releases;}valid=false;}
  bool header_valid(std::size_t size) const {
    if(h[0]!=0x31535142U || h[1]!=5 || !h[2] || h[2]>h[3] || !h[4] || h[4]>128 ||
       !h[5] || h[6] || h[7] || h[21]!=h[5] || h[8]!=1024 || h[9]<4ULL*h[5] || h[9]>16325 ||
       h[10]!=(std::uint64_t(h[8])+h[9]+1023)/1024*1024 || h[11]!=std::uint64_t(h[5])*1024 ||
       h[18]!=std::uint64_t(h[10])+h[11] || h[19]!=std::uint64_t(h[18])+std::uint64_t(h[3])*64 ||
       h[20]!=std::uint64_t(h[19])+std::uint64_t(h[3])*8 || h[20]!=size)return false;
    for(unsigned i=12;i<18;++i)if(h[i])return false;
    for(unsigned i=22;i<32;++i)if(h[i])return false;
    return true;
  }
  const std::uint8_t* fence_data(std::uint32_t i,std::uint32_t& n) const {
    const auto at=bo32(nav+4*i);n=nav[at];return nav+at+1;
  }
  BaseOrderStep advance(const VerifiedResourceReader& reader,const WorkBudget& b,
      void* (*allocate)(std::size_t),void (*deallocate)(void*)) {
    BaseOrderStep out{BaseOrderStatus::Pending,{},{}};++stats.initialization_calls;
    auto bad=[&](){init=Init::Error;reader.cancel_read_budgeted(load_context);clear_nav();out.status=BaseOrderStatus::Error;return out;};
    if(init==Init::Ready){out.status=BaseOrderStatus::Ready;return out;}
    if(init==Init::Error){out.status=BaseOrderStatus::Error;return out;}
    if(init==Init::Cancelled){out.status=BaseOrderStatus::Stale;return out;}
    if(!b.max_read_calls || !b.max_read_bytes || !b.max_work_items){out.status=BaseOrderStatus::InvalidRequest;return out;}
    while(out.cost.work_items<b.max_work_items) {
      if(init==Init::HeaderRead || init==Init::AuthRead) {
        const bool header=init==Init::HeaderRead;
        const auto r=reader.read_budgeted(header?0:h[8],header?raw.data():nav,header?raw.size():h[9],block_remaining(b,out.cost),load_context);
        block_charge(out.cost,r.cost);block_charge(stats.auth_cost,r.cost);
        if(r.status==ResourceReadStatus::Error)return bad();
        if(r.status!=ResourceReadStatus::Ready)return out;
        pos=0;init=header?Init::HeaderParse:Init::FenceOffset;
        if(!header){expected=4*h[5];offset=0;fence=0;}
        continue;
      }
      if(!block_work(b,out.cost))break;
      ++stats.local_work;
      if(init==Init::HeaderParse) {
        h[pos/4]|=std::uint32_t(raw[pos])<<(8*(pos%4));
        if(++pos==128){if(!header_valid(reader.size()))return bad();init=Init::Allocate;}
      } else if(init==Init::Allocate) {
        if(!allocate || !deallocate)return bad();
        nav=static_cast<std::uint8_t*>(allocate(h[9]));release=deallocate;
        if(!nav)return bad();
        ++stats.allocations;stats.nav_bytes=h[9];init=Init::AuthRead;
      } else if(init==Init::FenceOffset) {
        offset|=std::uint32_t(nav[4*fence+pos])<<(8*pos);
        if(++pos==4){if(offset!=expected || offset>=h[9])return bad();init=Init::FenceLength;}
      } else if(init==Init::FenceLength) {
        length=nav[offset];if(!length || length>h[4] || offset+1ULL+length>h[9])return bad();
        character=0;comparison=0;copying=false;init=Init::FenceChars;
      } else if(init==Init::FenceChars) {
        const auto value=nav[offset+1+character];
        if(!copying) {
          if(fence && !comparison)comparison=character<previous_length ? (value>previous[character]?1:value<previous[character]?-1:0) : 1;
          copying=true;
        } else {
          previous[character]=value;copying=false;
          if(++character==length) {
            if(fence && (!comparison ? length<=previous_length : comparison<0))return bad();
            previous_length=length;expected=offset+1+length;
            if(++fence==h[5]) {if(expected!=h[9])return bad();init=Init::Ready;stats.initialized=true;out.status=BaseOrderStatus::Ready;return out;}
            offset=0;pos=0;init=Init::FenceOffset;
          }
        }
      }
    }
    return out;
  }
};
struct BlockCursor {
  enum class Phase { HintLower, HintUpper, Nav, NeedBlock, AuthPending, Header, RestartOffsets, RestartValidate, RestartUpper, RestartSearch, DecodeHeader, DecodeCharacters, DecodeBounds, DecodeCompare, Done } phase{Phase::Nav};
  std::uint32_t lo{0},hi{0},mid{kBlockNone},block{kBlockNone},restart{0},record{0},end{0},at{0};
  std::uint32_t index{0},offset{0},last_offset{0},key_length{0},previous_length{0},character{0};
  std::uint32_t start{0},count{0},paths{0};
  std::array<std::uint8_t,14> meta{};
  std::array<std::uint8_t,128> key{};
  std::uint64_t generation{0};
  std::uint32_t cmp_pos{0},nav_offset{0},nav_length{0},nav_progress{0};int comparison{0},record_order{0};bool cmp_done{false};
  std::uint32_t hint{kBlockNone},hint_restart{0};bool found{false},hinted{false};
  void reset() {
    phase=Phase::Nav;lo=hi=0;mid=block=kBlockNone;
    restart=record=end=at=index=offset=last_offset=key_length=previous_length=character=start=count=paths=0;
    generation=0;hint=kBlockNone;hint_restart=0;found=hinted=false;record_order=0;compare_reset();
    // Owned character/meta buffers are length-invalid; each consumed byte is
    // filled before use. Reusing a cursor does not clear or copy 128 key bytes.
  }
  void compare_reset(){cmp_pos=0;comparison=0;cmp_done=false;nav_offset=nav_length=nav_progress=0;}
  bool nav_compare(const BlockState& s,std::uint32_t i,std::string_view right,const WorkBudget& b,WorkCost& cost) {
    while(nav_progress<5) {
      if(!block_work(b,cost))return false;
      if(nav_progress<4)nav_offset|=std::uint32_t(s.nav[4*i+nav_progress])<<(8*nav_progress);
      else nav_length=s.nav[nav_offset];
      ++nav_progress;
    }
    return compare(s.nav+nav_offset+1,nav_length,right,b,cost);
  }
  // One compared byte or the final length comparison per work item. Supports
  // work=1 without a hidden whole-string compare or an unbounded retry.
  bool compare(const std::uint8_t* left,std::uint32_t size,std::string_view right,const WorkBudget& b,WorkCost& c) {
    while(!cmp_done && block_work(b,c)) {
      if(cmp_pos<size && cmp_pos<right.size()) {
        const auto r=static_cast<std::uint8_t>(right[cmp_pos]);const auto l=left[cmp_pos++];
        if(l!=r){comparison=l<r?-1:1;cmp_done=true;}
      } else {comparison=size<right.size()?-1:size>right.size()?1:0;cmp_done=true;}
    }
    return cmp_done;
  }
};
inline bool block_prepare_hint(BlockState& s,BlockCursor& c,std::string_view code,const WorkBudget& b,WorkCost& cost) {
  using P=BlockCursor::Phase;
  while(c.phase==P::HintLower || c.phase==P::HintUpper) {
    if(!c.nav_compare(s,c.phase==P::HintLower?c.hint:c.hint+1,code,b,cost))return false;
    const bool lower=c.phase==P::HintLower;
    if((lower && c.comparison>0) || (!lower && c.comparison<=0)) {
      ++s.stats.prefix_rejects;c.hinted=false;c.phase=P::Nav;c.lo=0;c.hi=s.h[5];c.mid=kBlockNone;c.compare_reset();return true;
    }
    c.compare_reset();
    if(lower && c.hint+1<s.h[5]){c.phase=P::HintUpper;continue;}
    ++s.stats.prefix_hits;c.hinted=true;c.block=c.hint;c.phase=P::NeedBlock;
  }
  return true;
}
// Query-only state machine. Existing owner/revision and resource activation
// checks are performed by BaseOrderIndex before every call.
inline BaseOrderStep block_locate(BlockState& s,BlockCursor& c,const VerifiedResourceReader& reader,
    const ResourceReadContext& context,std::string_view code,const WorkBudget& b) {
  BaseOrderStep out{BaseOrderStatus::Pending,{},{}};
  auto bad=[&](){s.valid=false;out.status=BaseOrderStatus::Error;return out;};
  auto done=[&](bool found){c.phase=BlockCursor::Phase::Done;c.found=found;out.status=found?BaseOrderStatus::Ready:BaseOrderStatus::NotFound;return out;};
  using P=BlockCursor::Phase;
  while(out.cost.work_items<b.max_work_items) {
    if(c.phase==P::Done)return done(c.found);
    if(c.phase==P::HintLower || c.phase==P::HintUpper) {
      if(!block_prepare_hint(s,c,code,b,out.cost))return out;
      continue;
    }
    if(c.phase==P::Nav) {
      if(c.lo==c.hi){if(!c.lo)return done(false);c.block=c.lo-1;c.phase=P::NeedBlock;continue;}
      if(c.mid==kBlockNone){c.mid=c.lo+(c.hi-c.lo)/2;c.compare_reset();}
      if(!c.nav_compare(s,c.mid,code,b,out.cost))return out;
      if(c.comparison<=0)c.lo=c.mid+1;else c.hi=c.mid;c.mid=kBlockNone;continue;
    }
    if(c.phase>=P::RestartSearch && c.phase<P::Done &&
       (!s.valid || s.block_id!=c.block || s.generation!=c.generation)) {
      // The key/offset are owned, but restart from this immutable block's
      // bounded restart search after replacement; no stale borrowed bytes.
      c.phase=P::NeedBlock;c.mid=kBlockNone;c.compare_reset();continue;
    }
    if(c.phase==P::NeedBlock) {
      if(!block_work(b,out.cost))return out;
      if(s.valid && s.block_id==c.block){++s.stats.block_reuses;c.generation=s.generation;c.lo=c.hinted?std::min(c.hint_restart,s.restarts-1):0;c.hi=s.restarts;c.mid=kBlockNone;c.phase=P::RestartSearch;continue;}
      if(s.generation==std::numeric_limits<std::uint64_t>::max())return bad();
      ++s.generation;s.stats.generation=s.generation;s.valid=false;s.block_id=c.block;
      c.generation=s.generation;c.phase=P::AuthPending;++s.stats.block_loads;continue;
    }
    if(c.phase==P::AuthPending) {
      const auto r=reader.read_budgeted(std::uint64_t(s.h[10])+std::uint64_t(c.block)*1024,s.bytes.data(),1024,block_remaining(b,out.cost),context);
      block_charge(out.cost,r.cost);
      if(r.status==ResourceReadStatus::Error)return bad();
      if(r.status!=ResourceReadStatus::Ready)return out;
      c.index=0;c.phase=P::Header;continue;
    }
    if(c.phase==P::Header) {
      if(!block_work(b,out.cost))return out;
      c.meta[c.index]=s.bytes[c.index];
      if(++c.index==8) {
        s.records=block16(c.meta.data());s.restarts=block16(c.meta.data()+2);s.first=bo32(c.meta.data()+4);
        if(!s.records || s.restarts!=(s.records+15)/16 || s.first>=s.h[2] || s.records>s.h[2]-s.first ||
           8+2*s.restarts+15*s.records>1024 || (!c.block && s.first) || (c.block+1==s.h[5] && s.first+s.records!=s.h[2]))return bad();
        c.index=0;c.restart=0;c.offset=0;c.last_offset=0;c.previous_length=0;c.phase=P::RestartOffsets;
      }continue;
    }
    if(c.phase==P::RestartOffsets) {
      if(!block_work(b,out.cost))return out;
      if(c.index<2)c.offset|=std::uint32_t(s.bytes[8+2*c.restart+c.index])<<(8*c.index);
      else c.meta[c.index-2]=s.bytes[c.offset+c.index-2];
      if(++c.index==2 && (c.offset<8+2*s.restarts || c.offset+14>=1024 ||
          (!c.restart && c.offset!=8+2*s.restarts) || (c.restart && c.offset<c.last_offset+14+c.previous_length)))return bad();
      if(c.index==4) {
        if(c.meta[0] || !c.meta[1] || c.meta[1]>s.h[4] || c.offset+14+c.meta[1]>1024)return bad();
        c.key_length=c.meta[1];c.character=0;c.comparison=0;c.phase=P::RestartValidate;
      }continue;
    }
    if(c.phase==P::RestartValidate) {
      if(!block_work(b,out.cost))return out;
      const auto value=s.bytes[c.offset+14+c.character];
      std::uint32_t lower_n=0;const auto* lower=s.fence_data(c.block,lower_n);
      if(!c.restart && (c.key_length!=lower_n || value!=lower[c.character]))return bad();
      if(c.restart && !c.comparison)c.comparison=c.character<c.previous_length ? (value<c.key[c.character]?-1:value>c.key[c.character]?1:0) : 1;
      c.key[c.character]=value;
      if(++c.character==c.key_length) {
        if(c.restart && (!c.comparison?c.key_length<=c.previous_length:c.comparison<0))return bad();
        c.previous_length=c.key_length;c.last_offset=c.offset;c.compare_reset();c.phase=P::RestartUpper;
      }continue;
    }
    if(c.phase==P::RestartUpper) {
      if(c.block+1<s.h[5]) {
        if(!c.nav_compare(s,c.block+1,{reinterpret_cast<const char*>(c.key.data()),c.key_length},b,out.cost))return out;
        if(c.comparison<=0)return bad();
      }
      if(++c.restart==s.restarts){s.valid=true;c.lo=c.hinted?std::min(c.hint_restart,s.restarts-1):0;c.hi=s.restarts;c.mid=kBlockNone;c.compare_reset();c.phase=P::RestartSearch;}
      else{c.index=0;c.offset=0;c.phase=P::RestartOffsets;}
      continue;
    }
    if(c.phase==P::RestartSearch) {
      if(c.lo==c.hi) {
        if(!c.lo)return bad();
        c.restart=c.lo-1;c.record=c.restart*16;c.end=std::min(s.records,(c.restart+1)*16);
        c.at=block16(s.bytes.data()+8+2*c.restart);c.index=0;c.previous_length=0;c.phase=P::DecodeHeader;continue;
      }
      if(c.mid==kBlockNone){c.mid=c.lo+(c.hi-c.lo)/2;c.compare_reset();}
      const auto at=block16(s.bytes.data()+8+2*c.mid);
      if(!c.compare(s.bytes.data()+at+14,s.bytes[at+1],code,b,out.cost))return out;
      if(c.comparison<=0)c.lo=c.mid+1;else c.hi=c.mid;c.mid=kBlockNone;continue;
    }
    if(c.phase==P::DecodeHeader) {
      if(c.at+14>1024)return bad();
      if(!block_work(b,out.cost))return out;
      c.meta[c.index]=s.bytes[c.at+c.index];
      if(++c.index==14) {
        const auto prefix=c.meta[0],suffix=c.meta[1];
        if(prefix>c.previous_length || !suffix || prefix+suffix>s.h[4] || c.at+14+suffix>1024 ||
           (c.record%16==0 && prefix))return bad();
        c.start=bo32(c.meta.data()+2);c.count=bo32(c.meta.data()+6);c.paths=bo32(c.meta.data()+10);
        if(!c.count || c.start>s.h[3] || c.count>s.h[3]-c.start || c.paths<c.count)return bad();
        const std::uint32_t boundary=c.restart+1<s.restarts?block16(s.bytes.data()+8+2*(c.restart+1)):1024U;
        if(c.at+14+suffix>boundary || (c.record+1==c.end && c.restart+1<s.restarts && c.at+14+suffix!=boundary))return bad();
        c.key_length=prefix+suffix;c.character=0;c.record_order=0;c.phase=P::DecodeCharacters;
      }continue;
    }
    if(c.phase==P::DecodeCharacters) {
      if(!block_work(b,out.cost))return out;
      // Prefix remains in owned storage. Charge each retained or copied byte.
      const auto value=c.character>=c.meta[0]?s.bytes[c.at+14+c.character-c.meta[0]]:c.key[c.character];
      if(c.record%16 && !c.record_order)c.record_order=c.character<c.previous_length?
          (value<c.key[c.character]?-1:value>c.key[c.character]?1:0):1;
      c.key[c.character]=value;
      if(++c.character==c.key_length) {
        if(c.record%16 && (!c.record_order?c.key_length<=c.previous_length:c.record_order<0))return bad();
        c.compare_reset();c.phase=P::DecodeBounds;
      }
      continue;
    }
    if(c.phase==P::DecodeBounds) {
      if(c.block+1<s.h[5]) {
        if(!c.nav_compare(s,c.block+1,{reinterpret_cast<const char*>(c.key.data()),c.key_length},b,out.cost))return out;
        if(c.comparison<=0)return bad();
      }
      c.compare_reset();c.phase=P::DecodeCompare;continue;
    }
    if(c.phase==P::DecodeCompare) {
      if(!c.compare(c.key.data(),c.key_length,code,b,out.cost))return out;
      if(c.comparison==0)return done(true);
      if(c.comparison>0)return done(false);
      if(++c.record==c.end)return done(false);
      c.previous_length=c.key_length;c.at+=14+c.meta[1];
      // A restart boundary cannot be crossed by the preceding group's records.
      if(c.restart+1<s.restarts && c.at>=block16(s.bytes.data()+8+2*(c.restart+1)))return bad();
      c.index=0;c.phase=P::DecodeHeader;continue;
    }
  }
  return out;
}
}
