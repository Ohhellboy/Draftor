#pragma once
#include <cstdint>
#include <cstddef>
using esp_err_t=int;using gpio_num_t=int;using BYTE=unsigned char;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=0x101,ESP_ERR_INVALID_ARG=0x102,ESP_ERR_INVALID_STATE=0x103,ESP_ERR_NOT_FOUND=0x105,ESP_ERR_TIMEOUT=0x107;
constexpr unsigned SDMMC_HOST_FLAG_1BIT=1,SDMMC_HOST_FLAG_DEINIT_ARG=32,SDMMC_FREQ_PROBING=400;
int sdmmc_host_init();int sdmmc_host_deinit();int sdmmc_host_deinit_slot(int);
struct sdmmc_host_t {unsigned flags=0;int max_freq_khz=0,command_timeout_ms=0,slot=1;int(*init)()=sdmmc_host_init;int(*deinit)()=sdmmc_host_deinit;int(*deinit_p)(int)=sdmmc_host_deinit_slot;};
struct sdmmc_slot_config_t {int width=0,clk=-1,cmd=-1,d0=-1;};
struct sdmmc_card_t {sdmmc_host_t host;int real_freq_khz=0;};
struct FATFS {unsigned live=0;};
struct esp_vfs_fat_conf_t {const char* base_path;const char* fat_drive;std::size_t max_files;};
inline sdmmc_host_t model_host(){sdmmc_host_t h;h.flags=SDMMC_HOST_FLAG_1BIT|SDMMC_HOST_FLAG_DEINIT_ARG;return h;}
#define SDMMC_HOST_DEFAULT() model_host()
#define SDMMC_SLOT_CONFIG_DEFAULT() sdmmc_slot_config_t{}
int sdmmc_host_init_slot(int,const sdmmc_slot_config_t*);
int sdmmc_card_init(const sdmmc_host_t*,sdmmc_card_t*);
int sdmmc_host_get_real_freq(int,int*);
int esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t*,FATFS**);
int esp_vfs_fat_unregister_path(const char*);
constexpr int FR_OK=0,FR_DISK_ERR=1;
int f_mount(FATFS*,const char*,unsigned char);
