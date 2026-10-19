// SPDX-License-Identifier: Apache-2.0
#include "wifi_transfer.hpp"
#include "hanime_document_sd_storage.hpp"
#include "src/hanime_document/paths.hpp"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
namespace fs=std::filesystem;namespace ds=hanime::documents;namespace ex=draftor::exchange;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
struct Network final:s3app::WifiTransport {
 std::string in,out;std::size_t at=0,chunk=127,live=0,peak=0;bool online=false,queued=false,connected=false,eof=false,stall=false,fail_start=false;
 unsigned starts=0,allocations=0,releases=0;
 std::string ssid_value="Draftor-test",password_value="12345678abcdef90",address_value="http://192.168.4.1";
 bool start(char* s,std::size_t sn,char* p,std::size_t pn,char* t,std::size_t)noexcept override{++starts;online=true;if(fail_start||ssid_value.size()>=sn||password_value.size()>=pn)return false;std::strcpy(s,ssid_value.c_str());std::strcpy(p,password_value.c_str());std::strcpy(t,"0123456789abcdef0123456789abcdef");return true;}
 const char* startError()const noexcept override{return "Wi-Fi needs more internal memory";}
 const char* address()const noexcept override{return address_value.c_str();}
 void stop()noexcept override{close();online=false;}
 bool accept()noexcept override{if(queued){queued=false;connected=true;return true;}return false;}
 int receive(char* p,std::size_t n)noexcept override{if(!connected)return -1;if(at==in.size())return eof?0:-2;n=std::min({n,chunk,in.size()-at});std::memcpy(p,in.data()+at,n);at+=n;return int(n);}
 int send(const char* p,std::size_t n)noexcept override{if(!connected)return -1;if(stall)return -2;n=std::min(n,chunk);out.append(p,n);return int(n);}
 void close()noexcept override{connected=false;}
 char* allocate(std::size_t n)noexcept override{++allocations;char* p=static_cast<char*>(draftor::memory::allocate(n));if(p){live+=n;peak=std::max(peak,live);}return p;}
 void deallocate(char* p,std::size_t n)noexcept override{++releases;live-=n;draftor::memory::release(p);}
 void request(std::string s){CHECK(!connected);in=std::move(s);out.clear();at=0;queued=true;eof=false;stall=false;}
};
struct Rig {
 fs::path path; s3fs::Fs files;ds::ArduinoSdStoragePort storage;s3host::BuiltinSmallFont font;s3app::App app;Network net;s3app::UsbDocumentImport sink;s3app::WifiTransfer wifi;std::uint32_t now=0;
 static fs::path temp(){const auto base=std::getenv("TMPDIR");CHECK(base);std::string p=(fs::path(base)/"wifi-doc-XXXXXX").string();CHECK(mkdtemp(p.data()));return p;}
 Rig():path(temp()),files(path.string()),storage(files),app(storage,{},90317,font),sink(storage,app.catalog,app.persistence,app.session,app.editorPort),wifi(app,net,sink){storage.noteMounted();CHECK(app.startup(++now));}
 ~Rig(){s3fs::faults={};draftor::memory::allocation_fail_after=-1;wifi.stop();std::error_code e;fs::remove_all(path,e);}
 void on(){CHECK(wifi.start({}));CHECK(wifi.panel());CHECK(wifi.key({1,++now,41,0,true},{}));CHECK(!wifi.blocksInput());}
 void tick(){wifi.tick(++now,{});}
 void until(s3app::WifiPhase p){for(unsigned i=0;i<50000;++i){tick();if(wifi.phase()==p)return;}CHECK(false);}
 std::string request(const std::string& s){net.request(s);tick();until(s3app::WifiPhase::Listening);return net.out;}
 std::string get(const std::string& path){return "GET "+path+" HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";}
 std::string auth()const{return "X-Draftor-Token: "+std::string(wifi.token())+"\r\n";}
 std::string ticket(){auto r=request("POST /ticket HTTP/1.1\r\nHost: 192.168.4.1\r\n"+auth()+"Content-Length: 0\r\n\r\n");CHECK(r.find("200 OK")!=r.npos);auto start=r.find("\"operation\":\"");CHECK(start!=r.npos);return r.substr(start+13,16);}
 std::string upload(const std::string& text,const std::string& op,const std::string& name="hello.txt",std::size_t length=SIZE_MAX){return "POST /import?op="+op+"&name="+name+" HTTP/1.1\r\nHost: 192.168.4.1\r\n"+auth()+"Content-Type: application/octet-stream\r\nContent-Length: "+std::to_string(length==SIZE_MAX?text.size():length)+"\r\n\r\n"+text;}
 std::size_t count(){ds::ListPage p;CHECK(app.catalog.list(ds::DocumentArea::Normal,0,16,&p)==ds::Status::Ok);return p.items.size();}
 std::string loaded(){ds::DocumentId id;id.bytes=wifi.lastImport().document;auto r=app.persistence.loadLatest(id);CHECK(r.status==ds::Status::Ok);return std::string(r.snapshot.text);}
};
std::string body(const std::string& r){auto at=r.find("\r\n\r\n");CHECK(at!=r.npos);return r.substr(at+4);}
void exportEditing(){
 for(bool vertical:{false,true}){
 Rig r;CHECK(!r.wifi.active()&&!r.net.online);CHECK(r.app.commitText("中文 initial\n",r.app.editStamp()));if(vertical){r.app.layoutKey({1,++r.now,61,0,true});r.app.layoutKey({1,++r.now,61,0,false});}
 const auto layout=r.app.layoutMode();const auto id=r.app.session.currentId();const auto revision=r.app.document.revision();const auto cursor=r.app.document.cursorByte();const auto undo=r.app.editor.undoCount();const auto before=std::string(r.app.document.text());r.on();const auto focus=r.app.focusToken();
 r.net.request(r.get("/export?token="+std::string(r.wifi.token())));r.tick();r.until(s3app::WifiPhase::Send);
 CHECK(r.app.focusToken()==focus&&!r.app.transferActive()&&r.app.document.dirty()&&r.app.document.revision()==revision&&r.app.document.cursorByte()==cursor&&r.app.editor.undoCount()==undo);
 CHECK(r.app.commitText("later input",r.app.editStamp()));r.until(s3app::WifiPhase::Listening);CHECK(body(r.net.out)==before);CHECK(r.net.live==0&&r.app.session.currentId()==id&&r.app.layoutMode()==layout);CHECK(r.app.document.dirty());
 r.wifi.stop();CHECK(!r.net.online&&!r.app.transferActive());CHECK(r.app.commitText("after off",r.app.editStamp()));
 }
}
void boundaries(){
 for(const auto& text:{std::string(),std::string("中文\n"),std::string(65536,'x')}){Rig r;r.on();auto op=r.ticket();auto res=r.request(r.upload(text,op));CHECK(res.find("200 OK")!=res.npos&&r.loaded()==text&&r.count()==2);CHECK(r.wifi.lastImport().bodyBytes==text.size());}
 Rig r;r.on();std::string wire="\xef\xbb\xbf";for(unsigned i=0;i<65536;++i)wire+="\r\n";auto op=r.ticket();CHECK(r.request(r.upload(wire,op)).find("200 OK")!=std::string::npos);CHECK(r.loaded()==std::string(65536,'\n'));
}
void exportBoundary(){Rig r;r.on();CHECK(body(r.request(r.get("/export?token="+std::string(r.wifi.token())))).empty());auto op=r.ticket();CHECK(r.request(r.upload(std::string(65536,'x'),op)).find("200 OK")!=std::string::npos);ds::DocumentId id;id.bytes=r.wifi.lastImport().document;CHECK(r.app.session.open(id,++r.now));CHECK(body(r.request(r.get("/export?token="+std::string(r.wifi.token()))))==std::string(65536,'x'));CHECK(r.net.peak==65536&&r.net.live==0);}
void invalid(){for(const auto& text:{std::string("\xff"),std::string("\xff")+std::string(65536,'x'),std::string("x\0y",3),std::string("\xe4\xb8",2),std::string(65537,'x'),std::string(131076,'x')}){Rig r;r.on();auto op=r.ticket();auto res=r.request(r.upload(text,op));CHECK(res.find("422 Error")!=res.npos||res.find("413 Error")!=res.npos);CHECK(r.count()==1&&!r.app.transferActive()&&!r.app.session.busy());}}
void filenames(){Rig r;r.on();for(const auto& n:{"..%2fx.txt","%2Ftmp%2Fx.txt","a%00.txt","no.bin"}){auto op=r.ticket();CHECK(r.request(r.upload("test",op,n)).find("400 Error")!=std::string::npos);CHECK(r.count()==1);}for(unsigned i=0;i<2;++i){auto op=r.ticket();CHECK(r.request(r.upload("中文",op,"%E7%A8%BF%E4%BB%B6.txt")).find("200 OK")!=std::string::npos);}CHECK(r.count()==3);}
void currentAndRestart(){
 Rig r;CHECK(r.app.commitText("original unsaved",r.app.editStamp()));const auto original=r.app.session.currentId();const auto cursor=r.app.document.cursorByte();const auto rev=r.app.document.revision();r.on();auto op=r.ticket();r.net.request(r.upload("new\r\n文档",op));r.tick();r.until(s3app::WifiPhase::Upload);CHECK(r.app.transferActive()&&!r.app.commitText("blocked",r.app.editStamp()));r.until(s3app::WifiPhase::Listening);
 CHECK(r.app.session.currentId()==original&&r.app.document.text()=="original unsaved"&&r.app.document.cursorByte()==cursor&&r.app.document.revision()==rev&&r.app.document.dirty());CHECK(r.loaded()=="new\n文档");r.wifi.stop();
 s3app::App reboot(r.storage,{},90318,r.font);CHECK(reboot.startup(++r.now));CHECK(reboot.session.currentId()==original);ds::DocumentId id;id.bytes=r.wifi.lastImport().document;CHECK(reboot.persistence.loadLatest(id).snapshot.text=="new\n文档");
}
void disconnect(){
 Rig r;r.on();const auto id=r.app.session.currentId();auto op=r.ticket();r.net.request(r.upload("partial",op,"half.txt",999));r.tick();r.until(s3app::WifiPhase::Upload);r.net.eof=true;r.until(s3app::WifiPhase::Listening);CHECK(r.count()==1&&!r.app.transferActive()&&!r.app.session.busy());
 CHECK(r.request(r.get("/receipt?token="+std::string(r.wifi.token())+"&op="+op)).find("retrySafe")!=std::string::npos);
 op=r.ticket();CHECK(r.request(r.upload("recovered",op)).find("200 OK")!=std::string::npos);CHECK(r.count()==2&&r.app.session.currentId()==id);
}
void receiptLoss(){
 Rig r;r.on();const auto op=r.ticket();r.net.request(r.upload("exactly once",op));r.tick();r.until(s3app::WifiPhase::Send);r.net.connected=false;r.until(s3app::WifiPhase::Listening);CHECK(r.count()==2);
 auto result=r.request(r.get("/receipt?token="+std::string(r.wifi.token())+"&op="+op));CHECK(result.find("\"imported\":true")!=result.npos);CHECK(r.request(r.upload("exactly once",op)).find("200 OK")!=std::string::npos);CHECK(r.count()==2);
}
void faults(){for(unsigned mode=0;mode<4;++mode){Rig r;r.on();const auto id=r.app.session.currentId();auto op=r.ticket();r.net.request(r.upload("original incoming",op));r.tick();r.until(s3app::WifiPhase::Verify);
 if(mode==0)s3fs::faults.write_bytes=0;else if(mode==1)r.storage.noteUnmounted();else if(mode==2){CHECK(r.wifi.key({1,++r.now,67,0,true},{}));CHECK(r.wifi.panel());CHECK(r.wifi.key({1,++r.now,18,0,true},{}));CHECK(!r.wifi.active()&&!r.app.session.busy()&&!r.app.transferActive()&&r.count()==1);continue;}else draftor::memory::allocation_fail_after=0;
 r.until(s3app::WifiPhase::Listening);s3fs::faults={};draftor::memory::allocation_fail_after=-1;CHECK(r.app.session.currentId()==id&&!r.app.session.busy());if(mode!=1)CHECK(r.count()==1);
 }}
