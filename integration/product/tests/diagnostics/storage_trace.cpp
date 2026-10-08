// Exact production reader trace; run outside the source tree, never on device.
#define main historical_query_rows_main
#include "../query_rows.cpp"
#undef main
#include <map>
namespace {
std::string phase;
std::map<int,std::string> paths;
void observe(int fd,const char* path,size_t offset,size_t bytes){
 if(path){paths[fd]=fs::path(path).filename().string();return;}
 if(offset==SIZE_MAX){paths.erase(fd);return;}
 std::cout<<"ACCESS phase="<<phase<<" file="<<paths.at(fd)<<" offset="<<offset<<" bytes="<<bytes<<'\n';
}
}
int main(){try{
 TfReader::readObserver=observe;
 Rig r;phase="bootstrap";r.tap(0x39);r.settle();
 for(const char* raw:{"ui","wjdjle","wojbtm"}){
  phase=std::string(raw)+"_first";r.type(raw);r.settle();
  if(std::string(raw)=="ui"){
   for(unsigned row:{1u,2u,3u,4u,5u,6u,7u,8u,7u,6u,5u,4u,3u,2u,1u,0u,1u,2u,3u,4u,24u,25u,24u,0u}){
    phase="ui_row_"+std::to_string(row);const auto before=metrics.io();
    r.ime.window()->request(row,0,true);r.settle();
    const auto d=metrics.io()-before;
    std::cout<<"PHASE name="<<phase<<" reads="<<d.reads<<" bytes="<<d.bytes<<" seeks="<<d.seeks<<" hash="<<digest(*r.ime.view())<<'\n';
   }
  }
  r.tap(0x29);r.settle();
 }
 std::cout<<"PASS real production storage trace\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
