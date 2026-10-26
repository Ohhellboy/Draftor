// Host-only probe: reuse the actual Wi-Fi import and SD/catalog adapters.
// No product instrumentation, capacity, transaction or scheduling changes.
#define main inherited_wifi_suite_main
#include "wifi_product_flow.cpp"
#undef main
#include "operation_profile.hpp"
#include "docmanager_display.hpp"
#include "support/r2_raster.hpp"
#include "support/menu_measure_probe.hpp"

template<class F> s3profile::Record sample(Rig& r,unsigned total,bool collision,const char* label,F action){
 if(s3profile::active)(void)s3profile::finish();s3profile::begin(label);action();const auto p=s3profile::finish();
 std::cout<<"CATALOG total="<<total<<" collision="<<collision<<" operation="<<label
  <<" cache_entries="<<r.app.catalog.cachedEntries()<<" reads="<<p.reads<<" read_bytes="<<p.read_bytes
  <<" opendir="<<p.calls[s3profile::Opendir].count<<" opens="<<p.opens<<" writes="<<p.writes
  <<" host_us="<<p.total_us<<'\n';return p;
}
static unsigned paintOpens=0;
using CountedMenuFont=menu_probe::CountedMenuFont;
static void paintPage(Rig& r,unsigned total,bool collision,const char* shape){
 CHECK(r.app.filesOpen());const auto& state=r.app.manager.state();
 const auto selected=state.selected,offset=state.page.offset,count=state.page.items.size();
 const auto id=r.app.session.currentId(),rowId=state.page.items[selected].id;
 const auto revision=r.app.document.revision();
 r2preview::Raster raster;CountedMenuFont face;unsigned rasterLookups=0,rasterMetrics=0;paintOpens=0;
 s3fs::File::openObserver=[](const char*,const char*){++paintOpens;};
 const auto p=sample(r,total,collision,"menu_layout_and_raster",[&]{
  CHECK(s3app::display::documentMenu(r.app,face,raster.rect(),raster.aux(),
   [&](std::string_view text,int x,int y,bool inverse){
    const auto before=face.lookups,metrics=face.metrics;raster.text(text,x,y,inverse,face);rasterLookups+=face.lookups-before;rasterMetrics+=face.metrics-metrics;
   }));
  s3app::display::menuFeedback(r.app,raster.rect(),raster.aux());
 });
 s3fs::File::openObserver=nullptr;
 CHECK(p.reads==0&&p.writes==0&&p.opens==0&&p.calls[s3profile::Opendir].count==0&&paintOpens==0);
 CHECK(state.selected==selected&&state.page.offset==offset&&state.page.items.size()==count&&state.page.items[selected].id==rowId);
 CHECK(r.app.session.currentId()==id&&r.app.document.revision()==revision);
 const auto ink=std::count(raster.pixels.begin(),raster.pixels.end(),0);
 CHECK(ink>0&&face.lookups>0);
 std::cout<<"PAINT total="<<total<<" names="<<shape<<" page_items="<<count
  <<" selected_bytes="<<state.page.items[selected].name.size()<<" content_glyph_lookups="<<face.lookups
  <<" layout_glyph_lookups="<<face.lookups-rasterLookups<<" raster_glyph_lookups="<<rasterLookups
  <<" metric_queries="<<face.metrics<<" layout_metric_queries="<<face.metrics-rasterMetrics<<" raster_metric_queries="<<rasterMetrics
  <<" text_calls="<<raster.calls<<" ink_pixels="<<ink<<" storage_opens="<<paintOpens
  <<" HOST_ONLY_DEVICE_DRAW_TIME_NOT_MEASURED\n";
}
void threshold(unsigned before,bool collision,bool longNames=false){
 Rig r;CHECK(ds::DocumentCatalog::CacheEntries==128);const auto trashed=r.app.session.currentId();
 std::cout<<"SCENARIO total="<<before+1<<" collision="<<collision<<" names="<<(longNames?"96_byte_ascii":"short")<<'\n';
 CHECK(r.app.session.rename(trashed,"恢复目标")==ds::Status::Ok);
 CHECK(r.app.session.create("保留正文",++r.now));CHECK(r.app.session.moveToTrash(trashed,++r.now));
 if(collision)CHECK(r.app.session.create("恢复目标",++r.now));
 for(unsigned i=collision?3:2;i<before;++i){
  char suffix[7];std::snprintf(suffix,sizeof(suffix),"%06u",i);
  const std::string name=longNames?std::string(90,'W')+suffix:std::string();
  CHECK(r.app.session.create(name,++r.now));
 }
 ds::ListPage page;
 CHECK(r.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);
 const auto warm=sample(r,before,collision,"warm_before_import",[&]{CHECK(r.app.catalog.list(ds::DocumentArea::Normal,0,16,&page)==ds::Status::Ok);});
 if(before<=128)CHECK(warm.reads==0&&r.app.catalog.cachedEntries()==before);
 else CHECK(warm.reads>0&&r.app.catalog.cachedEntries()==0);
 const auto original=r.app.session.currentId();r.on();const auto operation=r.ticket();
 CHECK(r.request(r.upload("新导入正文\n",operation,"imported.txt")).find("200 OK")!=std::string::npos);r.wifi.stop();
 CHECK(r.app.session.currentId()==original&&!r.app.transferActive());
 const unsigned total=before+1;std::cout<<"IMPORT total="<<total<<" cache_entries="<<r.app.catalog.cachedEntries()<<" invalidations="<<r.app.catalog.cacheStats.invalidations<<'\n';
 const auto first=sample(r,total,collision,"F2_after_import",[&]{r.app.key({1,++r.now,59,0,true});CHECK(r.app.filesOpen());});
 if(longNames)r.app.manager.moveSelection(1); // select one real 96-byte row, no metadata mutation
 paintPage(r,total,collision,longNames?"96_byte_ascii":"short");
 r.app.key({1,++r.now,41,0,true});CHECK(!r.app.filesOpen());
 const auto repeat=sample(r,total,collision,"F2_repeated",[&]{r.app.key({1,++r.now,59,0,true});CHECK(r.app.filesOpen());});
 const auto next=sample(r,total,collision,"next_batch",[&]{CHECK(r.app.manager.nextPage()==ds::Status::Ok);});
 const auto third=sample(r,total,collision,"third_batch",[&]{CHECK(r.app.manager.nextPage()==ds::Status::Ok);});
 CHECK(r.app.manager.state().page.offset==32);
 sample(r,total,collision,"T_trash",[&]{r.app.key({1,++r.now,23,0,true});CHECK(r.app.manager.state().view==ds::FileManagerView::Trash);});
 CHECK(r.app.manager.state().page.items.size()==1&&r.app.manager.state().page.items[0].id==trashed);
 const auto restored=sample(r,total,collision,"Enter_restore",[&]{r.app.key({1,++r.now,40,0,true});CHECK(r.app.lastOperation.status==ds::Status::Ok);});
 ds::DocumentMetadata meta;CHECK(r.app.catalog.get(trashed,&meta)==ds::Status::Ok&&meta.area==ds::DocumentArea::Normal);
 CHECK(meta.name==(collision?"恢复目标-01":"恢复目标"));CHECK(r.app.session.currentId()==original);
 if(total<=128){CHECK(first.reads==0&&repeat.reads==0&&next.reads==0&&third.reads==0&&r.app.catalog.cachedEntries()==128);}
 else{
  CHECK(r.app.catalog.cachedEntries()==0&&repeat.reads>0&&restored.reads>repeat.reads);
  CHECK(repeat.calls[s3profile::Opendir].count==1&&next.calls[s3profile::Opendir].count==2&&third.calls[s3profile::Opendir].count==3);
  CHECK(next.reads==repeat.reads*2&&third.reads==repeat.reads*3);
  if(before<=128)CHECK(first.reads>repeat.reads);else CHECK(first.reads==repeat.reads);
 }
 std::cout<<"PASS unchanged production WiFi import/F2/restore count="<<total<<" collision="<<collision<<" DEVICE_TIME_NOT_MEASURED\n";
}
int main(){try{threshold(127,false);threshold(128,false);threshold(127,false,true);threshold(128,true,true);threshold(159,false);return 0;}
catch(const std::exception& e){s3fs::File::openObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
