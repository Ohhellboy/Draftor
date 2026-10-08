// Synthetic codes, actual HID decoder/Tracker FIFO/InputRoute/DeviceIme/window.
// Host FIFO models the existing N1 rig; BLE locks and physical LCD are excluded.
#define main lc1_previous_rows_main
#include "query_rows.cpp"
#undef main
#include "input_route.hpp"
#include <deque>
#include <fstream>
inline size_t ic1_trace_sequence=0; // Host replay logs: keep every same-name case.
struct LongRig:Rig {
 s3hid::Map map; s3hid::Tracker tracker; s3hid::InputRoute route;
 std::deque<s3hid::Event> fifo; std::vector<UiTraceRecord> records;
 uint32_t sequence=0; size_t accepted=0; std::ofstream log;
 LongRig(const std::string& name,hanime_extension::StorageBackend backend={}):Rig(backend),log(name+"_"+std::to_string(++ic1_trace_sequence)+".trace.log") {
  const uint8_t descriptor[]={0x05,1,0x09,6,0xa1,1,0x05,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,6,0x75,8,0x15,0,0x25,0x65,0x05,7,0x19,0,0x29,0x65,0x81,0,0xc0};
  CHECK(map.parse(descriptor,sizeof descriptor)); route.framePresented(app.focusToken());
  UiTraceRecord old;while(uiTrace.pop(old)){}uiTrace.setMode(1);
 }
 void drain(){UiTraceRecord r;while(uiTrace.pop(r)){
  records.push_back(r);if(r.kind==UiTraceKind::BodyAccepted)++accepted;
  for(unsigned part=0;part<UiTraceParts;++part){char line[384];auto n=formatUiTrace(r,part,line,sizeof line);log.write(line,n);}
 }}
 void enqueue(uint8_t key,bool down,uint8_t mods=0){
  uint8_t bytes[8]={mods,0,uint8_t(down?key:0),0,0,0,0,0};s3hid::State state;
  CHECK(s3hid::Map::decode(*map.report(0),bytes,sizeof bytes,state));
  s3hid::IngressContext context{route.epoch(),frame,ime.attemptSerial(),ime.ingressContext(),ime.resourceOwner()};
  s3hid::dispatchReport(tracker,0,state,1,++now,context,[&](s3hid::Event e){e.event_id=++sequence;e.received_us=clockUs();CHECK(fifo.size()<512);fifo.push_back(e);});
 }
 void dispatch(){while(!fifo.empty()){auto e=fifo.front();fifo.pop_front();CHECK(route.accept(e,now));if(!ime.event(e))app.key(e);drain();}}
 void key(uint8_t k,uint8_t mods=0){enqueue(k,true,mods);enqueue(k,false);dispatch();}
 void letters(const std::string& raw){for(char c:raw)key(uint8_t(c-'a'+4));}
 void ready(){unsigned n=0;do{step();drain();CHECK(++n<2000000);}while(!resources.ready()||!ime.composition()||ime.hasWork());frame=ime.framePresented();traceUi(UiTraceKind::Lcd,ime.composition(),ime.window());drain();}
 void start(bool py,const std::string& raw){key(0x39);ready();if(py){key(44,1);ready();}letters(raw);ready();}
};
bool expect_capacity_diagnostic=false;
void shape(unsigned long_parts,unsigned short_parts,const char* label,bool baseline,uint8_t key=44,bool early=false){
 std::vector<std::string> parts(long_parts,"wjdj");for(unsigned i=0;i<short_parts;++i)parts.push_back("wo");
 CHECK(parts.size()==16);const std::string tail=long_parts==15?"wo":"wjdj";
 std::string raw;for(auto& p:parts)raw+=p;const size_t prefix=raw.size();raw+=tail;
 CHECK((raw.size()==64&&prefix==62)||(raw.size()==62&&prefix==58));
 LongRig r(label);r.start(false,raw);std::string expected;size_t end=0;
 for(size_t i=0;i<parts.size();++i){
  end+=parts[i].size();r.ime.window()->query(end);r.drain();r.ready();
  CHECK(r.ime.window()->state()==WindowState::Ready);auto item=r.ime.view()->rows[0].items[0];
  CHECK(item.begin==end-parts[i].size()&&item.end==end);expected.append(item.text.data(),item.bytes);
  r.key(key);r.ready();auto s=r.ime.composition()->snapshot();CHECK(s.member_count==i+1&&s.focus==end);
  CHECK(s.members[i].key==item.key&&s.members[i].ref==item.ref&&s.members[i].begin==item.begin&&s.members[i].end==item.end);
  CHECK(r.app.document.text().empty()&&r.accepted==0);
 }
 r.ime.window()->query(raw.size());r.ready();CHECK(r.ime.window()->state()==WindowState::Ready);
 auto before=r.ime.composition()->snapshot();const auto revision=before.revision;std::string selected(before.selected_text);
 const std::vector<hanime_extension::Member> old_members(before.members,before.members+before.member_count);
 const auto old_view=digest(*r.ime.view());
 const auto item=r.ime.view()->rows[0].items[0];CHECK(item.begin==prefix&&item.end==raw.size());expected.append(item.text.data(),item.bytes);
 if(early){r.ime.window()->query(raw.size());r.frame=0;}
 const auto trace_begin=r.records.size();r.key(key);r.ready();auto after=r.ime.composition()->snapshot();
 std::cout<<"CASE "<<label<<" raw="<<raw.size()<<" focus_before="<<prefix<<" members_before=16 state="<<int(r.ime.window()->state())<<" members_after="<<after.member_count<<" focus_after="<<after.focus<<" accepted="<<r.accepted<<" notice="<<r.ime.notice()<<'\n';
 bool captured=false,no_candidate=false;size_t capacity_terminals=0;for(size_t i=trace_begin;i<r.records.size();++i){auto& e=r.records[i];if(e.kind==UiTraceKind::Choice)captured=true;if(e.kind==UiTraceKind::Intent&&e.detail==NoCandidate)no_candidate=true;
#ifdef LC1_DIAGNOSTIC_TEST
if(e.kind==UiTraceKind::Intent&&e.detail==SelectionCapacity)++capacity_terminals;
#endif
}
 CHECK(early||captured);
 if(baseline){CHECK((expect_capacity_diagnostic?(!no_candidate&&capacity_terminals==1):no_candidate)&&after.revision==revision&&after.raw==raw&&after.selected_text==selected&&after.member_count==16&&after.focus==prefix&&r.accepted==0&&r.app.document.text().empty());
 CHECK(digest(*r.ime.view())==old_view);
 for(size_t i=0;i<old_members.size();++i){const auto& a=old_members[i];const auto& z=after.members[i];
  CHECK(a.key==z.key&&a.ref==z.ref&&a.begin==z.begin&&a.end==z.end&&a.valid==z.valid&&
        a.text_begin==z.text_begin&&a.text_size==z.text_size&&a.form_begin==z.form_begin&&
        a.form_size==z.form_size&&a.invocation==z.invocation);
 }
}
 else CHECK(r.ime.raw().empty()&&std::string_view(r.app.document.text())==expected&&r.accepted==1&&!no_candidate);
}

