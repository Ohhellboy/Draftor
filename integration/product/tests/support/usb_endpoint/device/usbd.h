#pragma once
#include <cstdint>
#define SCSI_SENSE_MEDIUM_ERROR 3
#define SCSI_SENSE_DATA_PROTECT 7
#define SCSI_SENSE_NOT_READY 2
#define SCSI_SENSE_ILLEGAL_REQUEST 5
#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1e
inline bool tud_disconnect(){return true;}
inline bool tud_connect(){return true;}
inline void tud_msc_set_sense(int,std::uint8_t,std::uint8_t,int){}
