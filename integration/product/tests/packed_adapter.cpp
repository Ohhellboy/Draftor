// Adapter mixed-format wiring. Core authentication implementation is unmodified.
#include "tf_resources.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace ime_device;
namespace fs=std::filesystem;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
struct Scratch {
 fs::path path;
 Scratch(){char name[]="packed-adapter-XXXXXX";CHECK(mkdtemp(name));path=fs::absolute(name);}
 ~Scratch(){std::error_code error;fs::remove_all(path,error);}
};
void drain(Resources& r){unsigned n=0;while(!r.ready()&&r.state()!=ResourceState::Error&&n++<100000)r.service();CHECK(n<100000);}
int main(){try{
 const auto pins=productionPins();CHECK(pins.shared->verification_format==2);
 CHECK(pins.xiaohe->verification_format==1&&pins.full_pinyin->verification_format==1&&pins.xiaohe_base->verification_format==1&&pins.full_pinyin_base->verification_format==1);
 Scratch scratch;
 for(auto name:{"xiaohe.hnm","full_pinyin.hnm"})fs::create_symlink(fs::path(RESOURCE_ROOT)/name,scratch.path/name);
 CHECK(!fs::exists(scratch.path/"shared.hns"));
 const auto handles=metrics.resource_handles;
 {
  Resources r(scratch.path.string(),std::make_shared<Mount>(),pins,INDEX_ROOT);
  const auto io=metrics.io();r.start(hanime_extension::SchemeRules::R1Xiaohe);drain(r);
  CHECK(r.ready()&&metrics.resource_handles==handles+5);
  CHECK(r.cacheBytes()==58368&&!r.sharedSnapshot().whole_file_checked);
  auto delta=metrics.io()-io;
  CHECK(delta.bytes==15400&&delta.reads==16);
  auto shared=r.sharedCacheAllocation();const auto before=metrics.phases[unsigned(Phase::SharedValidation)].steps;
  r.switchScheme(hanime_extension::SchemeRules::FullPinyin);CHECK(metrics.resource_handles==handles+1);drain(r);
  CHECK(r.ready()&&metrics.resource_handles==handles+5&&shared==r.sharedCacheAllocation());
  CHECK(metrics.phases[unsigned(Phase::SharedValidation)].steps==before);
  std::cout<<"PASS mixed production: no shared.hns; 5 handles; 15400 B/16 reads bootstrap; 58368 B auth payload; switch reuses shared\n";
 }
 CHECK(metrics.resource_handles==handles);
 // Old same-name format1 proof length cannot be accepted with the new trusted pin.
 {std::ofstream out(scratch.path/"shared.hnv",std::ios::binary);out.seekp(946175);out.put(0);}
 {
  Resources r(scratch.path.string(),std::make_shared<Mount>(),pins,scratch.path.string());r.start(hanime_extension::SchemeRules::R1Xiaohe);drain(r);
  CHECK(r.state()==ResourceState::Error&&!r.shared.ready());CHECK(r.error().find("shared")!=std::string::npos);
 }
 // Correct transport length, corrupted first authenticated packet: fail rather than Ready/full scan.
 {std::ifstream in(fs::path(INDEX_ROOT)/"shared.hnv",std::ios::binary);char packet[1024];CHECK(bool(in.read(packet,sizeof(packet))));packet[200]^=1;
  std::ofstream out(scratch.path/"shared.hnv",std::ios::binary|std::ios::trunc);out.write(packet,sizeof(packet));out.seekp(58646527);out.put(0);}
 {
  Resources r(scratch.path.string(),std::make_shared<Mount>(),pins,scratch.path.string());const auto bytes=metrics.bytes;r.start(hanime_extension::SchemeRules::R1Xiaohe);drain(r);
  CHECK(r.state()==ResourceState::Error&&!r.shared.ready());CHECK(metrics.bytes-bytes<=1024);
 }
 CHECK(metrics.resource_handles==handles);
 std::cout<<"PASS same-name old format and damaged packet rejected; no fallback; handles balanced; Host only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
