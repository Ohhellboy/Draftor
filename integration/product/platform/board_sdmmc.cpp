// SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
// SDMMC/FAT lifecycle adapted from ESP-IDF v5.5.5 b774170ff46,
// components/fatfs/vfs/vfs_fat_sdmmc.c (Espressif, Apache-2.0).
// See vendor/sdmmc-lifecycle/NOTICE.md for provenance and changes.
#include "board_sdmmc.hpp"
#if DRAFTOR_BOARD == WAVESHARE_RLCD42
#include <cstdlib>
#include <new>
#include <diskio_impl.h>
#include <diskio_sdmmc.h>

#if defined(DRAFTOR_SDMMC_HOST_TEST) && defined(ARDUINO)
#error "SDMMC fault allocator must never be compiled into firmware"
#endif
namespace {
#ifdef DRAFTOR_SDMMC_HOST_TEST
extern "C" void* draftor_sdmmc_test_allocate(std::size_t);
extern "C" void draftor_sdmmc_test_free(void*);
void* allocate(std::size_t size){return draftor_sdmmc_test_allocate(size);}
void dispose(void* p){draftor_sdmmc_test_free(p);}
#else
void* allocate(std::size_t size){return std::calloc(1,size);}
void dispose(void* p){std::free(p);}
#endif
}

