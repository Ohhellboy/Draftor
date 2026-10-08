#include <Arduino.h>
#include <DraftorUsb.h>
#include "draftor_exchange/transfer_selection.hpp"

// Compile/link evidence only. The target keeps neutral TXT and channel choice
// with no USB module, FAT, MSC callback, or USB interface admission.
namespace dx=draftor::exchange;
namespace {
dx::TransferSelection selection;
volatile unsigned neutralStatus=0;
__attribute__((noinline)) dx::Status selectNeutral(std::uint64_t& token) {
  return selection.acquire(dx::TransferChannel::Wifi,token);
}
}
void setup() {
  std::uint64_t token=0;
  const auto status=selectNeutral(token);
  dx::ImportWire text(65536);char output[16]{};
  const auto fed=text.feed("Draftor\r\n",{output,sizeof output});
  const auto ended=text.finish({output+fed.produced,sizeof output-fed.produced});
  neutralStatus=static_cast<unsigned>(status)+static_cast<unsigned>(fed.produced)+static_cast<unsigned>(ended.flow);
  selection.stage(token,dx::TransferStage::Closing);
  selection.release(token,true);
}
void loop(){delay(1);}
