#define main historical_main
#include "query_rows.cpp"
#undef main
#include "input_route.hpp"
#include "operation_profile.hpp"
void pump(Rig& r){for(unsigned i=0;i<10000;++i){r.app.service(++r.now,false);if(!r.app.commandBarrier()&&!r.app.persistence.backgroundSaveActive())return;}CHECK(false);}
void save(Rig& r){r.app.key({1,++r.now,22,1,true});pump(r);CHECK(!r.app.document.dirty());}
void setup(Rig& r){CHECK(r.app.commitText("first",r.app.editStamp()));save(r);CHECK(r.app.session.create({},++r.now));CHECK(r.app.commitText("second",r.app.editStamp()));save(r);r.app.deferFileCommands(true);}
void menus(Rig& r){
 r.app.key({1,++r.now,18,8,true});CHECK(r.app.fileRequestPending()&&!r.app.filesOpen());
 const auto serial=r.app.operationSerial;r.app.service(++r.now,false);CHECK(r.app.operationSerial==serial);
 r.app.fileFramePresented(clockUs(),true);pump(r);CHECK(r.app.filesOpen());
 const auto& page=r.app.manager.state().page;size_t index=0;while(index<page.items.size()&&page.items[index].id==r.app.session.currentId())++index;CHECK(index<page.items.size());
 while(r.app.manager.state().selected<index)r.app.key({1,++r.now,81,0,true});
}
void run(unsigned failure){
 Rig r;setup(r);if(failure==5||failure==7)CHECK(r.app.commitText(" newer",r.app.editStamp()));menus(r);const auto old=r.app.session.currentId();const auto oldText=std::string(r.app.document.text());
 const auto chosen=r.app.manager.state().page.items[r.app.manager.state().selected].id;
 if(s3profile::active)s3profile::finish();
 r.app.key({1,++r.now,40,0,true});CHECK(r.app.fileRequestPending());const auto request=r.app.fileTiming.serial;const auto oldFocus=r.app.focusToken();
 CHECK(s3profile::record.opens==0&&r.app.session.currentId()==old);
 r.app.key({1,++r.now,40,0,true});r.app.key({1,++r.now,40,0,false});CHECK(r.app.fileTiming.serial==request);
 r.app.service(++r.now,false);CHECK(r.app.session.currentId()==old&&s3profile::record.opens==0);
 if(failure==1){r.app.key({1,++r.now,41,0,true});CHECK(r.app.fileTiming.cancelled);}
 else if(failure==2){r.storage.noteUnmounted();}
 else if(failure==3){s3fs::faults.read=true;}
 else if(failure==4){r.app.manager.moveSelection(r.app.manager.state().selected? -1:1);}
 else if(failure==6){r.app.disconnected();}
 else if(failure==7){s3fs::faults.write_bytes=0;}
 if(!failure)s3fs::File::openObserver=[](const char* path,const char* mode){std::cout<<"FILE_OPEN mode="<<mode<<" path="<<fs::path(path).string()<<'\n';};
 r.app.fileFramePresented(clockUs(),true);pump(r);s3fs::faults={};s3fs::File::openObserver=nullptr;
 if(failure&&failure!=5){CHECK(r.app.session.currentId()==old&&r.app.document.text()==std::string_view(oldText));if(failure==7)CHECK(r.app.document.dirty());}
 else {CHECK(r.app.session.currentId()==chosen&&r.app.document.text()=="first");CHECK(r.app.fileTiming.install_us>=r.app.fileTiming.start_us);CHECK(r.app.fileTiming.start_us>=r.app.fileTiming.feedback_us);r.app.fileFramePresented(clockUs(),false);CHECK(r.app.fileTiming.body_lcd_us>=r.app.fileTiming.complete_us);}
 if(!failure){
  s3hid::InputRoute route;route.framePresented(oldFocus);s3hid::Event oldEnter{1,++r.now,40,0,true,route.epoch()};
  route.framePresented(r.app.focusToken());CHECK(!route.accept(oldEnter,r.now));oldEnter.down=false;CHECK(!route.accept(oldEnter,r.now));
  CHECK(r.app.document.text()=="first");
 }
 const auto result=s3profile::active?s3profile::finish():s3profile::Record{};
 std::cout<<"FILE scenario="<<failure<<" opens="<<result.opens<<" reads="<<result.reads<<" read_bytes="<<result.read_bytes<<" request="<<request<<" feedback="<<r.app.fileTiming.feedback_us<<" start="<<r.app.fileTiming.start_us<<" install="<<r.app.fileTiming.install_us<<" done="<<r.app.fileTiming.complete_us<<" cancelled="<<r.app.fileTiming.cancelled<<'\n';
}
int main(){try{for(unsigned i:{0u,1u,2u,3u,4u,5u,6u,7u})run(i);std::cout<<"PASS A3 LCD gate, one request, cancel, removal, read failure, target change, disconnect\n";return 0;}catch(const std::exception& e){s3fs::faults={};std::cerr<<e.what()<<'\n';return 1;}}
