#pragma once
#include "board_config.hpp"
#if DRAFTOR_BOARD == WAVESHARE_RLCD42
#include <Arduino.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include "tf_mount_recovery.hpp"
namespace draftor::board {
// Boot/loop owned. Only this target's staged mount implementation owns /sd.
// No Arduino SD/SD_MMC or IDF convenience-mount context shares its ownership.
class SdmmcVolume {
 struct State;
 State* owner_=nullptr;
 static SdmmcVolume* active_;
 static State quarantined_;
 static esp_err_t cleanup_error_;
 static esp_err_t release(State&,const sdmmc_host_t&);
 static void quarantine(const State&,esp_err_t);
 public:
 SdmmcVolume()=default;
 SdmmcVolume(const SdmmcVolume&)=delete;
 SdmmcVolume& operator=(const SdmmcVolume&)=delete;
 static constexpr unsigned InitKHz=SDMMC_FREQ_PROBING;
 bool end();
 s3tf::MountAttempt begin(unsigned maxFiles,std::uint32_t admissionMs);
 int actualKHz()const;
 int cardKHz()const;
 esp_err_t cleanupError()const{return cleanup_error_;}
};
}
#endif
