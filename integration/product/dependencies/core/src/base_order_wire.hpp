#pragma once
#include "hanime/exact_ranker.hpp"
#include <array>
#include <algorithm>
#include <cstring>
namespace hanime::detail {
constexpr std::uint32_t kBaseMagic=0x314f4248; // HBO1; all integers LE
constexpr std::size_t kBaseHeader=64, kBaseItem=64, kBaseMember=8, kBaseMaxCode=128;
inline std::uint32_t bo32(const std::uint8_t* p) {
  return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
inline std::uint64_t bo64(const std::uint8_t* p) {return bo32(p)|(std::uint64_t(bo32(p+4))<<32);}
inline void put32(std::uint8_t* p,std::uint32_t x) {for (unsigned i=0;i<4;++i) p[i]=std::uint8_t(x>>(8*i));}
inline void put64(std::uint8_t* p,std::uint64_t x) {for (unsigned i=0;i<8;++i) p[i]=std::uint8_t(x>>(8*i));}
inline std::int32_t signed32(std::uint32_t x) {
  return x<=0x7fffffffU ? std::int32_t(x) : -1-std::int32_t(~x);
}
inline std::int64_t signed64(std::uint64_t x) {
  return x<=0x7fffffffffffffffULL ? std::int64_t(x) : -1-std::int64_t(~x);
}
struct BaseHeader {
  std::uint32_t width{0}, codes{0}, groups{0}, max_code{0};
  std::uint64_t rank_offset{0}, member_offset{0}, file_bytes{0};
  std::uint32_t format{1}, directory_pages{0};
};
inline bool base_header(const std::uint8_t* p,std::size_t bytes,BaseHeader& h) {
  h={bo32(p+8),bo32(p+12),bo32(p+16),bo32(p+20),bo64(p+40),bo64(p+48),bo64(p+56)};
  h.format=bo32(p+4);
  h.directory_pages=bo32(p+28);
  const bool flat=h.format==1 && bo32(p)==kBaseMagic && !h.directory_pages &&
      bo64(p+32)==kBaseHeader && h.rank_offset==kBaseHeader+std::uint64_t(h.width)*h.codes;
  const bool paged=h.format==2 && bo32(p)==0x324f4248U && h.codes && h.directory_pages &&
      bo64(p+32)==1024 && h.rank_offset==1024+std::uint64_t(h.directory_pages)*1024;
  return (flat || paged) && bo32(p+24)==kBaseItem &&
      h.max_code && h.max_code<=kBaseMaxCode && h.width==h.max_code+16 &&
      h.member_offset==h.rank_offset+std::uint64_t(kBaseItem)*h.groups &&
      h.file_bytes==h.member_offset+std::uint64_t(kBaseMember)*h.groups && h.file_bytes==bytes;
}
// dir: length, start group, count, raw paths, code[max_code] padded with zeroes.
inline bool base_item(const std::uint8_t* p,RuntimeLexemeRef max,RankItem& x) {
  std::copy_n(p,32,x.key.bytes.begin());
  x.score=signed64(bo64(p+32)); x.ref=bo32(p+40); x.scheme_weight=signed32(bo32(p+44));
  x.path_count=bo32(p+48); x.representative_ordinal=bo32(p+52); x.effective_count=0;
  return !x.key.empty() && x.ref && x.ref<=max && x.path_count && !bo64(p+56);
}
inline void encode_item(std::uint8_t* p,const RankItem& x) {
  std::copy(x.key.bytes.begin(),x.key.bytes.end(),p);
  put64(p+32,std::uint64_t(x.score)); put32(p+40,x.ref); put32(p+44,std::uint32_t(x.scheme_weight));
  put32(p+48,x.path_count); put32(p+52,x.representative_ordinal); put64(p+56,0);
}
} // namespace hanime::detail
