#define main inherited_wifi_ui_main
#include "wifi_product_flow.cpp"
#undef main
#include "wifi_display.hpp"
#include "reader_display.hpp"
#include "support/r2_raster.hpp"
void menuPreview(Rig& r,const char* name){
 r2preview::Raster canvas;
 const auto phase=r.wifi.phase();const auto page=r.wifi.qr().page();
 const auto allocations=r.net.allocations;const auto id=r.app.session.currentId();const auto revision=r.app.document.revision();
 s3app::paintWifiPanel(r.wifi,canvas.contentFont,canvas.rect(),canvas.aux(),canvas.content());
 CHECK(r.wifi.phase()==phase&&r.wifi.qr().page()==page&&r.net.allocations==allocations);
 CHECK(r.app.session.currentId()==id&&r.app.document.revision()==revision);
 if(page!=s3app::WifiQrPage::Manual&&r.wifi.qr().matrix().valid()){
  const auto matrix=r.wifi.qr().matrix();const auto g=draftor::qr::fit(matrix,s3app::WifiQrRect);
  for(int y=0;y<g.pixels;++y)for(int x=0;x<g.pixels;++x)
   CHECK((canvas.pixels[(g.y+y)*300+g.x+x]==0)==matrix.module(x/g.scale-g.quiet,y/g.scale-g.quiet));
 }
 canvas.save(name);
}
int main(){try{
 Rig r;CHECK(r.wifi.start({}));menuPreview(r,"r2-wifi-connect");
 CHECK(r.wifi.key({1,++r.now,43,0,true},{}));menuPreview(r,"r2-wifi-address");
 CHECK(r.wifi.key({1,++r.now,19,0,true},{}));menuPreview(r,"r2-wifi-manual");
 r.wifi.stop();
 draftor::memory::allocation_fail_after=0;CHECK(r.wifi.start({}));draftor::memory::allocation_fail_after=-1;
 CHECK(r.wifi.qr().status()==draftor::qr::Status::AllocationFailed);menuPreview(r,"r2-wifi-qr-unavailable");r.wifi.stop();
 r.app.key({1,++r.now,59,0,true});CHECK(r.app.filesOpen());
 const auto offset=r.app.manager.state().page.offset;const auto selected=r.app.manager.state().selected;
 CHECK(r.wifi.start({}));CHECK(r.wifi.key({1,++r.now,41,0,true},{}));
 CHECK(r.app.filesOpen()&&r.app.manager.state().page.offset==offset&&r.app.manager.state().selected==selected);
 CHECK(r.wifi.active()&&!r.wifi.panel());r.wifi.stop();
 // The same production report drives labels and values; no partial counts leak.
 auto report=hanime::text_stats::count_utf8("文章 Draftor2026\nHello 世界 123");
 CHECK(report.complete());r2preview::Raster stats;
 s3app::display::statistics(report,stats.contentFont,stats.rect(),stats.aux(),stats.content());
 CHECK(stats.has("中文字数")&&stats.has("当前正文，包含未保存修改"));stats.save("r2-statistics");
 for(auto state:{hanime::text_stats::State::Counting,hanime::text_stats::State::InvalidUtf8,hanime::text_stats::State::InvalidSource}){
  report.state=state;report.counts.characters=999999;r2preview::Raster pending;
  s3app::display::statistics(report,pending.contentFont,pending.rect(),pending.aux(),pending.content());CHECK(!pending.has("999999"));
  if(state==hanime::text_stats::State::Counting)pending.save("r2-statistics-counting");
 }
 std::cout<<"PASS R2 secondary Wi-Fi/state/QR pixels and statistics actual adapters\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
