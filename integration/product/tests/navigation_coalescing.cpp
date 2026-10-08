#define main existing_query_rows_main
#include "query_rows.cpp"
#undef main
struct Result {uint64_t hash=0,bytes=0,reads=0,seeks=0,rows=0,us=0;size_t row=0,col=0;};
Result sequence(const std::vector<uint8_t>& keys,bool batch,bool bottom=false){
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();
 if(bottom){for(size_t row=2;row<150;++row){r.ime.window()->request(row,0,true);r.settle();const auto& v=*r.ime.view();if(v.at_end&&v.row==v.rows[0].index+v.count-1)break;}}
 const auto before=metrics.io();const auto rows=r.ime.window()->costs().built_rows;const auto start=clockUs();
 for(auto k:keys){r.tap(k);if(!batch)r.settle();}
 r.settle();const auto after=metrics.io()-before;
 return {digest(*r.ime.view()),after.bytes,after.reads,after.seeks,r.ime.window()->costs().built_rows-rows,clockUs()-start,r.ime.view()->row,r.ime.view()->col};
}
void paced(){
 for(uint64_t interval:{100000u,200000u}){
  Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();
  const std::vector<uint8_t> keys={0x51,0x51,0x51,0x51,0x51,0x51,0x52,0x52,0x52,0x52};
  const auto reference=sequence(keys,false);uint64_t model=0,last=0;size_t next=0,steps=0;
  while(next<keys.size()||r.ime.hasWork()){
   while(next<keys.size()&&model>=next*interval){last=model;r.tap(keys[next++]);}
   const auto before=metrics.io();r.step();const auto d=metrics.io()-before;
   // Explicit sensitivity model, not measured SPI bandwidth: 6ms per read or
   // seek and a 1ms scheduler floor. No sleep or changed resource contents.
   model+=std::max(uint64_t(1000),(d.reads+d.seeks)*6000);
   CHECK(++steps<2000000);
  }
  r.frame=r.ime.framePresented();CHECK(digest(*r.ime.view())==reference.hash);
  std::cout<<"NAV_PACED_MODEL interval_us="<<interval<<" read_or_seek_assumption_us=6000 scheduler_floor_us=1000 last_receive_to_final_prepared_us="<<model-last<<" peak="<<r.ime.arrowPeak()<<" overflow="<<r.ime.arrowOverflow()<<" steps="<<steps<<" final_identity_equal=1 not_device_time=1\n";
 }
}
int main(){try{
 std::vector<std::vector<uint8_t>> sequences;
 sequences.push_back(std::vector<uint8_t>(10,0x51));sequences.push_back(std::vector<uint8_t>(10,0x52));
 auto turn=std::vector<uint8_t>(6,0x51);turn.insert(turn.end(),4,0x52);sequences.push_back(turn);
 sequences.push_back({0x51,0x52,0x51,0x52,0x51,0x52,0x4f,0x50});
 sequences.push_back({0x4f,0x4f,0x4f,0x51,0x51,0x52,0x50,0x50});
 sequences.push_back({0x51,0x51,0x51,0x52});
 for(size_t i=0;i<sequences.size();++i){
  const bool bottom=i==sequences.size()-1;const auto a=sequence(sequences[i],false,bottom),b=sequence(sequences[i],true,bottom);
  std::cout<<"NAV_COMPARE case="<<i<<" reference_rows="<<a.rows<<" batch_rows="<<b.rows<<" reference_bytes="<<a.bytes<<" batch_bytes="<<b.bytes<<" reference_reads="<<a.reads<<" batch_reads="<<b.reads<<" reference_seeks="<<a.seeks<<" batch_seeks="<<b.seeks<<" reference_host_us="<<a.us<<" batch_host_us="<<b.us<<" ref_row="<<a.row<<" row="<<b.row<<" col="<<b.col<<'\n';
  CHECK(a.hash==b.hash);if(i==2)CHECK(b.bytes<a.bytes);
 }
 paced();std::cout<<"PASS final identity/pixel rows/order/short-column and boundary-aware reversals match sequential reference\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
