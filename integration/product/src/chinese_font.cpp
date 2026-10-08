#include "chinese_font.hpp"
#include "rare_glyphs.hpp"
#include "vertical_punctuation_glyph.hpp"
#include <array>
#include <cstring>
namespace ime_device {
namespace {
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
constexpr auto missing=[](){std::array<uint8_t,112> b{};for(unsigned y=2;y<26;++y)for(unsigned x=2;x<26;++x)if(x==2||x==25||y==2||y==25||x==y)b[y*4+x/8]|=uint8_t(0x80>>(x%8));return b;}();
constexpr uint8_t supplementary[]={0,0,0,0,5,129,16,0,15,195,48,0,25,163,48,0,57,178,32,0,25,182,12,0,25,135,252,0,25,142,96,0,127,254,96,0,25,150,96,0,25,135,252,0,25,166,96,0,28,182,96,0,28,230,96,0,24,230,108,0,120,231,252,0,120,198,96,0,24,198,96,0,25,230,96,0,25,102,100,0,26,55,254,0,60,62,0,0,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // Unchanged CC0 Jigmo U+28FCD bitmap.
}
void ChineseFont::fail(const char* error){error_=error;failed_=true;ready_=false;reader_.reset();bytes_.reset();}
bool ChineseFont::start(const std::string& path,std::shared_ptr<const Mount> mount){
 ready_=failed_=false;error_="";filled_=index_=0;crc_=0xffffffff;prior_=0;
 reader_=std::make_unique<TfReader>(std::move(mount),path);
 if(!reader_->valid()){fail("font file unavailable");return false;}
 if(reader_->size()!=Bytes){fail("font size mismatch");return false;}
 size_=reader_->size();bytes_.reset(static_cast<uint8_t*>(draftor::memory::allocate(size_)));
 if(!bytes_){fail("font PSRAM allocation");return false;}return true;
}
bool ChineseFont::begin(){
#ifdef ASSET_ROOT
 if(!start(std::string(ASSET_ROOT)+"/matched28.hbf1",std::make_shared<Mount>()))return false;
 while(!ready_&&!failed_)service();
 return ready_;
#else
 return false; // Device must schedule service; no synchronous full-font load.
#endif
}
bool ChineseFont::checkHeader(){
 const auto* b=bytes_.get();return std::memcmp(b,"HBF1\1\0\34\0\34\0\4\0",12)==0&&u32(b+12)==Glyphs&&u32(b+16)==28&&u32(b+20)==DataOffset&&u32(b+24)==0xfa193df0U;
}
void ChineseFont::service(){
 if(ready_||failed_||!reader_)return;
 const auto start=clockUs();const auto io=metrics.io();
 if(filled_<size_){
  const size_t n=std::min(ReadBytes,size_-filled_);
  if(!reader_->readBlock(filled_,bytes_.get()+filled_,n)){metrics.step(Phase::FontLoad,start,1,n,0,io);fail("font read failed");return;}
  metrics.step(Phase::FontLoad,start,1,n,0,io);
  const auto checkStart=clockUs();const auto checkIo=metrics.io();
  const size_t end=filled_+n;
  for(size_t i=filled_;i<end;++i){crc_^=bytes_[i];for(unsigned b=0;b<8;++b)crc_=(crc_>>1)^(0xedb88320U&(0U-(crc_&1U)));}
  filled_=end;
  if(filled_==n&&!checkHeader()){metrics.step(Phase::FontContent,checkStart,0,0,n,checkIo);fail("font header mismatch");return;}
  metrics.step(Phase::FontContent,checkStart,0,0,n,checkIo);return;
 }
 if(~crc_!=0xeee33093U){fail("font CRC mismatch");return;}
 const size_t priorIndex=index_;const size_t end=std::min(index_+32,Glyphs);
 for(;index_<end;++index_){const auto* rec=bytes_.get()+28+index_*8;const auto cp=u32(rec);
  if((index_&&cp<=prior_)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||u32(rec+4)!=index_*112){fail("font index invalid");return;}prior_=cp;
 }
 if(index_==Glyphs){ready_=true;reader_.reset();}
 metrics.step(Phase::FontIndex,start,0,0,index_-priorIndex,io);
}
s3host::GlyphView ChineseFont::glyph(uint32_t cp)const noexcept{
 if(cp==0xfe31)return {verticalDash.data(),28,28,4,28,0,false};
 const auto latin=latin_.glyph(cp);if(!latin.missing)return latin;
 const uint8_t* bits=nullptr;
 if(cp==0x28fcd)bits=supplementary;
 else if(ready_){size_t lo=0,hi=Glyphs;while(lo<hi){const auto mid=(lo+hi)/2;if(u32(bytes_.get()+28+mid*8)<cp)lo=mid+1;else hi=mid;}
  if(lo<Glyphs&&u32(bytes_.get()+28+lo*8)==cp)bits=bytes_.get()+DataOffset+lo*112;
 }
 if(!bits){size_t lo=0,hi=sizeof(rareGlyphs)/sizeof(rareGlyphs[0]);while(lo<hi){const auto mid=(lo+hi)/2;if(rareGlyphs[mid].cp<cp)lo=mid+1;else hi=mid;}if(lo<sizeof(rareGlyphs)/sizeof(rareGlyphs[0])&&rareGlyphs[lo].cp==cp)bits=rareGlyphs[lo].bits;}
 return {bits?bits:missing.data(),28,28,4,28,0,bits==nullptr};
}
}
