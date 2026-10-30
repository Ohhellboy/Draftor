#define main inherited_usb_main
#include "../../integration/product/tests/usb_product_flow.cpp"
#undef main
#include "usb_runtime_allocation.hpp"
#include "runtime_findings_diag.hpp"
#include "stability_diagnostics.hpp"
#include <vector>
namespace rd=s3app::runtime_diag;
namespace capture_test {
static bool integrity=true;static unsigned checks=0,constructed=0;
bool capture(s3app::stability::Kind,uint32_t=0,uint32_t=0,uint32_t=0,bool check=false) noexcept {if(check){++checks;return integrity;}return true;}
}
static Rig* rig=nullptr;
static s3app::App* app=nullptr;
struct ImeSeam {bool composition=false,pending=false;bool active(){return composition;}bool hasWork(){return pending;}};
static ImeSeam* ime=nullptr;
static bool heapIntegrityStopped=false;
struct UsbRuntime {
 Platform platform;s3app::UsbDocumentImport sink;s3app::UsbTransfer transfer;
 UsbRuntime():sink(rig->storage,app->catalog,app->persistence,app->session,app->editorPort),transfer(*app,platform,sink){++capture_test::constructed;}
};
static UsbRuntime* usbRuntime=nullptr;
// Generated from exact device source. Only capture() is replaced by a controlled
// success/failure seam; allocation, App and UsbTransfer are production code.
#include "usb_key_actual.inc"
static void clean(){if(!usbRuntime)return;usbRuntime->transfer.cancel();for(unsigned i=0;i<10000&&usbRuntime->transfer.active();++i)usbRuntime->transfer.tick(++rig->now,{});CHECK(!usbRuntime->transfer.active());usbRuntime->~UsbRuntime();draftor::memory::release(usbRuntime);usbRuntime=nullptr;}
int main(){try{
 Rig r;rig=&r;app=&r.app;ImeSeam i;ime=&i;
 CHECK(app->commitText("合成测试文稿。验证菜单入口和正文保护。Synthetic fixture only.",app->editStamp()));CHECK(app->session.manualSave(++r.now));app->service(++r.now,false);
 CHECK(app->session.startupRestore());const auto text=std::string(app->document.text());
 const s3hid::Event f9{1,++r.now,66,0,true};
 CHECK(!usbKey({1,++r.now,24,3,true}));CHECK(!usbKey({1,++r.now,66,0,false}));
 CHECK(!usbKey({1,++r.now,4,0,true}));CHECK(!capture_test::checks);
 capture_test::integrity=false;CHECK(usbKey(f9));CHECK(heapIntegrityStopped&&!usbRuntime&&!capture_test::constructed);
 CHECK(std::string_view(app->document.text())==text); // The real scheduler latches this stop; no retry in this case.
 // New simulated boot / independent failure injection, not a product reset API.
 heapIntegrityStopped=false;capture_test::integrity=true;
 draftor::memory::allocation_fail_after=0;CHECK(usbKey(f9));CHECK(!usbRuntime&&!heapIntegrityStopped);CHECK(std::string_view(app->notice)=="USB unavailable: memory low");
 draftor::memory::allocation_fail_after=-1;CHECK(usbKey(f9));CHECK(usbRuntime&&usbRuntime->transfer.phase()==s3app::UsbUiPhase::Menu);CHECK(capture_test::constructed==1);
 CHECK(!usbRuntime->platform.allocated&&!usbRuntime->platform.state().accepting); // F9 has no endpoint/export work.
 for(unsigned n=0;n<100;++n)usbRuntime->transfer.tick(++r.now,{});CHECK(usbRuntime->transfer.phase()==s3app::UsbUiPhase::Menu&&!usbRuntime->platform.allocated);
 auto* owner=usbRuntime;const auto checked=capture_test::checks;
 CHECK(usbKey({1,++r.now,41,0,true}));for(unsigned n=0;n<100;++n)usbRuntime->transfer.tick(++r.now,{});
 CHECK(usbKey(f9));CHECK(owner==usbRuntime&&capture_test::checks==checked&&capture_test::constructed==1);
 clean();i.composition=true;CHECK(usbKey(f9));CHECK(!usbRuntime->transfer.active());CHECK(std::string_view(app->document.text())==text);clean();i.composition=false;i.pending=true;CHECK(usbKey(f9));CHECK(!usbRuntime->transfer.active());clean();
 std::cout<<"PASS exact usbKey: integrity halt prevents construction; allocation denial/retry/reuse; saved restart F9 Menu; composition/work admission; production diagnostics off. Hardware heap and USB transport NOT_TESTED.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
