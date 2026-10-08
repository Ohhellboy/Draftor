// Round 3 release-only counterexamples through the actual product Resources.
#include "tf_resources.hpp"
#include "resource_release.hpp"
#include "wubi_auto_safe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <vector>
namespace old_release {
#include "../../../reference/UI14_pins/release_pin.hpp"
#include "../../../reference/UI14_pins/wubi_auto_safe_v1.hpp"
}
using namespace ime_device;
namespace fs=std::filesystem;
using Rules=hanime_extension::SchemeRules;
#define REQUIRE(x) do {if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
bool ready(const fs::path& root,ReleasePins pins,Rules rule,bool installation=false){
 Resources r(root.string(),std::make_shared<Mount>(),pins,root.string());
 if(installation&&!r.checkInstallation()){std::cout<<"REJECT_INSTALL "<<root.filename()<<' '<<r.error()<<'\n';return false;}
 r.start(rule);
 for(size_t i=0;i<100000&&!r.ready()&&r.state()!=ResourceState::Error;++i)r.service();
 std::cout<<(r.ready()?"READY ":"REJECT_AUTH ")<<root.filename()<<' '<<int(rule)<<' '<<r.error()<<'\n';
 return r.ready();
}
int main(){try{
 const auto root=fs::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
 const auto current=root/"integration/resources/staged";
 const auto parent=root/"reference/UI14_resources";
 const auto temp=fs::current_path()/("lexicon_release_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(temp);
 const DirectRelease oldDirect{"wubi86",&old_release::wubi_release_v1::wubi86,&old_release::wubi_release_v1::wubi86_base,"wubi86.hnm","wubi86.hnv","wubi86_base.hbo","wubi86_base.hnv",&old_release::wubi_release_v1::auto_safe};
 const ReleasePins oldPins{&old_release::hanime_release::shared,&old_release::hanime_release::xiaohe,&old_release::hanime_release::full_pinyin,&old_release::hanime_release::xiaohe_base,&old_release::hanime_release::full_pinyin_base,&oldDirect};
 const auto pins=productionPins();unsigned mismatches=0;
 for(const auto rule:{Rules::R1Xiaohe,Rules::FullPinyin,Rules::DirectExact}){
  REQUIRE(ready(current,pins,rule));REQUIRE(ready(parent,oldPins,rule));
  REQUIRE(!ready(current,oldPins,rule));REQUIRE(!ready(parent,pins,rule));mismatches+=2;
 }
 auto mixed=pins;mixed.direct=&oldDirect;REQUIRE(!ready(current,mixed,Rules::DirectExact));
 mixed=oldPins;mixed.direct=pins.direct;REQUIRE(!ready(parent,mixed,Rules::DirectExact));mismatches+=2;
 std::vector<fs::path> files;
 for(const auto& e:fs::directory_iterator(current))if(e.path().extension()==".hns"||e.path().extension()==".hnm"||e.path().extension()==".hbo"||e.path().extension()==".hnv")files.push_back(e.path());
 REQUIRE(files.size()==14);
 auto stage=[&](const std::string& name,const std::string& missing){
  auto d=temp/name;fs::create_directory(d);
  for(const auto& f:files)if(f.filename()!=missing)fs::create_symlink(f,d/f.filename());
  std::ofstream(d/"CANDIDATE_READY.json")<<"{\"fixture\":true}";return d;
 };
 auto complete=stage("complete","");REQUIRE(ready(complete,pins,Rules::FullPinyin,true));
 for(const auto& f:files)REQUIRE(!ready(stage("missing_"+f.filename().string(),f.filename().string()),pins,Rules::FullPinyin,true));
 auto partial=stage("staging_marker","");std::ofstream(partial/"STAGING_INCOMPLETE")<<"incomplete";REQUIRE(!ready(partial,pins,Rules::FullPinyin,true));
 auto noMarker=stage("no_ready_marker","");fs::remove(noMarker/"CANDIDATE_READY.json");REQUIRE(!ready(noMarker,pins,Rules::FullPinyin,true));
 const auto& a=old_release::wubi_release_v1::auto_safe;const auto& b=*pins.direct->auto_safe;
 REQUIRE(a.bytes==b.bytes);for(size_t i=0;i<a.bytes;++i)REQUIRE(a.bits[i]==b.bits[i]);size_t positives=0;
 for(unsigned i=0;i<390625;++i){unsigned n=i;std::string code(4,'a');for(int j=3;j>=0;--j){code[j]=char('a'+n%25);n/=25;}
  const bool old=wubiAutoSafe(code,&a,a.shared,a.scheme,a.order),now=wubiAutoSafe(code,&b,pins.shared,pins.direct->scheme,pins.direct->order);
  REQUIRE(old==now);REQUIRE(!wubiAutoSafe(code,&a,pins.shared,pins.direct->scheme,pins.direct->order));REQUIRE(!wubiAutoSafe(code,&b,a.shared,a.scheme,a.order));positives+=now;
 }
 std::cout<<"PASS release counterexamples: "<<mismatches<<" pin/resource mismatches,14 missing files,2 installation markers;390625 unchanged Wubi decisions positive="<<positives<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
