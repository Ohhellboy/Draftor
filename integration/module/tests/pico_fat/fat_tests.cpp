#include "sdfat_inbox.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include <vector>
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace draftor::usb;
using draftor::usb::pico::SdFatInbox;
#define CHECK(condition) do { if(!(condition)){std::cerr<<__LINE__<<": "#condition"\n";std::exit(1);} }while(false)
class Disk final : public BlockDevice, public FsBlockDevice {
 public:
  std::vector<uint8_t> bytes=std::vector<uint8_t>(8*1024*1024);
  std::size_t reads=0,writes=0;bool failSync=false;
  uint32_t sectorCount() const noexcept override{return bytes.size()/512;}
  Sector_t sectorCount() override{return bytes.size()/512;}
  IoResult read(uint64_t at,void* p,size_t n) noexcept override {
    ++reads;if(at>bytes.size()||n>bytes.size()-at)return{Status::Bounds,0};
    std::memcpy(p,bytes.data()+at,n);return{Status::Ok,n};
  }
  IoResult write(uint64_t at,const void* p,size_t n) noexcept override {
    ++writes;if(at>bytes.size()||n>bytes.size()-at)return{Status::Bounds,0};
    std::memcpy(bytes.data()+at,p,n);return{Status::Ok,n};
  }
  Status sync() noexcept override{return failSync?Status::SyncFailed:Status::Ok;}
  bool syncDevice()override{return !failSync;}bool isBusy()override{return false;}
  bool readSector(Sector_t at,uint8_t* p)override{return readSectors(at,p,1);}
  bool writeSector(Sector_t at,const uint8_t* p)override{return writeSectors(at,p,1);}
  bool readSectors(Sector_t at,uint8_t* p,size_t n)override{return read(uint64_t(at)*512,p,n*512).status==Status::Ok;}
  bool writeSectors(Sector_t at,const uint8_t* p,size_t n)override{return write(uint64_t(at)*512,p,n*512).status==Status::Ok;}
};
void writeFile(FatVolume& v,const char* name,const std::string& body,int attributes=0){
 FatFile f;CHECK(f.open(&v,name,O_CREAT|O_RDWR|O_EXCL));CHECK(f.write(body.data(),body.size())==body.size());
 CHECK(f.sync());if(attributes)CHECK(f.attrib(attributes));CHECK(f.close());
}
InboxCandidate find(SdFatInbox& inbox){InboxCandidate c;for(unsigned i=0;i<513;++i){auto s=inbox.next(c);if(s==Status::NotFound)break;CHECK(s==Status::Ok);if(isTxtCandidate(c))return c;}CHECK(false);return{};}
void roundtrip(){
 Disk disk;SdFatInbox inbox(disk);CHECK(inbox.prepare(7)==Status::Ok);FatVolume host;CHECK(host.begin(&disk,false));
 const std::string body="\xEF\xBB\xBF" "first\r\n\xe7\xae\x80\xe4\xbd\x93\n";
 writeFile(host,u8"Long received 文本 name.txt",body);writeFile(host,".DS_Store","metadata");
 writeFile(host,"._shadow.txt","metadata");writeFile(host,"HIDDEN.TXT","metadata",FS_ATTRIB_HIDDEN);
 CHECK(host.mkdir(".Spotlight-V100"));host.end();
 CHECK(inbox.mountReadOnly(7)==Status::NotReady);CHECK(inbox.seal(6)==Status::Stale);CHECK(inbox.seal(7)==Status::Ok);
 CHECK(inbox.device().write(0,body.data(),1).status==Status::ReadOnly);CHECK(inbox.mountReadOnly(7)==Status::Ok);
 InboxCandidate selected{};unsigned visible=0,ignored=0;for(unsigned i=0;i<513;++i){InboxCandidate c;auto s=inbox.next(c);if(s==Status::NotFound)break;CHECK(s==Status::Ok);if(isTxtCandidate(c)){selected=c;++visible;}else ++ignored;}
 CHECK(visible==1);CHECK(ignored==4);CHECK(std::strcmp(selected.name.data(),u8"Long received 文本 name.txt")==0);
 auto forged=selected;forged.source.size++;CHECK(inbox.open(forged)!=Status::Ok);CHECK(inbox.open(selected)==Status::Ok);
 char output[64]{};auto read=inbox.read(0,output,body.size());CHECK(read.status==Status::Ok&&std::string(output,read.bytes)==body);
 CHECK(inbox.read(body.size(),output,1).status==Status::Bounds);inbox.closeFile();inbox.release();CHECK(!inbox.stable(7));
}
void full(){Disk disk;SdFatInbox inbox(disk);CHECK(inbox.prepare(1)==Status::Ok);FatVolume host;CHECK(host.begin(&disk,false));
 FatFile f;CHECK(f.open(&host,"FULL.BIN",O_CREAT|O_RDWR));uint8_t block[512]{};unsigned sectors=0;while(sectors<16385&&f.write(block,sizeof block)==sizeof block)++sectors;
 CHECK(sectors>1000&&sectors<16384);CHECK(f.fileSize()<=disk.bytes.size());f.close();host.end();CHECK(inbox.seal(1)==Status::Ok);CHECK(inbox.mountReadOnly(1)==Status::Ok);
 CHECK(inbox.device().write(disk.bytes.size(),block,1).status==Status::ReadOnly);inbox.release();}
