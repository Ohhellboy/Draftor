#define main inherited_usb_product_main
#include "usb_product_flow.cpp"
#undef main
#include <fstream>
#include <map>
using CurrentFiles=std::map<std::string,std::string>;
CurrentFiles currentFiles(const Rig& r){
 CurrentFiles out;for(const auto& f:fs::recursive_directory_iterator(r.path))if(f.is_regular_file()&&f.path().filename().string().find("CURRENT.")==0){std::ifstream in(f.path(),std::ios::binary);out[f.path().lexically_relative(r.path).string()]={std::istreambuf_iterator<char>(in),{}};}
 CHECK(!out.empty());return out;
}
int main(){try{
 const char* names[]={"admission_full","allocator_refusal","format_prepare_failure","publish_failure","mount_failure","cancel_before_publish","cancel_waits_for_drain","successful_import"};
 for(unsigned scenario=0;scenario<8;++scenario){
  Rig r;CHECK(r.app.commitText("T04 protected original",r.app.editStamp()));CHECK(r.app.session.manualSave(++r.now));
  const auto original=r.app.session.currentId();const auto before=currentFiles(r);ds::DocumentMetadata originalMeta;CHECK(r.app.catalog.get(original,&originalMeta)==ds::Status::Ok);
  if(scenario==0)r.platform.allocate_result=u::Status::Full;
  if(scenario==1)r.platform.allocate_result=u::Status::Io;
  if(scenario==2)r.platform.backing_result=u::Status::Filesystem;
  if(scenario==3)r.platform.publish_result=u::Status::Io;
  if(scenario==4)r.platform.mount_result=u::Status::Filesystem;
  if(scenario<7){
   CHECK(r.transfer.request(true,{})==u::Status::Ok);
   if(scenario<=3){r.until(s3app::UsbUiPhase::Result);CHECK(!r.platform.state().accepting);}
   else{
    r.until(s3app::UsbUiPhase::Receiving);
    if(scenario==5){CHECK(!r.platform.state().accepting);r.transfer.cancel();}
    else{r.transfer.tick(++r.now,{});CHECK(r.platform.state().accepting);
     if(scenario==4){r.platform.withdraw(u::EndReason::Ejected);r.until(s3app::UsbUiPhase::Result);}
     else{r.platform.hold=true;r.transfer.cancel();for(unsigned n=0;n<4;++n)r.transfer.tick(++r.now,{});CHECK(r.transfer.phase()==s3app::UsbUiPhase::Closing&&r.platform.allocated&&r.platform.inbox_free_calls==0);r.platform.hold=false;}
    }
   }
   CHECK(r.transfer.imported().outcome!=u::Outcome::Imported);r.close();
   CHECK(r.platform.inbox_free_calls==(scenario<2?0u:1u));CHECK(!r.platform.unsafe_free&&!r.app.commandBarrier());
   CHECK(r.app.session.currentId()==original&&r.app.document.text()=="T04 protected original"&&currentFiles(r)==before);
   ds::ListPage page;CHECK(r.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok&&page.items.size()==1);
  }
  r.platform.allocate_result=r.platform.backing_result=r.platform.publish_result=r.platform.mount_result=u::Status::Ok;
  r.platform.wire="Draftor T04. Inbox 导入测试。";r.platform.filename="T04_IMPORT.TXT";CHECK(r.platform.wire.size()==34);
  r.receive();const auto receipt=r.transfer.imported();CHECK(receipt.outcome==u::Outcome::Imported&&receipt.operation&&receipt.bodyBytes==34);
  ds::DocumentId created;created.bytes=receipt.document;CHECK(created!=original&&!created.empty());
  CHECK(r.app.session.currentId()==original&&r.app.document.text()=="T04 protected original"&&currentFiles(r)==before);
  u::ImportResult queried;CHECK(r.sink.query(receipt.operation,queried)==u::Status::Ok&&queried.document==receipt.document&&queried.outcome==u::Outcome::Imported);
  const auto body=r.app.persistence.loadLatest(created);CHECK(body.status==ds::Status::Ok&&std::string_view(body.snapshot.text)==std::string_view(r.platform.wire));
  r.close();CHECK(!r.platform.unsafe_free&&!r.app.commandBarrier()&&currentFiles(r)==before);
  ds::DocumentMetadata meta;CHECK(r.app.catalog.get(original,&meta)==ds::Status::Ok&&meta.name==originalMeta.name);
  std::cout<<"PASS "<<names[scenario]<<" cleanup/retry CreateNew receipt readback=34 CURRENT_bytes_unchanged=1\n";
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
