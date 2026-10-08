#include "libraries/BLE/src/hanime_ble_ownership.hpp"
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
struct Service {static unsigned live;Service(){++live;}~Service(){--live;}};unsigned Service::live=0;
struct Descriptor {static unsigned made,dead,live;Descriptor(){++made;++live;}~Descriptor(){++dead;--live;}};
unsigned Descriptor::made=0,Descriptor::dead=0,Descriptor::live=0;
struct ThrowAliases:std::map<std::string,Descriptor*> {
 bool fail=false;
 void emplace(const std::string& key,Descriptor* d){if(fail)throw std::bad_alloc();std::map<std::string,Descriptor*>::emplace(key,d);}
};
struct ThrowOwners:std::map<uint16_t,Descriptor*> {
 bool fail=false;
 auto emplace(uint16_t key,Descriptor* d){if(fail)throw std::bad_alloc();return std::map<uint16_t,Descriptor*>::emplace(key,d);}
};
void descriptors(){
 for(unsigned round=0;round<1000;++round){
  ThrowAliases aliases;ThrowOwners owners;
  auto discover=[&](unsigned h){return hanime_ble::retainDescriptor(aliases,owners,h,[]{return new Descriptor;},[](Descriptor*){return std::string("2902");});};
  auto* first=discover(1);assert(discover(1)==first&&Descriptor::live==1);
  auto* second=discover(2);assert(first!=second&&aliases.size()==1&&owners.size()==2&&Descriptor::live==2);
  // Partial discovery failure leaves live references intact, then retry deduplicates.
  assert(discover(1)==first&&discover(2)==second);
  aliases.fail=true;try{discover(3);assert(false);}catch(const std::bad_alloc&){}
  assert(owners.size()==2&&Descriptor::live==2&&owners.at(1)==first);
  aliases.fail=false;owners.fail=true;
  try{discover(3);assert(false);}catch(const std::bad_alloc&){}
  assert(Descriptor::live==2&&owners.size()==2);owners.fail=false;
  try{hanime_ble::retainDescriptor(aliases,owners,3,[]{return new Descriptor;},[](Descriptor*)->std::string{throw std::bad_alloc();});assert(false);}catch(const std::bad_alloc&){}
  assert(Descriptor::live==2&&owners.size()==2);discover(3);assert(Descriptor::live==3);
  try{hanime_ble::retainDescriptor(aliases,owners,4,[]()->Descriptor*{throw std::bad_alloc();},[](Descriptor*){return std::string("x");});assert(false);}catch(const std::bad_alloc&){}
  assert(Descriptor::live==3);
  hanime_ble::releaseDescriptors(aliases,owners);hanime_ble::releaseDescriptors(aliases,owners);
  assert(!Descriptor::live&&Descriptor::made==Descriptor::dead);
 }
 std::printf("PASS descriptor production shared ownership: repeated handle, duplicate UUID, partial/retry, owner/alias/UUID allocation failures, constructor failure, 1000 retirements created=%u destroyed=%u live=%u\n",Descriptor::made,Descriptor::dead,Descriptor::live);
}
int main(){descriptors();
 for(unsigned cycle=0;cycle<1000;++cycle){std::map<std::string,Service*> aliases;std::map<Service*,unsigned> instances;
  for(unsigned handle=0;handle<5;++handle){auto s=new Service;aliases.insert({"duplicate HID UUID",s});instances.insert({s,handle});}
  assert(aliases.size()==1&&Service::live==5);
  hanime_ble::releaseServices(aliases,instances);assert(!Service::live&&aliases.empty()&&instances.empty());
  hanime_ble::releaseServices(aliases,instances);assert(!Service::live);
 }
 std::puts("PASS BLE shared ownership cleanup: duplicate UUID / partial discovery / repeated empty cleanup 1000 rounds; not a BLE stack test");
}
