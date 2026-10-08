// Reuse the product USB fixture, transport model and real persistence owners.
#define main inherited_usb_flow_main
#include "usb_product_flow.cpp"
#undef main
int main(){try{
 const std::string text="他说：“你好，Draftor！”（真的？）《草稿》；今天……继续——\n"
  "Hello, world! Don't change v1.2, test@example.com or 3.14.\n"
  "，。！？；：、（）【】《》“”‘’「」『』……——\n";
 for(bool vertical:{false,true}){
  Rig r;
  CHECK(r.app.commitText(text,r.app.editStamp()));
  const auto bytes=std::string(r.app.document.text());
  const auto cursor=r.app.document.cursorByte();const auto revision=r.app.document.revision();
  if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}
  CHECK(std::string(r.app.document.text())==bytes&&r.app.document.cursorByte()==cursor&&r.app.document.revision()==revision);
  CHECK(r.app.session.manualSave(++r.now));
  s3app::App reopened(r.storage,{},918,r.font);CHECK(reopened.startup(++r.now));
  CHECK(std::string(reopened.document.text())==text);
  exported(r,text);
  // Existing UTF-8 import semantics: no per-key punctuation mapping, even
  // for pre-existing compatibility characters imported alongside ASCII.
  r.platform.wire=text+"ASCII ,.;:!?()[] @# 3.14 / "+"︱﹁﹂";
  r.receive();CHECK(r.transfer.imported().outcome==u::Outcome::Imported);
  ds::DocumentId incoming;incoming.bytes=r.transfer.imported().document;r.close();
  CHECK(r.app.enterTransfer());CHECK(r.app.openImported(incoming,++r.now));r.app.leaveTransfer();
  CHECK(std::string(r.app.document.text())==r.platform.wire);
  CHECK(r.app.session.manualSave(++r.now));exported(r,r.platform.wire);
  std::cout<<"PASS punctuation persistence/import/USB export exact bytes layout="<<vertical<<" bytes="<<text.size()<<'\n';
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
