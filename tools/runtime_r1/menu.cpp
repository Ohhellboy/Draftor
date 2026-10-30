#define main inherited_usb_main
#include "../../integration/product/tests/usb_product_flow.cpp"
#undef main
#include "docmanager_display.hpp"
#include "menu_footer_owner.hpp"
#include "../../integration/product/tests/support/r2_raster.hpp"
static void key(Rig& r,uint8_t u){r.app.key({1,++r.now,u,0,true});r.app.key({1,++r.now,u,0,false});}
static void render(Rig& r,const char* name,bool pairing=false,int bottom=328){
 r2preview::Raster c;const auto before=std::string(r.app.document.text());unsigned opens=0;
 static unsigned* observed=nullptr;observed=&opens;s3fs::File::openObserver=[](const char*,const char*){++*observed;};
 CHECK(s3app::display::documentMenu(r.app,c.contentFont,c.rect(),c.aux(),c.content(),bottom));
 if(!r.app.renameOpen())s3app::display::menuFeedback(r.app,c.rect(),c.aux());
 const auto menu=s3app::display::menuSurface(r.app);
 CHECK(s3app::display::menuOwnsFooter(true,menu));
 s3app::display::menuPairingHint(pairing,123456,bottom,c.rect(),c.aux());
 CHECK(!s3app::display::menuOwnsFooter(false,menu));CHECK(!s3app::display::menuOwnsFooter(true,false));
 CHECK(!opens&&std::string_view(r.app.document.text())==before);CHECK(!c.has("Raw buffered")&&!c.has("not in body"));
 if(pairing)CHECK(c.has("Pair: 123456 + Enter"));
 c.save(name);s3fs::File::openObserver=nullptr;
}
int main(){try{
 Rig r;CHECK(r.app.session.create("清风与写作",++r.now));CHECK(r.app.session.create("Draft 001 移植成功",++r.now));key(r,59);CHECK(r.app.filesOpen());
 render(r,"r1-files");render(r,"r1-files-pair",true);
 key(r,58);CHECK(r.app.menuHelpOpen());render(r,"r1-help");key(r,58);
 key(r,21);CHECK(r.app.renameOpen());render(r,"r1-rename");render(r,"r1-rename-ime-area",false,266);render(r,"r1-rename-ime-pair",true,266);
 r.app.notice="Name limit: 96 valid UTF-8 bytes";render(r,"r1-rename-error",false,266);key(r,41);
 key(r,23);CHECK(r.app.manager.state().view==ds::FileManagerView::Trash);render(r,"r1-trash");
 std::cout<<"PASS menu owner list/trash/rename/help, pairing and rename errors; no storage during painting; candidate area reserved. Raster only, no device timing claim.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
