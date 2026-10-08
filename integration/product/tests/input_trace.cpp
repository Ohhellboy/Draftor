#define main existing_query_rows_main
#include "query_rows.cpp"
#undef main
#include "ui_trace.hpp"
#include <fstream>
struct TracePort:LogPort{
 std::string text;size_t max_write=0;
 bool connected()const override{return true;}size_t available()override{return 128;}
 size_t write(const char* s,size_t n)override{text.append(s,n);max_write=std::max(max_write,n);return n;}
};
void modeRun(uint8_t mode){
 Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();
 UiTraceRecord discard;while(uiTrace.pop(discard)){}uiTrace.setMode(mode);
 const auto lost=uiTrace.lost(),textLost=uiTrace.textLost();IdleLog output;TracePort port;UiTraceRecord record;unsigned part=UiTraceParts;uint64_t loggingUs=0;uint32_t event=0;
 port.text="[BOOT] HOST_CONTROLLED_TRACE\n[TRACE_SCHEMA] version=2 mode="+std::to_string(mode)+"\n# task_label=reproduction\n";
 auto drain=[&](){const auto start=clockUs();unsigned spins=0;while(output.size()||uiTrace.size()||part<UiTraceParts){drainUiTracePart(uiTrace,record,part,output);output.service(port,{});CHECK(++spins<100000);}loggingUs+=clockUs()-start;};
 auto key=[&](uint8_t key){for(bool down:{true,false}){s3hid::Event e{1,++r.now,key,0,down,0,r.frame,r.ime.attemptSerial(),++event,clockUs()};e.ime_context=r.ime.ingressContext();e.resource_owner=r.ime.resourceOwner();r.ime.event(e);drain();}};
 const auto before=metrics.io();const auto start=clockUs();
 for(unsigned group=0;group<2;++group){key(24);key(12);key(0x2c);} // ui Space twice, all before query advances.
 for(unsigned step=0;r.ime.hasWork()&&step<2000000;++step){r.step();drain();}
 CHECK(r.app.document.text()=="是是"&&!r.ime.hasWork());
 key(24);key(12);key(0x50);key(0x28);key(0x29); // explicit raw Enter branch plus cancel/edit context
 r.settle();drain();key(0x29);r.settle();drain();
 const auto io=metrics.io()-before;
 CHECK(uiTrace.lost()==lost&&uiTrace.textLost()==textLost&&output.costs().dropped==0);
 CHECK(port.max_write<=128);
 CHECK(port.text.find("key=24 ")==std::string::npos&&port.text.find("key=12 ")==std::string::npos);
 if(mode==2)CHECK(port.text.find("[UI_TEXT]")!=std::string::npos);
 else CHECK(port.text.find("[UI_TEXT]")==std::string::npos);
 if(mode){CHECK(port.text.find("unassigned_raw_enter")!=std::string::npos);CHECK(port.text.find("pending_letters=2")!=std::string::npos);}
 std::ofstream file("input_trace_mode"+std::to_string(mode)+".log");file<<port.text;
 std::cout<<"TRACE_CONTROL mode="<<unsigned(mode)<<" host_total_us="<<clockUs()-start<<" drain_format_host_us="<<loggingUs<<" bytes="<<port.text.size()<<" io_bytes="<<io.bytes<<" io_reads="<<io.reads<<" io_seeks="<<io.seeks<<" record_bytes="<<sizeof(UiTraceRecord)<<" record_capacity="<<UiTrace::Capacity<<" device_ime_bytes="<<sizeof(DeviceIme)<<" device_window_bytes="<<sizeof(CandidateWindow)<<" trace_peak="<<uiTrace.peak()<<" dropped_delta="<<uiTrace.lost()-lost<<" text_lost_delta="<<uiTrace.textLost()-textLost<<" model=drain_after_event_or_work_slice_not_device_bandwidth\n";
}
int main(){try{for(uint8_t m:{0,1,2})modeRun(m);uiTrace.setMode(1);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
