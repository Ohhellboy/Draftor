#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
#define CONTROL_STAGE_SETUP 1
#define TUSB_REQ_TYPE_CLASS 1
#define MSC_REQ_RESET 0xff
struct tusb_control_request_t { struct { uint8_t type=0; } bmRequestType_bit;uint8_t bRequest=0;uint16_t wValue=0,wLength=0; };
#define TUD_MSC_DESC_LEN 23
#define TUD_MSC_DESCRIPTOR(i,s,o,n,m) 9,4,i,0,2,8,6,80,s,7,5,o,2,m,0,0,7,5,n,2,m,0,0
#define TUSB_DIR_IN 1
#define TUSB_DIR_OUT 0
#define SCSI_SENSE_MEDIUM_ERROR 3
#define SCSI_SENSE_DATA_PROTECT 7
#define SCSI_SENSE_NOT_READY 2
#define SCSI_SENSE_ILLEGAL_REQUEST 5
#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1e
#define TUD_MSC_RET_ERROR -1
#define TUD_MSC_RET_BUSY 0
#define TUD_MSC_RET_ASYNC -2
class Adafruit_USBD_Interface {protected: uint8_t _strid=1;public:virtual uint16_t getInterfaceDescriptor(uint8_t,uint8_t*,uint16_t)=0;void setStringDescriptor(const char*){}};
namespace shim {inline uint32_t now=0,attaches=0,detaches=0,completions=0;inline bool rejectCompletion=false;inline void (*beforeTask)()=nullptr;inline int32_t lastCompletion=0;inline uint8_t sense=0;}
class FakeUsb {public:bool isInitialized(){return true;}void begin(unsigned){}bool addInterface(Adafruit_USBD_Interface&){return true;}bool detach(){++shim::detaches;return true;}bool attach(){++shim::attaches;return true;}uint8_t allocInterface(unsigned){return 0;}uint8_t allocEndpoint(uint8_t direction){return direction?0x81:1;}};
inline FakeUsb TinyUSBDevice;
inline uint32_t millis(){return shim::now;}
inline bool tud_msc_set_sense(uint8_t,uint8_t key,uint8_t,uint8_t){shim::sense=key;return true;}
inline bool tud_msc_async_io_done(int32_t n,bool){if(shim::rejectCompletion)return false;++shim::completions;shim::lastCompletion=n;return true;}
inline void tud_task_ext(uint32_t,bool){if(shim::beforeTask){auto f=shim::beforeTask;shim::beforeTask=nullptr;f();}}