namespace draftor::board {
struct SdmmcVolume::State {
 sdmmc_card_t* card=nullptr;
 BYTE drive=FF_DRV_NOT_USED;
 bool host_started=false,disk_owned=false,vfs_owned=false,fat_attached=false;
};
SdmmcVolume* SdmmcVolume::active_=nullptr;
SdmmcVolume::State SdmmcVolume::quarantined_{};
esp_err_t SdmmcVolume::cleanup_error_=ESP_OK;

esp_err_t SdmmcVolume::release(State& s,const sdmmc_host_t& host){
 // Remove all references before releasing their backing objects. If FatFs
 // refuses detachment, preserve the whole remaining state until reset.
 if(s.fat_attached){
  const char drive[]={char('0'+s.drive),':','\0'};
  if(f_mount(nullptr,drive,0)!=FR_OK)return ESP_FAIL;
  s.fat_attached=false;
 }
 esp_err_t result=ESP_OK;
 if(s.vfs_owned){
  // This pinned SDK consumes its FAT context even when VFS unregister fails.
  result=esp_vfs_fat_unregister_path(MountPoint);
  s.vfs_owned=false;
 }
 if(s.disk_owned){ff_diskio_unregister(s.drive);s.disk_owned=false;}
 if(s.host_started){
  const auto error=(host.flags&SDMMC_HOST_FLAG_DEINIT_ARG)
      ?host.deinit_p(host.slot):host.deinit();
  if(error!=ESP_OK)return result==ESP_OK?error:result;
  s.host_started=false;
 }
 dispose(s.card);s.card=nullptr;
 return result;
}

void SdmmcVolume::quarantine(const State& s,esp_err_t error){
 // Never retry uncertain cleanup or expose these objects as a mounted card.
 // Any still-live backing allocation is retained once, until device reset.
 quarantined_=s;cleanup_error_=error;
}

bool SdmmcVolume::end(){
 if(cleanup_error_!=ESP_OK)return false;
 if(!owner_)return true;
 auto* old=owner_;owner_=nullptr; // Retire the public handle before cleanup.
 const sdmmc_host_t host=SDMMC_HOST_DEFAULT();
 const auto error=release(*old,host);
 if(error!=ESP_OK)quarantine(*old,error);
 dispose(old);active_=nullptr;
 return error==ESP_OK;
}

s3tf::MountAttempt SdmmcVolume::begin(unsigned maxFiles,std::uint32_t admissionMs){
 if(cleanup_error_!=ESP_OK)return {0,false,false,"sdmmc_cleanup_blocked",cleanup_error_};
 if(active_||owner_)return {0,false,false,"already_owned",ESP_ERR_INVALID_STATE};
 const auto start=millis();
 sdmmc_host_t host=SDMMC_HOST_DEFAULT();
 host.flags=SDMMC_HOST_FLAG_1BIT|SDMMC_HOST_FLAG_DEINIT_ARG;
 host.max_freq_khz=SdRuntimeKHz;host.command_timeout_ms=SdCommandTimeoutMs;
 sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
 slot.width=1;slot.clk=gpio_num_t(SdClk);slot.cmd=gpio_num_t(SdCmd);slot.d0=gpio_num_t(SdD0);
 State pending{};
 active_=this;
 auto fail=[&](esp_err_t error,const char* reason){
  const auto cleanup=release(pending,host);
  if(cleanup!=ESP_OK)quarantine(pending,cleanup);
  active_=nullptr;
  return s3tf::MountAttempt{std::uint32_t(millis()-start),false,error==ESP_ERR_TIMEOUT,reason,error};
 };
 auto error=ff_diskio_get_drive(&pending.drive);
 if(error!=ESP_OK||pending.drive==FF_DRV_NOT_USED)return fail(error==ESP_OK?ESP_ERR_NO_MEM:error,"sdmmc_drive_error");
 auto* card_memory=allocate(sizeof(sdmmc_card_t));
 if(!card_memory)return fail(ESP_ERR_NO_MEM,"sdmmc_card_allocation_failed");
 pending.card=new(card_memory) sdmmc_card_t{};
 error=host.init();
 if(error!=ESP_OK)return fail(error,"sdmmc_host_init_error");
 pending.host_started=true;
 error=sdmmc_host_init_slot(host.slot,&slot);
 if(error!=ESP_OK)return fail(error,"sdmmc_slot_init_error");
 error=sdmmc_card_init(&host,pending.card);
 if(error!=ESP_OK)return fail(error,"sdmmc_card_init_error");
 ff_diskio_register_sdmmc(pending.drive,pending.card);pending.disk_owned=true;
 ff_sdmmc_set_disk_status_check(pending.drive,false);
 const char drive[]={char('0'+pending.drive),':','\0'};
 esp_vfs_fat_conf_t config{};
 config.base_path=MountPoint;config.fat_drive=drive;config.max_files=maxFiles;
 FATFS* fs=nullptr;
 error=esp_vfs_fat_register_cfg(&config,&fs);
 // An existing registration belongs to someone else. Never adopt/unregister it.
 if(error!=ESP_OK)return fail(error,"sdmmc_vfs_register_error");
 pending.vfs_owned=true;
 // f_mount registers fs before reading the card, including on mount failure.
 pending.fat_attached=true;
 if(f_mount(fs,drive,1)!=FR_OK)return fail(ESP_FAIL,"sdmmc_fat_mount_error");
 // Publish only after the last fallible allocation. A failure here retires
 // FAT/VFS/diskio/host/card in order and preserves ESP_ERR_NO_MEM.
 auto* context_memory=allocate(sizeof(State));
 if(!context_memory)return fail(ESP_ERR_NO_MEM,"sdmmc_context_allocation_failed");
 owner_=new(context_memory) State(pending);
 const auto elapsed=std::uint32_t(millis()-start);
 // Admission is checked after the synchronous call; it is not preemption.
 if(elapsed>=admissionMs){
  end();
  return {std::uint32_t(millis()-start),false,true,"sdmmc_admission_overrun",ESP_ERR_TIMEOUT};
 }
 return {elapsed,true,false,"sdmmc_mounted",ESP_OK};
}
int SdmmcVolume::actualKHz()const{
 int khz=0;
 return owner_&&cleanup_error_==ESP_OK&&sdmmc_host_get_real_freq(owner_->card->host.slot,&khz)==ESP_OK?khz:-1;
}
int SdmmcVolume::cardKHz()const{return owner_&&cleanup_error_==ESP_OK?owner_->card->real_freq_khz:-1;}
}
#endif