void corrupt(){
 {Disk disk;SdFatInbox inbox(disk);CHECK(inbox.prepare(1)==Status::Ok);FatVolume host;CHECK(host.begin(&disk,false));
 writeFile(host,"CYCLE.TXT",std::string(4096,'c'));FatFile f;CHECK(f.open(&host,"CYCLE.TXT",O_RDONLY));auto cluster=f.firstCluster();auto fat=host.fatStartSector();auto second=fat+host.sectorsPerFat();f.close();host.end();
 for(auto table:{fat,second}){const auto at=uint64_t(table)*512+cluster*2;disk.bytes[at]=cluster;disk.bytes[at+1]=cluster>>8;}
 CHECK(inbox.seal(1)==Status::Ok);CHECK(inbox.mountReadOnly(1)==Status::Ok);auto c=find(inbox);auto before=disk.reads;CHECK(inbox.open(c)==Status::Filesystem);CHECK(disk.reads-before<512);inbox.release();}
 {Disk disk;SdFatInbox inbox(disk);CHECK(inbox.prepare(1)==Status::Ok);disk.bytes[510]=0;CHECK(inbox.seal(1)==Status::Ok);auto before=disk.reads;CHECK(inbox.mountReadOnly(1)==Status::Filesystem);CHECK(disk.reads-before<64);inbox.release();}
 {Disk disk;SdFatInbox inbox(disk);CHECK(inbox.prepare(1)==Status::Ok);const auto partition=uint32_t(disk.bytes[454])|(uint32_t(disk.bytes[455])<<8)|(uint32_t(disk.bytes[456])<<16)|(uint32_t(disk.bytes[457])<<24);
 disk.bytes[partition*512+17]=0;disk.bytes[partition*512+18]=0;CHECK(inbox.seal(1)==Status::Ok);CHECK(inbox.mountReadOnly(1)==Status::Filesystem);inbox.release();}
}
void lifecycle(){Disk disk;SdFatInbox inbox(disk);for(uint64_t session=1;session<=20;++session){CHECK(inbox.prepare(session)==Status::Ok);CHECK(inbox.prepare(session)==Status::Busy);CHECK(inbox.seal(session)==Status::Ok);CHECK(inbox.mountReadOnly(session)==Status::Ok);CHECK(!inbox.stable(session+1));inbox.release();CHECK(inbox.device().write(0,"x",1).status==Status::ReadOnly);}disk.failSync=true;CHECK(inbox.prepare(21)==Status::Filesystem);CHECK(!inbox.stable(21));}
int main(int argc,char**argv){CHECK(argc==2);std::string test=argv[1];if(test=="roundtrip")roundtrip();else if(test=="full")full();else if(test=="corrupt")corrupt();else if(test=="lifecycle")lifecycle();else CHECK(false);std::cout<<"PASS real SdFat "<<test<<"\n";}
