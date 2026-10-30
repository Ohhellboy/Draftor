// Finite Host fault injection: the real namespace and VFS implementations,
// plus the exact WS startup gate extracted from device_main.cpp by the runner.
#include "board_namespace.hpp"
#include "device_metrics.hpp"
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
namespace fs=std::filesystem;
using Result=draftor::board::NamespaceResult;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace fault {
bool armed=false,hit=false,after_open=false;
std::size_t at=0,count=0;
unsigned original_opens=0,marker_opens=0;
void* allocate(std::size_t n){
 if(armed&&++count==at){hit=true;after_open=marker_opens!=0;throw std::bad_alloc();}
 if(void* p=std::malloc(n?n:1))return p;
 throw std::bad_alloc();
}
void observe(const char* path,const char*){
 if(std::strstr(path,"/HANIME_S3_TEST/"))++original_opens;
 if(std::strstr(path,"/.draftor-namespace"))++marker_opens;
}
}
void* operator new(std::size_t n){return fault::allocate(n);}
void* operator new[](std::size_t n){return fault::allocate(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
namespace ime_device {uint64_t clockUs() noexcept{return 0;}}
namespace draftor::board {const char* MountPoint=nullptr;}
namespace boot {
const char* fatal=nullptr;
unsigned draws=0,mounted=0,app_admissions=0;
void draw(){++draws;}
struct Storage {void noteMounted(){++mounted;}} storage;
void enter(){
#include "namespace_startup.inc"
 ++app_admissions; // Boundary immediately after the production gate.
}
void reset(){fatal=nullptr;draws=mounted=app_admissions=0;}
}
void put(const fs::path& p,const std::string& bytes){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<bytes;CHECK(out.good());}
std::string get(const fs::path& p){std::ifstream in(p,std::ios::binary);CHECK(in.good());return {std::istreambuf_iterator<char>(in),{}};}
void protectedFiles(const fs::path& p,bool create){
 for(const char* rel:{"HANIME_S3_TEST/v01/CURRENT.A","HANIME_S3_TEST/v01/documents/old.txt","HANIME/IME/shared.sentinel","HANIME/IME/font.sentinel","PICO2W/old.sentinel"}){
  if(create)put(p/rel,std::string("protected:")+rel);else CHECK(get(p/rel)==std::string("protected:")+rel);
 }
}
Result attempt(const fs::path& p,std::size_t failAt,bool gate=false){
 fault::count=0;fault::at=failAt;fault::hit=false;fault::after_open=false;fault::original_opens=fault::marker_opens=0;
 boot::reset();draftor::board::MountPoint=p.c_str();
 s3fs::File::openObserver=fault::observe;
 Result result=Result::Invalid;
 fault::armed=true;
 // noexcept is also a compile-time contract; an escaped exception terminates
 // this process and fails the runner rather than being mistaken for success.
 static_assert(noexcept(draftor::board::prepareNamespace(p.c_str())));
 if(gate)boot::enter();else result=draftor::board::prepareNamespace(p.c_str());
 fault::armed=false;s3fs::File::openObserver=nullptr;
 CHECK(!fault::original_opens);CHECK(ime_device::metrics.document_handles==0);
 if(gate){
  if(fault::hit){CHECK(boot::draws==1&&!boot::mounted&&!boot::app_admissions);CHECK(boot::fatal&&!std::strcmp(boot::fatal,"WS data space memory failed"));result=Result::OutOfMemory;}
  else{CHECK(!boot::draws&&boot::mounted==1&&boot::app_admissions==1&&!boot::fatal);result=Result::Ready;}
 }
 return result;
}
void sweep(const fs::path& base,bool existing){
 unsigned failures=0,partial=0,open_phase=0;std::size_t positions=0;
 for(unsigned i=0;i<256;++i){
  char name[32];std::snprintf(name,sizeof(name),"%s-%04u",existing?"valid":"fresh",i);const auto p=base/name;
  protectedFiles(p,true);
  if(existing)put(p/"WAVESHARE_RLCD42/.draftor-namespace",draftor::board::NamespaceIdentity);
  const auto result=attempt(p,i);const auto count=fault::count;
  if(i==0){CHECK(result==Result::Ready);positions=count;CHECK(positions>2&&positions<255);}
  else{
   CHECK(fault::hit&&result==Result::OutOfMemory);++failures;open_phase+=fault::after_open;
   CHECK(!fs::exists(p/"WAVESHARE_RLCD42/HANIME_S3_TEST"));
   if(existing){CHECK(get(p/"WAVESHARE_RLCD42/.draftor-namespace")==draftor::board::NamespaceIdentity);CHECK(attempt(p,0)==Result::Ready);}
   else if(fs::exists(p/"WAVESHARE_RLCD42")){
    ++partial;CHECK(!fs::exists(p/"WAVESHARE_RLCD42/.draftor-namespace"));
    CHECK(attempt(p,0)==Result::Invalid);CHECK(fs::is_empty(p/"WAVESHARE_RLCD42"));
   }else CHECK(attempt(p,0)==Result::Ready);
  }
  protectedFiles(p,false);fs::remove_all(p);
  // Repeat each failure through the exact device startup gate, in a fresh fixture.
  if(i){protectedFiles(p,true);if(existing)put(p/"WAVESHARE_RLCD42/.draftor-namespace",draftor::board::NamespaceIdentity);CHECK(attempt(p,i,true)==Result::OutOfMemory);protectedFiles(p,false);fs::remove_all(p);}
  if(i==positions)break;
 }
 CHECK(failures==positions&&open_phase>=2);if(!existing)CHECK(partial>0);
 std::cout<<"PASS "<<(existing?"valid_marker":"new_namespace")<<": "<<failures<<" allocation sites, "<<open_phase<<" in marker open/handle phase, "<<partial<<" incomplete directories refused; repeated through production startup gate; Host counts only\n";
}
void ioAndUnknown(const fs::path& base){
 for(unsigned i=0;i<7;++i){
  auto p=base/("io-"+std::to_string(i));protectedFiles(p,true);
  if(i==0)s3fs::faults.write_bytes=2;
  if(i==1)s3fs::faults.flush=true;
  if(i==2)s3fs::faults.close=true;
  if(i==3){put(p/"WAVESHARE_RLCD42/.draftor-namespace",draftor::board::NamespaceIdentity);s3fs::faults.read=true;}
  if(i==4)fs::create_directories(p/"WAVESHARE_RLCD42");
  if(i==5)put(p/"WAVESHARE_RLCD42/.draftor-namespace","unknown marker");
  if(i==6)put(p/"WAVESHARE_RLCD42","not a directory");
  CHECK(attempt(p,0)==Result::Invalid);s3fs::faults={};
  if(i==0||i==4||i==5||i==6)CHECK(attempt(p,0)==Result::Invalid);
  if(i==3)CHECK(get(p/"WAVESHARE_RLCD42/.draftor-namespace")==draftor::board::NamespaceIdentity);
  if(i==5)CHECK(get(p/"WAVESHARE_RLCD42/.draftor-namespace")=="unknown marker");
  protectedFiles(p,false);fs::remove_all(p);
 }
 std::cout<<"PASS marker short-write/sync/close/read errors, absent/corrupt marker and file obstruction; no original data fallback\n";
}
int main(){try{
 auto base=fs::absolute("namespace-allocation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));CHECK(fs::create_directory(base));
 sweep(base,false);sweep(base,true);ioAndUnknown(base);fs::remove_all(base);
 std::cout<<"PASS namespace allocation failure contract; no device or real card used\n";
 return 0;
}catch(const std::exception& e){fault::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