void maximum(bool py,uint8_t key){
 const size_t count=py?64:32;std::string raw;for(size_t i=0;i<count;++i)raw+=py?"a":"wo";
 CHECK(raw.size()==64);LongRig r(py?"maximum_py":"maximum_xh");r.start(py,raw);std::string expected;
 for(size_t i=0;i<count;++i){
  const size_t end=(i+1)*(py?1:2);r.ime.window()->query(end);r.ready();CHECK(r.ime.window()->state()==WindowState::Ready);
  const auto item=r.ime.view()->rows[0].items[0];CHECK(item.begin==i*(py?1:2)&&item.end==end);expected.append(item.text.data(),item.bytes);
  r.key(key);r.ready();
  if(i+1<count){const auto s=r.ime.composition()->snapshot();CHECK(s.member_count==i+1&&s.focus==end&&r.accepted==0);CHECK(s.members[i].key==item.key&&s.members[i].ref==item.ref);}
 }
 CHECK(r.accepted==1&&r.ime.raw().empty()&&std::string_view(r.app.document.text())==expected);
 std::cout<<"CASE maximum py="<<py<<" members="<<count<<" raw=64 accepted=1 text_bytes="<<expected.size()<<'\n';
}

#ifndef LC1_REPLAY_LIBRARY
int main(int argc,char** argv){try{expect_capacity_diagnostic=argc>1&&std::string(argv[1])=="diagnostic";bool baseline=expect_capacity_diagnostic||(argc>1&&std::string(argv[1])=="baseline");shape(15,1,"tail2",baseline);shape(13,3,"tail4",baseline,30);if(expect_capacity_diagnostic)shape(13,3,"tail4_early_number",baseline,30,true);if(!baseline){maximum(false,30);maximum(true,44);}std::cout<<"LC1_REPLAY shapes=2 maxima="<<(!baseline?2:0)<<" baseline_failure_observed="<<baseline<<'\n';return baseline&&!expect_capacity_diagnostic?1:0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}}

#endif
