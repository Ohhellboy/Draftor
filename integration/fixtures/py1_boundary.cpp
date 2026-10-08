#define LC1_REPLAY_LIBRARY
#include "lc1_replay.cpp"
#include <set>
int main(){try{
 for(const auto& raw:{"fangeng","lingeng","mingeng","tangeng","geng"}){
  LongRig r(std::string("py1_")+raw);r.start(true,raw);std::set<size_t> positions{r.ime.rawCursor()};
  for(unsigned i=0;i<10;++i){r.key(0x50);positions.insert(r.ime.rawCursor());if(r.ime.rawCursor()==0)break;}
  for(unsigned i=0;i<10;++i){r.key(0x4f);positions.insert(r.ime.rawCursor());if(r.ime.rawCursor()==std::string(raw).size())break;}
  std::cout<<"PY1 raw="<<raw<<" member_count="<<r.ime.composition()->snapshot().member_count<<" cursor_left_then_right=";
  for(auto pos:positions)std::cout<<pos<<',';
  const auto target=r.ime.editTarget();std::cout<<" displayed_target="<<target.begin<<':'<<target.end;
  const auto end=std::string(raw)=="geng"?4:3;
  const auto status=r.ime.window()->tryPrefix(end);r.ready();
  std::cout<<" public_prefix="<<end<<" admission="<<int(status)<<" window="<<int(r.ime.window()->state());
  if(r.ime.window()->state()==WindowState::Ready){const auto item=r.ime.view()->rows[0].items[0];std::cout<<" candidate_span="<<item.begin<<':'<<item.end;}
  std::cout<<" ui_endpoint_reachable="<<bool(positions.count(end))<<" synthetic=1\n";
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
