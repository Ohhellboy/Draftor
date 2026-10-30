#include "board_sdmmc.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
namespace model {
std::uint32_t time=0,latency=25;
unsigned allocations=0,fail_allocation=0,host_calls=0,unregister_calls=0,frequency_calls=0;
bool host=false,disk=false,fat=false;
FATFS* vfs=nullptr;sdmmc_card_t* card=nullptr;
std::map<void*,std::size_t> live;
std::vector<std::string> events;
std::string fail_stage;
void event(const char* s){events.emplace_back(s);}
bool failing(const char* stage){return fail_stage==stage;}
void empty(){CHECK(!host&&!disk&&!fat&&!vfs&&live.empty());}
}
extern "C" void* draftor_sdmmc_test_allocate(std::size_t size){
 using namespace model;++allocations;
 if(allocations==fail_allocation){event("alloc_fail");if(allocations==2)CHECK(host&&disk&&fat&&vfs);return nullptr;}
 auto* p=std::calloc(1,size);CHECK(p);CHECK(live.emplace(p,size).second);event("allocate");return p;
}
extern "C" void draftor_sdmmc_test_free(void* p){
 if(!p)return;
 using namespace model;CHECK(live.count(p));
 if(p==card){CHECK(!host&&!disk&&!fat&&!vfs);card=nullptr;event("free_card");}
 else event("free_owner");
 live.erase(p);std::free(p);
}
std::uint32_t millis(){return model::time;}
int ff_diskio_get_drive(BYTE* out){model::event("drive");if(model::failing("drive"))return ESP_ERR_NOT_FOUND;*out=0;return ESP_OK;}
int sdmmc_host_init(){
 using namespace model;CHECK(!host);++host_calls;event("host_init");model::time+=latency;
 if(failing("host_init"))return ESP_FAIL;host=true;return ESP_OK;
}
int sdmmc_host_deinit(){throw std::runtime_error("DEINIT_ARG lost");}
int sdmmc_host_deinit_slot(int slot){
 using namespace model;CHECK(slot==1&&host&&!disk&&!fat&&!vfs);event("host_deinit");
 if(failing("host_deinit"))return ESP_FAIL;host=false;return ESP_OK;
}
int sdmmc_host_init_slot(int slot,const sdmmc_slot_config_t* c){
 CHECK(model::host&&slot==1&&c->width==1&&c->clk==38&&c->cmd==21&&c->d0==39);
 model::event("slot_init");return model::failing("slot_init")?ESP_FAIL:ESP_OK;
}
int sdmmc_card_init(const sdmmc_host_t* h,sdmmc_card_t* c){
 using namespace model;CHECK(host&&live.count(c));card=c;
 CHECK(h->flags==(SDMMC_HOST_FLAG_1BIT|SDMMC_HOST_FLAG_DEINIT_ARG));
 CHECK(h->max_freq_khz==20000&&h->command_timeout_ms==1000&&h->slot==1);
 c->host=*h;c->real_freq_khz=20000;event("card_init");return failing("card_init")?ESP_ERR_TIMEOUT:ESP_OK;
}
void ff_diskio_register_sdmmc(BYTE d,sdmmc_card_t* c){CHECK(d==0&&model::host&&!model::disk&&c==model::card);model::disk=true;model::event("disk_register");}
void ff_sdmmc_set_disk_status_check(BYTE d,bool check){CHECK(d==0&&!check&&model::disk);}
void ff_diskio_unregister(BYTE d){CHECK(d==0&&model::disk&&!model::fat);model::disk=false;model::event("disk_unregister");}
int esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t* c,FATFS** out){
 using namespace model;CHECK(!std::strcmp(c->base_path,"/sd")&&!std::strcmp(c->fat_drive,"0:")&&c->max_files==9);
 CHECK(host&&disk&&!vfs);event("vfs_register");
 if(failing("vfs_alloc"))return ESP_ERR_NO_MEM;
 if(failing("vfs_conflict"))return ESP_ERR_INVALID_STATE; // Must not unregister/adopt another owner's registration.
 vfs=static_cast<FATFS*>(std::calloc(1,sizeof(FATFS)));CHECK(vfs);vfs->live=0xface;*out=vfs;return ESP_OK;
}
int f_mount(FATFS* fs,const char* drive,unsigned char immediate){
 using namespace model;CHECK(!std::strcmp(drive,"0:"));
 if(fs){CHECK(fs==vfs&&fs->live==0xface&&disk&&immediate==1);fat=true;event("fat_attach");return failing("fat_mount")?FR_DISK_ERR:FR_OK;}
 CHECK(!immediate&&fat&&vfs&&disk);event("fat_detach");
 if(failing("fat_detach"))return FR_DISK_ERR;fat=false;return FR_OK;
}
int esp_vfs_fat_unregister_path(const char* path){
 using namespace model;CHECK(!std::strcmp(path,"/sd")&&vfs&&!fat);++unregister_calls;event("vfs_unregister");
 // Exact pinned behavior: the object is consumed even if unregister reports failure.
 CHECK(vfs->live==0xface);std::free(vfs);vfs=nullptr;
 return failing("vfs_unregister")?ESP_ERR_INVALID_STATE:ESP_OK;
}
int sdmmc_host_get_real_freq(int slot,int* hz){
 using namespace model;++frequency_calls;CHECK(slot==1&&host&&disk&&fat&&vfs&&card&&live.count(card));
 CHECK(card->host.slot==slot);*hz=20000;return ESP_OK;
}
struct Port {
 draftor::board::SdmmcVolume volume;bool owned=false;
 std::uint32_t now()const{return model::time;}bool unowned()const{return !owned;}
 bool park(){return volume.end();}void pause(std::uint32_t n){model::time+=n;}
 s3tf::MountAttempt mount(std::uint32_t budget){return volume.begin(9,budget);}
 void record(unsigned,const s3tf::MountAttempt&){}
};
void retired(draftor::board::SdmmcVolume& v){
 auto n=model::frequency_calls;CHECK(v.cardKHz()==-1&&v.actualKHz()==-1);CHECK(model::frequency_calls==n);
}
void terminal(Port& p){
 retired(p.volume);CHECK(p.volume.cleanupError()!=ESP_OK);
 const auto events=model::events.size();const auto calls=model::host_calls;
 CHECK(!p.volume.end());CHECK(!p.volume.end());
 auto again=p.volume.begin(9,4000);CHECK(!again.mounted&&!std::strcmp(again.reason,"sdmmc_cleanup_blocked"));
 auto result=s3tf::recoverMount(p);CHECK(!result.mounted&&result.cleanup_failed&&!result.attempts);
 draftor::board::SdmmcVolume another;CHECK(!another.begin(9,4000).mounted);
 CHECK(model::events.size()==events&&model::host_calls==calls);
}
int main(int argc,char** argv){try{
 CHECK(argc==2);const std::string which=argv[1];Port p;
 static_assert(draftor::board::Waveshare&&draftor::board::Monochrome);
 CHECK(!std::strcmp(draftor::board::DocumentRoot,"/sd/WAVESHARE_RLCD42"));
 if(which=="normal"){
  auto r=s3tf::recoverMount(p);CHECK(r.mounted&&r.attempts==1&&!r.cleanup_failed);
  CHECK(p.volume.cardKHz()==20000&&p.volume.actualKHz()==20000);
  CHECK(!p.volume.begin(9,4000).mounted);
  draftor::board::SdmmcVolume another;CHECK(!another.begin(9,4000).mounted);CHECK(model::host_calls==1);
  CHECK(p.volume.end()&&p.volume.end());retired(p.volume);model::empty();
 }else if(which=="context_alloc"||which=="context_alloc_retry"){
  model::fail_allocation=2;
  auto a=p.volume.begin(9,4000);CHECK(!a.mounted&&a.fat_result==ESP_ERR_NO_MEM);
  CHECK(!std::strcmp(a.reason,"sdmmc_context_allocation_failed"));CHECK(p.volume.cleanupError()==ESP_OK);
  retired(p.volume);model::empty();
  auto& e=model::events;
  CHECK(e[e.size()-5]=="fat_detach"&&e[e.size()-4]=="vfs_unregister"&&e[e.size()-3]=="disk_unregister"&&e[e.size()-2]=="host_deinit"&&e.back()=="free_card");
  model::fail_allocation=0;
  if(which=="context_alloc_retry"){auto r=s3tf::recoverMount(p);CHECK(r.mounted&&!r.cleanup_failed);CHECK(p.volume.end());model::empty();}
 }else if(which=="unregister_error"||which=="fat_detach_error"||which=="host_deinit_error"){
  CHECK(p.volume.begin(9,4000).mounted);
  model::fail_stage=which=="unregister_error"?"vfs_unregister":which=="fat_detach_error"?"fat_detach":"host_deinit";
  CHECK(!p.volume.end());terminal(p);
  if(which=="unregister_error")model::empty();
  else{CHECK(model::live.size()==1&&model::card);CHECK(model::host);CHECK(model::fat==(which=="fat_detach_error"));}
 }else if(which=="context_alloc_cleanup_error"){
  model::fail_allocation=2;model::fail_stage="vfs_unregister";
  auto r=s3tf::recoverMount(p);CHECK(!r.mounted&&r.cleanup_failed&&r.attempts==1);
  CHECK(model::host_calls==1);terminal(p);model::empty();
 }else if(which=="admission_overrun"||which=="admission_cleanup_error"){
  model::latency=5000;if(which=="admission_cleanup_error")model::fail_stage="vfs_unregister";
  auto r=s3tf::recoverMount(p);CHECK(!r.mounted);
  if(which=="admission_cleanup_error"){CHECK(r.cleanup_failed&&r.attempts==1);terminal(p);}
  else CHECK(!r.cleanup_failed&&r.attempts==3&&r.elapsed_ms>=15000);
  retired(p.volume);model::empty();
 }else if(which=="retry_timeout"){
  model::fail_stage="card_init";auto r=s3tf::recoverMount(p);
  CHECK(!r.mounted&&!r.cleanup_failed&&r.attempts==3&&model::host_calls==3);retired(p.volume);model::empty();
 }else if(which=="owned_rejection"){
  p.owned=true;auto r=s3tf::recoverMount(p);CHECK(!r.mounted&&!r.attempts&&model::events.empty());
 }else{
  if(which=="card_alloc")model::fail_allocation=1;else model::fail_stage=which;
  auto a=p.volume.begin(9,4000);CHECK(!a.mounted);CHECK(p.volume.cleanupError()==ESP_OK);
  if(which=="card_alloc"||which=="vfs_alloc")CHECK(a.fat_result==ESP_ERR_NO_MEM);
  if(which=="vfs_conflict")CHECK(!model::unregister_calls);
  retired(p.volume);model::empty();
 }
 std::cout<<"PASS "<<which<<": real production SDMMC owner, real heap lifecycle; hardware/FAT effects mocked; no device access\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

