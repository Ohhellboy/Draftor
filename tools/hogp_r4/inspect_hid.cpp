#include "hid_keyboard.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv){
 if(argc!=4)return 2;std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> map((std::istreambuf_iterator<char>(input)),{});
 s3hid::Map m;if(!m.parse(map.data(),map.size()))return 3;const auto* report=m.report(uint8_t(std::strtoul(argv[2],nullptr,10)));if(!report)return 4;
 std::string hex=argv[3];if(hex.size()%2)return 5;std::vector<uint8_t> data;for(size_t i=0;i<hex.size();i+=2){char* end=nullptr;auto x=std::strtoul(hex.substr(i,2).c_str(),&end,16);data.push_back(uint8_t(x));}
 s3hid::State state;if(!s3hid::Map::decode(*report,data.data(),data.size(),state))return 6;
 std::printf("{\"bits\":%u,\"mods\":%u,\"keys\":[",report->bits,state.mods);bool first=true;
 for(unsigned u=4;u<224;++u)if(state.has(u)){std::printf("%s%u",first?"":",",u);first=false;}std::puts("]}");
}
