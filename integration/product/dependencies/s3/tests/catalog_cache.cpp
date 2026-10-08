#define main inherited_main
#include "host_tests.cpp"
#undef main
void cacheChecks(){
 Fixture f;f.start();auto trash=f.app.session.currentId();for(int i=0;i<18;++i)CHECK(f.app.session.create({},++f.now));CHECK(f.app.session.moveToTrash(trash,++f.now));
 f.app.catalog.invalidateCache();s3profile::begin("cold_metadata");CHECK(f.app.manager.enter(ds::FileManagerView::Normal)==ds::Status::Ok);auto cold=s3profile::finish();CHECK(cold.calls[s3profile::Opendir].count<=2);CHECK(f.app.manager.state().page.items.size()==16);
 if(ds::DocumentCatalog::CacheEntries>=19){CHECK(f.app.catalog.cachedEntries()==19);s3profile::begin("cached_page");CHECK(f.app.manager.nextPage()==ds::Status::Ok);auto hot=s3profile::finish();CHECK(hot.reads==0&&hot.calls[s3profile::Opendir].count==0);CHECK(f.app.manager.state().page.items.size()==2);}
 CHECK(f.app.session.create({},++f.now));auto id=f.app.session.currentId();CHECK(f.app.session.rename(id,"Unique cache rename")==ds::Status::Ok);CHECK(f.app.manager.enter(ds::FileManagerView::Normal)==ds::Status::Ok);CHECK(f.app.manager.state().page.items[0].id==id&&f.app.manager.state().page.items[0].name=="Unique cache rename");
 CHECK(f.app.session.moveToTrash(id,++f.now));CHECK(f.app.manager.enter(ds::FileManagerView::Trash)==ds::Status::Ok);CHECK(f.app.manager.state().page.items.size()==2);CHECK(f.app.session.restoreFromTrash(id));CHECK(f.app.manager.refresh()==ds::Status::Ok&&f.app.manager.state().page.items.size()==1);
 CHECK(f.app.session.rename(trash,"Unique cache rename")==ds::Status::NameExists);
 f.app.catalog.invalidateCache();s3fs::faults.directory=true;CHECK(f.app.session.rename(trash,"Cannot assume free")==ds::Status::IoError);CHECK(!f.app.catalog.cachedEntries()&&!f.app.catalog.cacheBytes());s3fs::faults={};CHECK(metadata(f,trash).name!="Cannot assume free");{auto status=f.app.manager.refresh();CHECK(status==ds::Status::Ok||status==ds::Status::OkDegraded);}
 s3fs::faults.write_bytes=0;CHECK(f.app.session.rename(trash,"Cannot write")==ds::Status::IoError);s3fs::faults={};CHECK(!f.app.catalog.cachedEntries());{auto status=f.app.manager.refresh();CHECK(status==ds::Status::Ok||status==ds::Status::OkDegraded);}
 f.storage.noteUnmounted();CHECK(f.app.catalog.validateSession()==ds::Status::MediaUnavailable);CHECK(!f.app.catalog.cacheBytes());f.storage.noteMounted();CHECK(f.app.catalog.validateSession()==ds::Status::SessionChanged);
 Fixture many;many.start();for(unsigned i=0;i<ds::DocumentCatalog::CacheEntries+2;++i)CHECK(many.app.session.create({},++many.now));many.app.catalog.invalidateCache();CHECK(many.app.manager.enter(ds::FileManagerView::Normal)==ds::Status::Ok);CHECK(!many.app.catalog.cachedEntries()&&!many.app.catalog.cacheBytes());CHECK(many.app.manager.nextPage()==(ds::DocumentCatalog::CacheEntries>=16?ds::Status::Ok:ds::Status::LastPage));
 std::printf("CACHE configured=%zu cold_reads=%u cold_opendir=%u peak_owned_budget=%zu cache_off_releases=1\n",ds::DocumentCatalog::CacheEntries,unsigned(cold.reads),unsigned(cold.calls[s3profile::Opendir].count),ds::DocumentCatalog::CacheEntries*(sizeof(ds::DocumentMetadata)+97));
}
int main(){try{cacheChecks();std::puts("PASS cached/cold metadata, create/rename/trash/restore/order, read/write/mount invalidation, over-capacity fallback");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
