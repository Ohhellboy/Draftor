#pragma once
#include "writing_app.hpp"
#include "hanime/core.hpp"
namespace ime_device {
struct Target {
 s3app::ds::DocumentId document;
 uint64_t session=0,input=0,revision=0;
 s3app::EditStamp stamp;
 size_t cursor=0,begin=0,end=0;bool selected=false,rename=false;
};
class EditorSink final:public hanime::TextCommitSink {
 s3app::App& app_;Target target_{};bool armed_=false;
 uint64_t accepted_us_=0;
 public:
 explicit EditorSink(s3app::App& app):app_(app){}
 bool bind(uint64_t input,uint64_t revision);
 bool current()const;
 bool matches(const Target&)const;
 const Target& target()const{return target_;}
 uint64_t acceptedUs()const{return accepted_us_;}
 void invalidate(){armed_=false;}
 bool commit(std::string_view) override;
};
}
