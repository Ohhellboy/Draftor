#pragma once
#include "font_port.hpp"
#include "tf_resources.hpp"
#include "core_memory.hpp"
namespace ime_device {
class ChineseFont final:public s3host::FontPort {
 s3host::BuiltinLatinFont latin_;
 std::unique_ptr<TfReader> reader_;draftor::memory::ArrayPtr<uint8_t> bytes_;
 size_t filled_=0,index_=0,size_=0;uint32_t crc_=0xffffffff,prior_=0;
 bool ready_=false,failed_=false;const char* error_="";
 bool checkHeader();void fail(const char*);
 public:
 static constexpr size_t Glyphs=20534,DataOffset=28+Glyphs*8,Bytes=DataOffset+Glyphs*112;
 static constexpr size_t ReadBytes=TfReader::MaximumBlockRead;
 bool begin(); // Host convenience only, consumes the same cooperative loader.
 bool start(const std::string&,std::shared_ptr<const Mount>);
 void service();
 bool ready()const{return ready_;}bool failed()const{return failed_;}
 const char* error()const{return error_;}
 size_t loadedBytes()const{return filled_;}
 size_t checkedGlyphs()const{return index_;}
 const void* allocation()const{return bytes_.get();}
 s3host::GlyphView glyph(uint32_t)const noexcept override;
 uint16_t lineHeight()const noexcept override{return 34;}
 uint16_t defaultAdvance()const noexcept override{return 28;}
};
}
