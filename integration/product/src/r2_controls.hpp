#pragma once
#include "device_ime.hpp"
#include "idle_log.hpp"
#include <cstring>
#include "r3_status.hpp"
namespace r2 {
// TEST_ONLY, reachable even with the inherited quiet capture profile. The
// platform supplies at most 32 received bytes/turn; replies use its existing
// nonblocking UART queue. No body text, allocation or synchronous draining.
class LearningControls {
 char command_[16]{};unsigned size_=0;bool overflow_=false;
 public:
 bool feed(char ch,ime_device::DeviceIme& ime,ime_device::LearningTrial& l,ime_device::IdleLog& log,bool (*productCommand)(const char*,ime_device::IdleLog&)=nullptr){
  if(ch=='\r')return false;
  if(ch!='\n'){if(size_+1<sizeof(command_))command_[size_++]=ch;else overflow_=true;return false;}
  command_[size_]=0;
  if(!overflow_&&productCommand&&productCommand(command_,log)){size_=0;overflow_=false;return true;}
  if(!overflow_&&r3::status(command_,ime,l,log)){size_=0;overflow_=false;return true;}
  const bool known=!overflow_&&std::strncmp(command_,"LEARN",5)==0;
  const bool accepted=known&&(std::strcmp(command_,"LEARNSTATUS")==0||ime.learningCommand(command_));
  auto* p=l.personal();
  log.printf("[R2_LEARN] command=%s accepted=%u phase=%u status=%u record=%u apply=%u work=%u index=%s saved=%llu\n",overflow_?"OVERFLOW":command_,accepted,unsigned(l.phase()),unsigned(l.status()),l.recording(),l.applying(),l.hasWork(),p?(p->healthy()?"healthy":p->error()):"unallocated",(unsigned long long)l.stats.saved_members);
  size_=0;overflow_=false;return accepted;
 }
};
}
