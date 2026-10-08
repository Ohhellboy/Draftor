#include <map>
#include <string>
#include <cstdio>
struct Desc {static int live; Desc(){++live;} ~Desc(){--live;}};int Desc::live=0;
int main(){std::map<std::string,Desc*> index; Desc* leaked=nullptr; for(int i=0;i<2;++i){auto* d=new Desc; auto r=index.insert(std::pair<std::string,Desc*>("2902",d));if(!r.second)leaked=d;}std::printf("before: live=%d owned=%zu\n",Desc::live,index.size());for(auto& row:index)delete row.second;index.clear();std::printf("after cleanup: live=%d owned=%zu EXPECTED FAILURE\n",Desc::live,index.size());bool reproduced=Desc::live==1;delete leaked;return reproduced?0:1;}
