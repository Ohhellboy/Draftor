#define main inherited_usb_flow_main
#include "usb_product_flow.cpp"
#undef main
int main(){try{
 Rig r;CHECK(r.app.commitText("Draftor T01. 中文保存。",r.app.editStamp()));
 const auto id=r.app.session.currentId();
 r.platform.prepare_result=u::Status::Io;
 CHECK(r.transfer.request(false,{})==u::Status::Ok);
 unsigned steps=0;
 while(r.transfer.phase()!=s3app::UsbUiPhase::Result){
  CHECK(++steps<10000);r.transfer.tick(++r.now,{});
  CHECK(r.transfer.phase()!=s3app::UsbUiPhase::Exporting);
  CHECK(!r.platform.state().accepting);
 }
 char message[128];r.transfer.line(0,message,sizeof(message));
 CHECK(std::strcmp(message,"USB startup failed; original retained")==0);
 CHECK(r.app.session.currentId()==id && r.app.document.text()=="Draftor T01. 中文保存。");
 r.close();CHECK(!r.app.commandBarrier());
 std::puts("PASS USB startup rejection never publishes or enters Exporting; original retained and owner released");
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
