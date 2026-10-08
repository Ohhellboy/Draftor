#include "metadata_font.hpp"
#include "chinese_font.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <sstream>
#include <vector>
static bool forbidAllocation=false;
void* operator new(std::size_t n){if(forbidAllocation)std::abort();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
#define REQUIRE(x) do{if(!(x)){std::cerr<<"UI14 assertion "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
struct Expected {uint32_t cp;unsigned width,height,stride,advance;int bearing;std::array<uint8_t,28> bits;};
int main(){
 const auto file=std::filesystem::path(__FILE__).parent_path()/"data/ui14_expected.tsv";
 std::ifstream in(file);REQUIRE(in.good());std::vector<Expected> rows;std::string line;
 while(std::getline(in,line)){Expected r{};std::string hex;std::istringstream s(line);s>>r.cp>>r.width>>r.height>>r.stride>>r.advance>>r.bearing>>hex;REQUIRE(hex.size()==56);
  for(unsigned i=0;i<28;++i)r.bits[i]=uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));rows.push_back(r);
 }
 REQUIRE(rows.size()==20606);ime_device::ChineseFont cold;REQUIRE(!cold.ready()&&!cold.failed()&&!cold.allocation());
 s3app::MetadataFont face(cold);REQUIRE(face.lineHeight()==20&&face.defaultAdvance()==14);
 forbidAllocation=true;
 for(const auto& r:rows){const auto g=face.glyph(r.cp);
  REQUIRE(g.width==r.width&&g.height==r.height&&g.stride==r.stride&&g.advance==r.advance&&g.bearing==r.bearing);
  REQUIRE(g.missing==(r.cp==0x10ffff));REQUIRE(std::memcmp(g.bits,r.bits.data(),28)==0);
 }
 // Every Unicode scalar, surrogate boundary and page gap is checked against
 // the actual parent's coverage, not a demonstration phrase or guessed count.
 size_t at=0;
 for(uint32_t cp=0;cp<=0x110000;++cp){while(at+1<rows.size()&&rows[at].cp<cp)++at;
  const bool exists=at+1<rows.size()&&rows[at].cp==cp;
  REQUIRE(face.glyph(cp).missing!=exists);
 }
 for(uint32_t cp:{0U,0xd7ffU,0xd800U,0xdfffU,0xe000U,0x28fcdU,0x10ffffU,0x110000U,0xffffffffU}){
  std::array<uint8_t,30> guarded{};guarded.front()=0xa5;guarded.back()=0x5a;
  s3app::ui14::lookup(cp,guarded.data()+1);REQUIRE(guarded.front()==0xa5&&guarded.back()==0x5a);
 }
 for(unsigned n=0;n<10000;++n){s3app::MetadataFont reopened(cold);REQUIRE(!reopened.glyph(0x85cf).missing);REQUIRE(!reopened.glyph(0x6536).missing);}
 forbidAllocation=false;
 REQUIRE(!cold.ready()&&!cold.failed()&&!cold.allocation());REQUIRE(!s3app::ui14::lookup(0x4e2d,nullptr));
 std::cout<<"PASS UI14 full parent coverage, exact expected pixels/metrics, Unicode boundaries, guard bytes, cold body font, repeated construction/lookup without allocation, no TF initialization\n";
}
