#include "tf_resources.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace ime_device;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
 char name[]="reader-position-XXXXXX";const int fd=mkstemp(name);CHECK(fd>=0);close(fd);
 const std::string bytes(8192,'x');{std::ofstream f(name,std::ios::binary);f<<bytes;}
 auto mount=std::make_shared<Mount>();uint8_t out[1024]{};
 {
  TfReader r(mount,name,1);CHECK(r.read(0,out,16));CHECK(r.io().seeks==0);
  CHECK(r.read(32,out,16));CHECK(r.io().reads==1&&r.io().hits==1);
  CHECK(r.read(1024,out,1024));CHECK(r.io().seeks==0&&r.io().reads==2);
  CHECK(r.read(0,out,16));CHECK(r.io().seeks==1);
  CHECK(r.read(1024,out,1024));CHECK(r.io().seeks==1); // physical fill ended at 1024
  CHECK(truncate(name,2050)==0);CHECK(!r.read(2048,out,1024));CHECK(!r.healthy());
  CHECK(!r.read(0,out,1)); // cached bytes never resurrect a failed reader
 }
 {TfReader r(mount,name);CHECK(r.read(0,out,1));mount->epoch++;CHECK(!r.read(1,out,1));}
 CHECK(unlink(name)==0);
 std::cout<<"PASS fresh zero position, cache preserves physical position, random seek, short read/error latch and epoch\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
