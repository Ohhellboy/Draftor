#include "namespace_usb_fixture.hpp"
#include <map>
static unsigned originalOpens=0;
static void observe(const char* path,const char*){
 if(std::strstr(path,"/HANIME_S3_TEST/")&&!std::strstr(path,"/WAVESHARE_RLCD42/"))++originalOpens;
}
static void pathsAndFaults(){
 Rig r;originalOpens=0;s3fs::File::openObserver=observe;
 const char* path="/HANIME_S3_TEST/v01/path.bin";
 CHECK(r.storage.write(path,"WS content")==ds::Status::Ok);
 CHECK(r.storage.read(path,100).bytes=="WS content");
 s3fs::Fs old(r.path.string());CHECK(!old.exists(path));
 CHECK(old.openFile("/HANIME_S3_TEST/v01/CURRENT.A")); // deliberate contrast check
 originalOpens=0;
 for(const auto* bad:{"/HANIME_S3_TEST/v01/../CURRENT.A","/HANIME_S3_TEST2/v01/a","/sd/HANIME_S3_TEST/v01/a","/WAVESHARE_RLCD42/HANIME_S3_TEST/v01/a","relative","//HANIME_S3_TEST/v01/a","/HANIME_S3_TEST/v01//a","/HANIME_S3_TEST/v01/./a","/HANIME_S3_TEST/v01/a\\b","/HANIME/IME/shared.hns","/PICO2W/a"}){
  CHECK(!r.files.openFile(bad,s3fs::Write));CHECK(!r.files.remove(bad));CHECK(!r.files.rename(path,bad));
  CHECK(r.storage.write(bad,"bad")==ds::Status::InvalidName);
 }
 CHECK(r.storage.rename(path,"/HANIME_S3_TEST/v01/renamed.bin")==ds::Status::Ok);
 path="/HANIME_S3_TEST/v01/renamed.bin";
 auto list=r.storage.list("/HANIME_S3_TEST/v01/",{},32);CHECK(list.status==ds::Status::Ok);
 s3fs::faults.max_read_bytes=2;CHECK(r.storage.read(path,100).bytes=="WS content");s3fs::faults={};
 s3fs::faults.read=true;CHECK(r.storage.read(path,100).status==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.write_bytes=2;CHECK(r.storage.write(path,"cannot succeed")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.flush=true;CHECK(r.storage.write(path,"sync failure")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.close=true;CHECK(r.storage.write(path,"close failure")==ds::Status::IoError);s3fs::faults={};
 s3fs::faults.rename=true;CHECK(r.storage.rename(path,"/HANIME_S3_TEST/v01/other")==ds::Status::IoError);s3fs::faults={};
 CHECK(r.storage.write(path,"retired")==ds::Status::Ok);
 CHECK(r.storage.cooperativeOpen(path,ds::CooperativeOpenMode::Read).status==ds::Status::Ok);
 const auto epoch=r.storage.mediumSession().mount_epoch;
 r.storage.noteUnmounted();CHECK(!r.storage.mediumSession().available);std::uint8_t data[12]{};
 CHECK(r.storage.cooperativeRead(data,sizeof(data)).status!=ds::Status::Ok);
 CHECK(r.storage.write(path,"after unmount")==ds::Status::MediaUnavailable);
 r.storage.noteMounted();CHECK(r.storage.mediumSession().mount_epoch!=epoch);
 CHECK(r.storage.cooperativeRead(data,sizeof(data)).status!=ds::Status::Ok);
 CHECK(r.storage.read(path,100).bytes=="retired");
 fs::create_directory_symlink(r.path/"HANIME_S3_TEST/v01",r.path/"WAVESHARE_RLCD42/HANIME_S3_TEST/v01/escape");
 CHECK(r.storage.read("/HANIME_S3_TEST/v01/escape/CURRENT.A",100).status==ds::Status::IoError);
 CHECK(r.storage.write("/HANIME_S3_TEST/v01/escape/CURRENT.A","bad")==ds::Status::IoError);
 CHECK(!r.files.rename(path,"/HANIME_S3_TEST/v01/escape/CURRENT.A"));
 CHECK(!originalOpens);fs::remove(r.path/"WAVESHARE_RLCD42/HANIME_S3_TEST/v01/escape");
 std::cout<<"PASS mapped read/write/list/rename/remove, cross-space and traversal rejection, short-read assembly, read/write/sync/close/rename faults, unmount/remount, Host symlink rejection\n";
}
static void lifecycle(){
 Rig r;originalOpens=0;s3fs::File::openObserver=observe;
 CHECK(r.app.document.text().empty());CHECK(r.app.commitText("微雪独立文稿",r.app.editStamp()));CHECK(r.app.session.manualSave(++r.now));
 const auto first=r.app.session.currentId();CHECK(r.app.session.rename(first,"微雪改名")==ds::Status::Ok);
 ds::DocumentId second;CHECK(r.app.session.create("第二篇",++r.now,&second));CHECK(second!=first);
 CHECK(r.app.commitText("第二篇内容",r.app.editStamp()));CHECK(r.app.session.manualSave(++r.now));
 CHECK(r.app.session.moveToTrash(first,++r.now));CHECK(r.app.session.restoreFromTrash(first));CHECK(r.app.session.open(first,++r.now));
 CHECK(r.app.document.text()=="微雪独立文稿");
 s3app::App reopened(r.storage,{},919,r.font);CHECK(reopened.startup(++r.now));CHECK(reopened.session.currentId()==first&&reopened.document.text()=="微雪独立文稿");
 CHECK(!originalOpens);
 std::cout<<"PASS create/save/rename/trash/restore/open/CURRENT session recovery using actual document system, zero original namespace opens\n";
}
static void missingAndUnknown(){
 const auto root=Rig::temp();s3fs::Fs ws((root/"WAVESHARE_RLCD42").string(),true);ds::ArduinoSdStoragePort storage(ws);
 storage.noteMounted();originalOpens=0;s3fs::File::openObserver=observe;
 CHECK(storage.read("/HANIME_S3_TEST/v01/CURRENT.A",100).status==ds::Status::NotFound);
 CHECK(draftor::board::prepareNamespace(root.c_str())==draftor::board::NamespaceResult::Ready);CHECK(draftor::board::prepareNamespace(root.c_str())==draftor::board::NamespaceResult::Ready);
 CHECK(storage.read("/HANIME_S3_TEST/v01/CURRENT.A",100).status==ds::Status::NotFound);
 CHECK(storage.mkdirs("/HANIME_S3_TEST/v01")==ds::Status::Ok);CHECK(storage.write("/HANIME_S3_TEST/v01/CURRENT.A","broken")==ds::Status::Ok);
 s3host::BuiltinSmallFont font;s3app::App app(storage,{},920,font);const auto result=app.startup(1);(void)result;
 CHECK(app.document.text().empty()&&!originalOpens); // corruption may fail or repair locally, never old CURRENT
 verifySentinels(root);s3fs::File::openObserver=nullptr;fs::remove_all(root);
 for(unsigned kind=0;kind<4;++kind){
  const auto p=Rig::temp();
  if(kind==0)fs::create_directories(p/"WAVESHARE_RLCD42");
  if(kind==1)put(p/"WAVESHARE_RLCD42/.draftor-namespace","unknown version");
  if(kind==2)put(p/"WAVESHARE_RLCD42","not directory");
  if(kind==3)fs::create_directory_symlink(p/"HANIME_S3_TEST",p/"WAVESHARE_RLCD42");
  CHECK(draftor::board::prepareNamespace(p.c_str())==draftor::board::NamespaceResult::Invalid);verifySentinels(p);fs::remove_all(p);
 }
 std::cout<<"PASS absent/empty/corrupt namespace has no original fallback; unknown existing directory/file/marker/symlink is not adopted\n";
}
static void resources(){
 auto mount=std::make_shared<ime_device::Mount>();
 ime_device::Resources good(RESOURCE_ROOT,mount);CHECK(good.checkInstallation());good.start(hanime_extension::SchemeRules::R1Xiaohe);
 for(unsigned n=0;!good.ready()&&good.state()!=ime_device::ResourceState::Error&&n<2000000;++n)good.service();
 CHECK(good.ready());good.retire();
 ime_device::TfReader reader(mount,std::string(RESOURCE_ROOT)+"/shared.hns",0);CHECK(reader.valid());
 std::uint8_t b[16]{};CHECK(reader.read(3,b,sizeof(b)));++mount->epoch;CHECK(!reader.valid()&&!reader.read(3,b,sizeof(b)));
 const auto root=Rig::temp();const auto wrong=root/"wrong-resource";fs::create_directories(wrong);
 fs::copy(RESOURCE_ROOT,wrong,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
 // This release uses PackedAuth: active shared data lives in shared.hnv.
 // shared.hns is retained in the bundle but is not the format-2 read source.
 {std::fstream file(wrong/"shared.hnv",std::ios::in|std::ios::out|std::ios::binary);char c=0;file.read(&c,1);c^=0x7f;file.seekp(0);file.write(&c,1);CHECK(file.good());}
 ime_device::Resources bad(wrong.string());
 if(bad.checkInstallation()){bad.start(hanime_extension::SchemeRules::R1Xiaohe);for(unsigned n=0;bad.state()!=ime_device::ResourceState::Error&&!bad.ready()&&n<2000000;++n)bad.service();}
 CHECK(bad.state()==ime_device::ResourceState::Error);bad.retire();fs::remove_all(root);
 std::cout<<"PASS exact format4 read-only resource admission, pinned authentication rejects damaged bundle; old resource reader invalid after epoch change\n";
}
int main(){try{pathsAndFaults();lifecycle();missingAndUnknown();flows();resources();std::cout<<"PASS namespace isolation and actual USB import/export; protected sentinels byte-identical\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
