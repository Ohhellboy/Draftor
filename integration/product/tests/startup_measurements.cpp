#include "tf_probe.hpp"
#include "io_profile.hpp"
#include "loading_status.hpp"
#include "tf_probe_expected.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ime_device;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
 const auto source=std::string(INDEX_ROOT)+"/shared.hnv";
 auto mount=std::make_shared<Mount>();const auto caches=metrics.cache_live_bytes;const auto handles=metrics.resource_handles;
 Resources resources(RESOURCE_ROOT,mount,productionPins(),INDEX_ROOT);resources.start(hanime_extension::SchemeRules::R1Xiaohe);resources.service();
 const auto state=resources.state();const auto work=resources.stageWork();
 unsigned cases=0;
 {
  TfProbe probe(source,mount);
  for(unsigned steps=0;!probe.finished()&&steps<1000;++steps){
   probe.service();
   if(probe.hasResult()){
    const auto r=probe.result();CHECK(r.ok&&r.crc==ProbeCrc[cases]);
    CHECK(r.io.requests==(TfProbe::cached(cases)?16:8)&&r.io.reads==8&&r.io.hits==(TfProbe::cached(cases)?8:0));
    CHECK(r.io.bytes==(TfProbe::cached(cases)?8192:8*r.request_bytes));
    CHECK(r.io.seeks==(cases<4||cases==12?1:8));
    std::cout<<"HOST_PROBE case="<<cases<<" requests="<<r.io.requests<<" physical="<<r.io.reads<<" bytes="<<r.io.bytes<<" hits="<<r.io.hits<<" read_us="<<r.io.read_us<<" seek_us="<<r.io.seek_us<<" wall_us="<<r.last_us-r.first_us<<'\n';
    ++cases;probe.acknowledge();
   }
  }
  CHECK(cases==14&&probe.finished()&&!*probe.error());
 }
 CHECK(resources.state()==state&&resources.stageWork()==work); // Probe cannot activate or advance core.
 resources.retire();CHECK(metrics.cache_live_bytes==caches&&metrics.resource_handles==handles);
 {
  TfProbe probe(source,mount);probe.service();++mount->epoch;probe.service();CHECK(probe.finished()&&*probe.error());
  TfProbe newer(source,mount);newer.service();probe.cancel();newer.service();CHECK(!newer.finished()); // Original cancellation cannot close a new fd.
 }
 {
  TfProbe probe(source,mount);mount->available=false;probe.service();CHECK(probe.finished()&&*probe.error());mount->available=true;
 }
 {TfProbe probe(source,mount);probe.expireAt(probe.requestedUs()+TfProbe::LimitUs);CHECK(probe.finished()&&*probe.error());}
 const auto scratch=std::filesystem::current_path()/"probe-corrupt-sparse.hns";
 {std::ofstream f(scratch,std::ios::binary);f.seekp(ProbeSharedBytes-1);f.put(0);}
 {
  TfProbe probe(scratch.string(),mount);for(unsigned i=0;i<20&&!probe.finished();++i)probe.service();
  CHECK(probe.finished()&&probe.hasResult()&&!probe.result().ok&&*probe.error());
 }
 std::filesystem::resize_file(scratch,1);
 {TfProbe probe(scratch.string(),mount);probe.service();CHECK(probe.finished()&&*probe.error());}
 std::filesystem::remove(scratch);CHECK(metrics.resource_handles==handles&&metrics.cache_live_bytes==caches);
 CHECK(ioProfile.samples[unsigned(IoKind::Sequential)][4].calls>0);
 CHECK(ioProfile.samples[unsigned(IoKind::Positioned)][4].calls>0);
 CHECK(ioProfile.samples[unsigned(IoKind::Sequential)][5].calls>0&&ioProfile.samples[unsigned(IoKind::Positioned)][5].calls>0);
 CHECK(ioProfile.logical_hits[0]>=8&&ioProfile.logical_hits[1]>=8);
 char mode[64];modeCaption(mode,sizeof(mode),false,hanime_extension::SchemeRules::R1Xiaohe,true);CHECK(std::string(mode)=="EN | UNSAVED");
 modeCaption(mode,sizeof(mode),true,hanime_extension::SchemeRules::FullPinyin,false);CHECK(std::string(mode)=="CN PY | SAVED");
 CHECK(receivedUs(10000000,10000,9980)==9980000);
 CHECK(receivedUs(5000000000ULL,10,UINT32_MAX-9)==4999980000ULL); // millis wrap, same boot.
 CHECK(receivedUs(10,100,0)==0); // Never wrap an impossible age into a huge timestamp.
 std::cout<<"PASS Host production sample bytes, sequential/random/cache accounting, original epoch/cancel, corruption/short file, release, core state unchanged, explicit EN and timestamp wrap; NO DEVICE PERFORMANCE CLAIM\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