void cycles(){Rig r;const auto base=draftor::memory::allocation_stats.live_allocations;for(unsigned i=0;i<24;++i){r.on();r.request(r.get("/export?token="+std::string(r.wifi.token())));r.wifi.stop();CHECK(r.net.live==0&&!r.app.transferActive()&&!r.net.online);CHECK(draftor::memory::allocation_stats.live_allocations==base);}r.net.fail_start=true;CHECK(!r.wifi.start({})&&!r.app.transferActive());}
void startFailure(){
 Rig r;CHECK(r.app.commitText("unsaved 中文",r.app.editStamp()));
 const auto id=r.app.session.currentId();const auto revision=r.app.document.revision();const auto cursor=r.app.document.cursorByte();const auto undo=r.app.editor.undoCount();
 const auto allocations=draftor::memory::allocation_stats.live_allocations;
 r.net.fail_start=true;
 for(unsigned i=0;i<4;++i){
  CHECK(r.wifi.key({1,++r.now,67,0,true},{}));
  CHECK(!r.wifi.active()&&!r.wifi.panel()&&!r.wifi.blocksInput()&&!r.net.online&&!r.app.transferActive());
  CHECK(std::string(r.app.notice)=="Wi-Fi needs more internal memory"&&r.app.dirtyDisplay);
  CHECK(r.app.session.currentId()==id&&r.app.document.text()=="unsaved 中文"&&r.app.document.dirty());
  CHECK(r.app.document.revision()==revision&&r.app.document.cursorByte()==cursor&&r.app.editor.undoCount()==undo);
  CHECK(r.count()==1&&draftor::memory::allocation_stats.live_allocations==allocations);
 }
 s3app::TransferInputState busy{};busy.composition=true;
 CHECK(r.wifi.key({1,++r.now,67,0,true},busy)&&r.net.starts==4);
 CHECK(std::string(r.app.notice)=="Wi-Fi unavailable; finish current action");
 CHECK(r.app.commitText(" input restored",r.app.editStamp()));
 r.net.fail_start=false;r.on();
 CHECK(body(r.request(r.get("/export?token="+std::string(r.wifi.token()))))=="unsaved 中文 input restored");
 r.wifi.stop();CHECK(!r.app.transferActive()&&r.app.session.currentId()==id&&r.app.document.dirty());
}
void admission(){Rig r;CHECK(r.app.commitText("snapshot bytes",r.app.editStamp()));r.on();r.net.request(r.get("/export?token="+std::string(r.wifi.token())));r.tick();r.until(s3app::WifiPhase::Send);r.net.stall=true;r.wifi.tick(r.now+16000,{});CHECK(r.wifi.phase()==s3app::WifiPhase::Listening&&r.net.live==0);CHECK(r.app.enterTransfer());auto result=r.request(r.get("/export?token="+std::string(r.wifi.token())));CHECK(result.find("409 Error")!=result.npos&&r.app.transferActive());r.app.leaveTransfer();draftor::memory::allocation_fail_after=0;result=r.request(r.get("/export?token="+std::string(r.wifi.token())));draftor::memory::allocation_fail_after=-1;CHECK(result.find("507 Error")!=result.npos&&r.app.document.dirty()&&r.net.live==0);}
void protocol(){Rig r;r.on();const auto token=std::string(r.wifi.token());CHECK(r.request(r.get("/session")).find(token)!=std::string::npos);CHECK(r.request(r.get("/")).find("viewport")!=std::string::npos);CHECK(r.request(r.get("/export?token=wrong")).find("403 Error")!=std::string::npos);
 const auto op=r.ticket();const auto good=r.upload("x",op);for(const auto& addition:{"Content-Length: 1\r\n","Transfer-Encoding: chunked\r\n","Origin: https://evil.example\r\n"}){auto bad=good;bad.insert(bad.find("\r\n\r\n")+2,addition);const auto result=r.request(bad);CHECK(result.find("400 Error")!=result.npos||result.find("403 Error")!=result.npos);CHECK(r.count()==1);}
 auto bad=good;bad.replace(bad.find("192.168.4.1"),11,"evil.example");CHECK(r.request(bad).find("403 Error")!=std::string::npos);
 CHECK(r.request("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\nHuge: "+std::string(2200,'x')).find("431 Error")!=std::string::npos);
}
int main(int argc,char** argv){try{CHECK(argc==2);std::string c=argv[1];if(c=="export_editing")exportEditing();else if(c=="boundaries")boundaries();else if(c=="export_boundary")exportBoundary();else if(c=="invalid")invalid();else if(c=="filenames")filenames();else if(c=="current_restart")currentAndRestart();else if(c=="disconnect")disconnect();else if(c=="receipt_loss")receiptLoss();else if(c=="faults")faults();else if(c=="cycles")cycles();else if(c=="start_failure")startFailure();else if(c=="admission")admission();else if(c=="protocol")protocol();else CHECK(false);std::cout<<"PASS "<<c<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
