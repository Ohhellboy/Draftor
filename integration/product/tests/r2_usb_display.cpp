#define main inherited_usb_ui_main
#include "usb_product_flow.cpp"
#undef main
#include "usb_display.hpp"
#include "support/r2_raster.hpp"
static unsigned receiptWrites=0;
static void failFinalReceipt(const char* path,const char* mode){
 // The first receipt may probe an absent imports directory before mkdir/retry.
 // Count writable opens, not that failed ENOENT probe: Reserved, Verified,
 // Publishing, then Imported. Only the final receipt write is faulted.
 if(std::strstr(path,"/imports/")&&mode[0]=='w'&&fs::is_directory(fs::path(path).parent_path())&&
    ++receiptWrites==4)s3fs::faults.write_bytes=0;
}
void menuPreview(Rig& r,const char* name){
 r2preview::Raster canvas;const auto phase=r.transfer.phase();const auto receipt=r.transfer.imported();
 const auto serial=r.transfer.serial(),allocs=r.platform.allocs;const auto id=r.app.session.currentId();const auto revision=r.app.document.revision();
 const auto selected=r.transfer.selectedCandidate(),count=r.transfer.candidateCount();
 s3app::paintUsbPanel(r.transfer,canvas.contentFont,canvas.rect(),canvas.aux(),canvas.content());
 CHECK(r.transfer.phase()==phase&&r.transfer.serial()==serial&&r.transfer.imported().operation==receipt.operation);
 CHECK(r.transfer.selectedCandidate()==selected&&r.transfer.candidateCount()==count&&r.platform.allocs==allocs);
 if(phase==s3app::UsbUiPhase::Result&&!receipt.operation)CHECK(!canvas.has("操作号"));
 CHECK(r.app.session.currentId()==id&&r.app.document.revision()==revision);canvas.save(name);
}
int main(){try{
 {Rig r;r.app.key({1,++r.now,59,0,true});CHECK(r.app.filesOpen());r.key(66);r.close();CHECK(r.app.filesOpen());}
 {Rig r;r.key(66);menuPreview(r,"r2-usb-menu");r.key(8);menuPreview(r,"r2-usb-prepare-export");
  r.until(s3app::UsbUiPhase::Exporting);menuPreview(r,"r2-usb-export-preparing");
  for(unsigned i=0;i<10000&&!r.transfer.exportPublished();++i)r.transfer.tick(++r.now,{});
  CHECK(r.transfer.exportPublished());menuPreview(r,"r2-usb-exporting");r.platform.withdraw(u::EndReason::Ejected);
  r.until(s3app::UsbUiPhase::Result);menuPreview(r,"r2-usb-exported");r.key(41);menuPreview(r,"r2-usb-closing");r.until(s3app::UsbUiPhase::Idle);}
 {Rig r;r.platform.filename="我的博士论文城市公共空间研究第三章最终修订版.txt";
  CHECK(r.transfer.request(true,{})==u::Status::Ok);menuPreview(r,"r2-usb-prepare-inbox");
  r.until(s3app::UsbUiPhase::Receiving);menuPreview(r,"r2-usb-receiving");r.transfer.tick(++r.now,{});
  r.platform.withdraw(u::EndReason::Ejected);r.until(s3app::UsbUiPhase::Selecting);menuPreview(r,"r2-usb-scanning");
  r.transfer.tick(++r.now,{});r.transfer.tick(++r.now,{});CHECK(r.transfer.candidatesReady());menuPreview(r,"r2-usb-selecting");
  CHECK(std::string(r.transfer.candidateName(0))==r.platform.filename);r.key(40);menuPreview(r,"r2-usb-importing");
  r.until(s3app::UsbUiPhase::Result);CHECK(r.transfer.imported().outcome==u::Outcome::Imported);menuPreview(r,"r2-usb-imported");
  r.key(40);r.until(s3app::UsbUiPhase::Opening);menuPreview(r,"r2-usb-opening");r.until(s3app::UsbUiPhase::Idle);}
 {Rig r;r.platform.prepare_result=u::Status::Io;CHECK(r.transfer.request(false,{})==u::Status::Ok);
  r.until(s3app::UsbUiPhase::Result);menuPreview(r,"r2-usb-start-error");}
 {Rig r;receiptWrites=0;s3fs::File::openObserver=failFinalReceipt;
  r.receive();s3fs::faults={};s3fs::File::openObserver=nullptr;
  CHECK(receiptWrites>=4);
  CHECK(r.transfer.imported().status==u::Status::Uncertain&&r.transfer.imported().operation);
  const auto operation=r.transfer.imported().operation;menuPreview(r,"r2-usb-uncertain");
  for(unsigned i=0;i<100&&r.transfer.imported().outcome!=u::Outcome::Imported;++i)r.key(21);
  CHECK(r.transfer.imported().operation==operation&&r.transfer.imported().outcome==u::Outcome::Imported);
  menuPreview(r,"r2-usb-recovered");}
 std::cout<<"PASS R2 USB actual phase adapters; render preserves selection, receipt and transaction state\n";return 0;
}catch(const std::exception& e){s3fs::faults={};s3fs::File::openObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
