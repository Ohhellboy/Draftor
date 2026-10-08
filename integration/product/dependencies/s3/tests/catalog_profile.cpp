#define main inherited_main
#include "host_tests.cpp"
#undef main
void sample(Fixture& f,unsigned normal,unsigned trash,const char* label,void(*op)(Fixture&)){
 if(s3profile::active) { (void)s3profile::finish(); }
 s3profile::begin(label);auto begin=s3profile::clockUs();op(f);f.settle();auto block=s3profile::clockUs()-begin;
 {s3profile::Scope draw(s3profile::Draw);s3app::display::documentMenu(f.app,f.smallFont,[&](const char* t,int,int,bool){for(;*t;++t){(void)f.font.glyph(static_cast<unsigned char>(*t));}},[](int,int,int,int){});}
 auto r=s3profile::finish();std::printf("CSV,%u,%u,%s,%llu,%llu,%llu,%u,%u,%u,%llu,%zu\n",normal,trash,label,(unsigned long long)r.total_us,(unsigned long long)r.us[s3profile::Access],(unsigned long long)r.us[s3profile::Draw],unsigned(r.reads),unsigned(r.calls[s3profile::Opendir].count),unsigned(r.read_bytes),(unsigned long long)block,sizeof(s3app::App));
}
int main(){try{
 std::puts("CSV,normal,trash,operation,total_us,access_us,host_draw_us,reads,opendir,read_bytes,host_block_us,sizeof_App");
 for(unsigned count:{0U,1U,18U,50U,130U}){Fixture f;f.start();std::vector<ds::DocumentId> trash;trash.push_back(f.app.session.currentId());CHECK(f.app.session.create("Existing reserved",++f.now));trash.push_back(f.app.session.currentId());for(unsigned i=0;i<count;++i)CHECK(f.app.session.create({},++f.now));for(auto id:trash)CHECK(f.app.session.moveToTrash(id,++f.now));
 f.app.catalog.invalidateCache();
 sample(f,count,2,"files_first",[](Fixture& f){f.key(59);});
 sample(f,count,2,"files_repeat",[](Fixture& f){f.key(18,1);});
 sample(f,count,2,"normal_next",[](Fixture& f){f.key(79);});
 sample(f,count,2,"normal_previous",[](Fixture& f){f.key(80);});
 sample(f,count,2,"trash",[](Fixture& f){f.key(23);});
 sample(f,count,2,"trash_repeat",[](Fixture& f){f.key(23);f.key(23);});
 sample(f,count,2,"restore",[](Fixture& f){f.key(40);});
 f.key(23);
 sample(f,count,1,"new",[](Fixture& f){f.key(17);});
 f.key(59);
 sample(f,count+2,1,"rename_rejected",[](Fixture& f){renameTo(f,"Existing reserved");f.key(41);});
 f.key(18,1);
 sample(f,count+2,1,"trash_selected",[](Fixture& f){f.key(7);f.key(40);});
 }
 return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
