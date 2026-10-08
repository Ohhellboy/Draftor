#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ime_device {
enum class FileRole:unsigned {Shared,SchemeData,SchemeProof,OrderData,OrderProof,Font,Other,Count};
inline FileRole fileRole(std::string_view path){
 const auto at=path.find_last_of('/');const auto name=path.substr(at==std::string_view::npos?0:at+1);
 if(name=="shared.hnv"||name=="shared.hns")return FileRole::Shared;
 if(name=="matched28.hbf1")return FileRole::Font;
 if(name=="xiaohe.hnm"||name=="full_pinyin.hnm")return FileRole::SchemeData;
 if(name=="xiaohe.hnv"||name=="full_pinyin.hnv")return FileRole::SchemeProof;
 if(name=="xiaohe_base.hbo"||name=="full_pinyin_base.hbo")return FileRole::OrderData;
 if(name=="xiaohe_base.hnv"||name=="full_pinyin_base.hnv")return FileRole::OrderProof;
 return FileRole::Other;
}
struct RoleIo {
 uint64_t requests=0,hits=0,reads=0,seeks=0,bytes=0,read_us=0,seek_us=0,max_seek_us=0,max_read_us=0;
 uint64_t position_matches=0,backward=0,forward=0,max_jump=0,max_offset=0,errors=0;
 uint32_t sizes[7]{},jumps[6]{}; // Marginal bounded histograms, not reconstructed per-call pairs.
 static unsigned jumpBucket(size_t n){constexpr size_t limits[]={512,4096,65536,1048576,16777216,SIZE_MAX};unsigned i=0;while(i<5&&n>limits[i])++i;return i;}
};
// POSIX call costs, not SPI transactions. Fixed buckets; no per-read logging.
enum class IoKind : unsigned { Sequential, Positioned, Seek, CacheHit, DocumentRead, DocumentWrite, DocumentSync, Count };
struct IoSample {
 uint64_t calls=0,bytes=0,us=0,max_us=0,errors=0;
 void add(size_t actual,uint64_t elapsed,bool ok){++calls;bytes+=actual;us+=elapsed;max_us=std::max(max_us,elapsed);errors+=!ok;}
};
struct IoProfile {
 static constexpr unsigned Sizes=7;
 static constexpr size_t Limits[Sizes]={16,64,256,512,1024,4096,SIZE_MAX};
 IoSample samples[unsigned(IoKind::Count)][Sizes]{};
 uint64_t logical[Sizes]{},logical_hits[Sizes]{};
 IoSample resource_during_save{};
 bool save_active=false;
 RoleIo roles[unsigned(FileRole::Count)]{};
 static const char* roleName(unsigned n){constexpr const char* names[]={"shared_packet","scheme_data","scheme_proof","order_data","order_proof","font","other"};return names[n];}
 static unsigned bucket(size_t n){unsigned i=0;while(i+1<Sizes&&n>Limits[i])++i;return i;}
 void request(size_t n,bool hit){const auto i=bucket(n);++logical[i];logical_hits[i]+=hit;}
 void record(IoKind kind,size_t requested,size_t actual,uint64_t us,bool ok){
  samples[unsigned(kind)][bucket(requested)].add(actual,us,ok);
  if(save_active&&(kind==IoKind::Sequential||kind==IoKind::Positioned))resource_during_save.add(actual,us,ok);
 }
 static const char* name(unsigned k){constexpr const char* names[]={"sequential","positioned","seek","cache_hit","document_read","document_write","document_fsync"};return names[k];}
};
inline IoProfile ioProfile;
}
