#include "fatfs_inbox.hpp"
#include "draftor_usb_transfer/inbox.hpp"
#include "draftor_usb_transfer/import.hpp"
#include "media_bridge.hpp"
#include "persistent_store.hpp"
#include "esp_heap_caps.h"
#include "check.hpp"
#include <filesystem>
using namespace draftor::usb;
static MediaBridge* hostBridge=nullptr;
static DSTATUS ready(BYTE){return hostBridge&&hostBridge->state().accepting?0:STA_NOINIT;}
static DRESULT rd(BYTE,BYTE* out,DWORD at,UINT n){auto x=hostBridge->read(hostBridge->generation(),std::uint64_t(at)*512,out,std::size_t(n)*512);return x.status==Status::Ok?RES_OK:RES_ERROR;}
static DRESULT wr(BYTE,const BYTE* in,DWORD at,UINT n){auto x=hostBridge->write(hostBridge->generation(),std::uint64_t(at)*512,in,std::size_t(n)*512);return x.status==Status::Ok?RES_OK:RES_ERROR;}
static DRESULT ctl(BYTE,BYTE c,void* p){switch(c){case CTRL_SYNC:return hostBridge->sync(hostBridge->generation())==Status::Ok?RES_OK:RES_ERROR;case GET_SECTOR_COUNT:*static_cast<DWORD*>(p)=hostBridge->sectors();return RES_OK;case GET_SECTOR_SIZE:*static_cast<WORD*>(p)=512;return RES_OK;case GET_BLOCK_SIZE:*static_cast<DWORD*>(p)=1;return RES_OK;case CTRL_TRIM:return RES_OK;default:return RES_PARERR;}}
static FATFS hostFat{};
static void hostMount(MediaBridge& bridge){hostBridge=&bridge;BYTE drive=0;CHECK(ff_diskio_get_drive(&drive)==ESP_OK&&drive==1);const ff_diskio_impl_t v{ready,ready,rd,wr,ctl};ff_diskio_register(1,&v);VolToPart[1]={1,0};CHECK(f_mount(&hostFat,"1:",1)==FR_OK);}
static void hostUnmount(){CHECK(f_mount(nullptr,"1:",0)==FR_OK);ff_diskio_unregister(1);hostBridge=nullptr;}
static void file(const std::string& name,std::string_view content){FIL f{};CHECK(f_open(&f,("1:/"+name).c_str(),FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);UINT n=0;CHECK(f_write(&f,content.data(),content.size(),&n)==FR_OK&&n==content.size());CHECK(f_sync(&f)==FR_OK&&f_close(&f)==FR_OK);}
static void sealed(InboxSession& inbox,MediaBridge& bridge){bridge.withdraw(EndReason::Ejected);for(unsigned i=0;i<5;++i)inbox.tick();}
int main(int argc,char** argv){
 CHECK(argc==2);std::filesystem::create_directories(argv[1]);
 for(unsigned cycle=0;cycle<3;++cycle){
  s3::PsramDisk disk;CHECK(disk.allocate()==Status::Ok&&disk.bytes()==512*1024);
  s3::FatFsInbox fs(disk);MediaBridge bridge;InboxSession inbox(fs,fs,bridge);
  CHECK(inbox.begin()==Status::Ok&&inbox.tick()==InboxPhase::HostMounted);hostMount(bridge);
  std::string wire="\xef\xbb\xbf";for(std::size_t i=0;i<BodyLimit;++i)wire+="\r\n";
  std::string longName(190,'L');longName+=u8"中文𨿍.txt";
  file(longName,wire);file(".DS_Store","metadata");file("hidden.txt","hidden");CHECK(f_chmod("1:/hidden.txt",AM_HID,AM_HID)==FR_OK);
  CHECK(f_mkdir("1:/ignored.txt")==FR_OK);file("ignored.txt/child.txt","not root");
  // A cyclic subdirectory must remain ignored: root-only enumeration never
  // follows it. Use real directory allocation, then corrupt its FAT12 link.
  CHECK(f_mkdir("1:/loop.txt")==FR_OK);FF_DIR loop{};CHECK(f_opendir(&loop,"1:/loop.txt")==FR_OK);
  const auto loopCluster=loop.obj.sclust;CHECK(f_closedir(&loop)==FR_OK);
  hostUnmount();
  std::array<BYTE,512> boot{};CHECK(bridge.read(bridge.generation(),0,boot.data(),boot.size()).status==Status::Ok);
  const auto fatSectors=unsigned(boot[22])+(unsigned(boot[23])<<8);
  const auto entryOffset=loopCluster+loopCluster/2;BYTE link[2]{};
  CHECK(bridge.read(bridge.generation(),512+entryOffset,link,2).status==Status::Ok);
  unsigned value=unsigned(link[0])+(unsigned(link[1])<<8);
  value=loopCluster&1?(value&0x000f)|(loopCluster<<4):(value&0xf000)|loopCluster;
  link[0]=value;link[1]=value>>8;
  for(unsigned copy=0;copy<2;++copy)CHECK(bridge.write(bridge.generation(),512+copy*fatSectors*512+entryOffset,link,2).status==Status::Ok);
  sealed(inbox,bridge);CHECK(inbox.phase()==InboxPhase::Inspecting&&inbox.confirmed());
  InboxCandidate candidate,found{};unsigned count=0;
  for(unsigned n=0;n<20;++n){auto status=inbox.next(candidate);if(status==Status::NotFound)break;if(status==Status::Busy)continue;CHECK(status==Status::Ok);++count;found=candidate;}
  CHECK(count==1&&std::string(found.name.data())==longName&&found.source.size==WireLimit);
  ImportSource* source=nullptr;CHECK(inbox.acquire(found,source)==Status::Ok);
  host::PersistentStore store(std::filesystem::path(argv[1])/std::to_string(cycle));ImportJob job(store);
  ImportIntent intent;intent.operation=cycle+1;intent.source=source->identity();intent.name=found.name;
  CHECK(job.begin(*source,intent)==Status::Ok);for(unsigned n=0;n<2000&&job.phase()!=ImportPhase::Complete;++n)job.tick();
  CHECK(job.phase()==ImportPhase::Complete&&store.body(cycle+1)==std::string(BodyLimit,'\n'));
  // SDK diskio refuses writes after sealing, even if a caller bypasses MediaBridge.
  BYTE byte[512]{};CHECK(disk_write(0,byte,0,1)==RES_WRPRT);
  CHECK(inbox.release(inbox.sourceLease())==Status::Ok&&inbox.close()==Status::Ok);disk.release();CHECK(host_heap_live==0);
 }
 {s3::PsramDisk disk;CHECK(disk.allocate()==Status::Ok);s3::FatFsInbox fs(disk);MediaBridge bridge;InboxSession inbox(fs,fs,bridge);CHECK(inbox.begin()==Status::Ok);inbox.tick();hostMount(bridge);
  FIL f{};CHECK(f_open(&f,"1:/FULL.BIN",FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);std::array<char,4096> chunk{};std::size_t total=0;bool full=false;
  for(unsigned n=0;n<256;++n){UINT got=0;CHECK(f_write(&f,chunk.data(),chunk.size(),&got)==FR_OK);total+=got;if(got<chunk.size()){full=true;break;}}
  CHECK(full&&total<512*1024);CHECK(f_close(&f)==FR_OK);hostUnmount();sealed(inbox,bridge);CHECK(inbox.phase()==InboxPhase::Inspecting);CHECK(inbox.close()==Status::Ok);disk.release();}
 {s3::PsramDisk disk;CHECK(disk.allocate()==Status::Ok);s3::FatFsInbox fs(disk);MediaBridge bridge;InboxSession inbox(fs,fs,bridge);CHECK(inbox.begin()==Status::Ok);inbox.tick();std::array<char,512> bad{};CHECK(bridge.write(bridge.generation(),0,bad.data(),bad.size()).status==Status::Ok);sealed(inbox,bridge);CHECK(inbox.phase()==InboxPhase::Closed&&inbox.result().status==Status::Filesystem);disk.release();}
 {s3::PsramDisk disk;CHECK(disk.allocate()==Status::Ok);s3::FatFsInbox fs(disk);MediaBridge bridge;InboxSession inbox(fs,fs,bridge);CHECK(inbox.begin()==Status::Ok);inbox.tick();
  BYTE alteredRoot[2]={0,1};CHECK(bridge.write(bridge.generation(),17,alteredRoot,2).status==Status::Ok);sealed(inbox,bridge);
  CHECK(inbox.phase()==InboxPhase::Closed&&inbox.result().status==Status::Filesystem);disk.release();}
 // Actual FatFs prepare failures must unregister the private drive and retry.
 struct FaultDisk final:BlockDevice {
  s3::PsramDisk memory;bool failWrite=false,failSync=false;
  std::uint32_t sectorCount()const noexcept override{return memory.sectorCount();}
  IoResult read(std::uint64_t a,void* p,std::size_t n)noexcept override{return memory.read(a,p,n);}
  IoResult write(std::uint64_t a,const void* p,std::size_t n)noexcept override{return failWrite?IoResult{Status::Io,0}:memory.write(a,p,n);}
  Status sync()noexcept override{return failSync?Status::Io:memory.sync();}
 };
 for(bool failSync:{false,true}){
  FaultDisk disk;CHECK(disk.memory.allocate()==Status::Ok);disk.failWrite=!failSync;disk.failSync=failSync;
  s3::FatFsInbox fs(disk);MediaBridge bridge;InboxSession inbox(fs,fs,bridge);
  CHECK(inbox.begin()==Status::Ok&&inbox.tick()==InboxPhase::Closed);CHECK(inbox.result().status==Status::Filesystem&&!bridge.state().accepting);
  CHECK(inbox.close()==Status::Ok);disk.memory.release();CHECK(host_heap_live==0);
  CHECK(disk.memory.allocate()==Status::Ok);disk.failWrite=disk.failSync=false;
  CHECK(inbox.begin()==Status::Ok&&inbox.tick()==InboxPhase::HostMounted);
  inbox.endHost(EndReason::Forced);for(unsigned n=0;n<5;++n)inbox.tick();CHECK(inbox.close()==Status::Ok);disk.memory.release();CHECK(host_heap_live==0);
 }
 CHECK(host_heap_live==0);std::printf("PASS real S3 FatFsInbox 512KiB format/FAT/MSC ownership checks=%zu allocationPeak=%zu live=%zu\n",checks,host_heap_peak,host_heap_live);
}
