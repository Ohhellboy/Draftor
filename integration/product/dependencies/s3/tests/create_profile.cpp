#define main inherited_main
#include "host_tests.cpp"
#undef main
int main(){try{
 std::puts("CSV,count,state,host_total_us,max_main_step_us,steps,open,stat,fsync,opendir,read_bytes");
 for(unsigned count:{14U,18U,20U,26U})for(bool dirty:{false,true}){
  Fixture f;f.start();for(unsigned i=1;i<count;++i)CHECK(f.app.session.create({},++f.now));
  f.key(59);f.key(41);if(dirty)f.key(4);
  if(s3profile::active) { (void)s3profile::finish(); }
  s3profile::begin("new");
  auto before=s3profile::clockUs();f.key(17,1);uint64_t longest=s3profile::clockUs()-before;unsigned steps=1;
  while(f.app.commandBarrier()||f.app.persistence.backgroundSaveActive()){
   auto start=s3profile::clockUs();f.app.service(++f.now,false);longest=std::max(longest,s3profile::clockUs()-start);CHECK(++steps<10000);
  }
  auto r=s3profile::finish();CHECK(f.app.lastOperation&&f.app.document.text().empty());
  std::printf("CSV,%u,%s,%llu,%llu,%u,%u,%u,%u,%u,%u\n",count,dirty?"dirty_old":"clean_old",(unsigned long long)r.total_us,(unsigned long long)longest,steps,unsigned(r.calls[s3profile::Open].count),unsigned(r.calls[s3profile::Stat].count),unsigned(r.calls[s3profile::Fsync].count),unsigned(r.calls[s3profile::Opendir].count),unsigned(r.read_bytes));
 }
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
