#pragma once
// TEST_ONLY. No output in a hot loop, no new queue, no text or candidate payload.
#ifndef DRAFTOR_CP_OBSERVE
#define DRAFTOR_CP_OBSERVE 0
#endif
#if DRAFTOR_CP_OBSERVE
#include "device_metrics.hpp"
#include "idle_log.hpp"
#include <algorithm>
#include <cstdint>
namespace cpattr {
enum Field { Service, Ime, Window, Core, Gap, Draw, Transfer, Delay, Fields };
enum Exit { SliceLimit, WallLimit, Keys, Publication, NoWork, Exits };
struct Stage {
 uint64_t us[Fields]{};
 uint32_t calls=0,windows=0,draws=0,sent=0,reused=0,exits[Exits]{},yield0=0,yield1=0;
};
struct State {
 Stage phase[3]{};
 uint64_t begin=0,range=0,ready=0,done=0,last_end=0,input=0,revision=0,scheme=0,mount=0,bundle=0;
 uint32_t action=0,query=0,frame=0,overwritten=0,dropped=0;
 bool active=false;
};
inline State state;
inline uint64_t window_start=0;
inline uint32_t increment(uint32_t n){return n==UINT32_MAX?n:n+1;}
inline uint64_t now(){return ime_device::clockUs();}
inline unsigned phase(){return state.ready?2:state.range?1:0;}
inline void interval(Field f,uint64_t from,uint64_t to){
 if(!state.active||to<from)return;
 from=std::max(from,state.begin);
 const uint64_t cuts[]={state.range?state.range:UINT64_MAX,state.ready?state.ready:UINT64_MAX,UINT64_MAX};
 for(unsigned p=0;p<3&&from<to;++p){const auto end=std::min(to,cuts[p]);if(end>from){state.phase[p].us[f]+=end-from;from=end;}}
}
struct Timer {
 Field field;uint64_t start;bool open=true;
 explicit Timer(Field f,uint64_t t=now()):field(f),start(t){}
 void stop(uint64_t t){if(open){interval(field,start,t);open=false;}}
 ~Timer(){if(open)stop(now());}
};
inline void begin(uint32_t a,uint32_t q,uint64_t i,uint64_t v,uint64_t se,uint64_t m,uint64_t b){
 const auto overwritten=state.active?increment(state.overwritten):state.overwritten,dropped=state.dropped;
 state={};state.overwritten=overwritten;state.dropped=dropped;state.active=true;
 state.action=a;state.query=q;state.input=i;state.revision=v;state.scheme=se;state.mount=m;state.bundle=b;
 state.begin=state.last_end=now();
}
inline void range(){if(state.active&&!state.range)state.range=now();}
inline void ready(){if(state.active&&!state.ready)state.ready=now();}
inline void cancel(){if(state.active){state.overwritten=increment(state.overwritten);state.active=false;}}
inline void windowBegin(){window_start=now();if(state.active){interval(Gap,state.last_end,window_start);++state.phase[phase()].windows;}}
inline void windowEnd(){if(state.active){const auto t=now();interval(Service,window_start,t);state.last_end=t;}}
inline void call(){if(state.active)++state.phase[phase()].calls;}
// The original short-circuit order. A false condition records exactly one exit.
template<class Elapsed,class Pending,class Publish,class Work>
bool again(unsigned slices,unsigned limit,uint32_t quantum,Elapsed elapsed,Pending pending,Publish publish,Work work){
 Exit why=NoWork;
 if(slices>=limit)why=SliceLimit;
 else if(elapsed()>=quantum)why=WallLimit;
 else if(pending())why=Keys;
 else if(publish())why=Publication;
 else if(work())return true;
 if(state.active)++state.phase[phase()].exits[why];
 return false;
}
inline void drawStart(){if(state.active)++state.phase[phase()].draws;}
inline void presented(uint64_t transfer,uint64_t done,bool sent){
 if(state.active){interval(Transfer,transfer,done);auto& p=state.phase[phase()];if(sent)++p.sent;else ++p.reused;}
}
inline void yielded(unsigned ms,uint64_t start,uint64_t end){
 if(state.active){auto& p=state.phase[phase()];if(ms)++p.yield1;else ++p.yield0;interval(Delay,start,end);}
}
inline void finish(uint64_t done,uint32_t frame,uint32_t action,uint32_t query){
 if(state.active&&(action!=state.action||query!=state.query)){cancel();return;}
 if(state.active&&state.ready&&!state.done){state.done=done;state.frame=frame;interval(Gap,state.last_end,done);}
}
// Six bounded lines use the existing 16-line IdleLog. Capacity shortage is explicit,
// never an extra queue or a partial six-line report. Emit AFTER software LCD time.
inline void emit(ime_device::IdleLog& log){
 if(!state.active||!state.done)return;
 if(ime_device::IdleLog::Capacity-log.size()<6){state.dropped=increment(state.dropped);state.active=false;return;}
 using U=unsigned long long;
 for(unsigned i=0;i<3;++i){const auto& p=state.phase[i];
  const bool a=log.printf("[CP1_TIME] a=%u q=%u ph=%u i=%llu v=%llu se=%llu m=%llu b=%llu frame=%u t0=%llu range=%llu ready=%llu done=%llu svc=%llu ime=%llu win=%llu core=%llu gap=%llu draw=%llu tx=%llu delay=%llu\n",
   unsigned(state.action),unsigned(state.query),i,U(state.input),U(state.revision),U(state.scheme),U(state.mount),U(state.bundle),unsigned(state.frame),U(state.begin),U(state.range),U(state.ready),U(state.done),U(p.us[Service]),U(p.us[Ime]),U(p.us[Window]),U(p.us[Core]),U(p.us[Gap]),U(p.us[Draw]),U(p.us[Transfer]),U(p.us[Delay]));
  const bool b=log.printf("[CP1_COUNT] a=%u q=%u ph=%u calls=%u windows=%u draws=%u sent=%u reused=%u limit=%u wall=%u keys=%u pub=%u no_work=%u y0=%u y1=%u overwritten=%u dropped=%u\n",
   unsigned(state.action),unsigned(state.query),i,unsigned(p.calls),unsigned(p.windows),unsigned(p.draws),unsigned(p.sent),unsigned(p.reused),unsigned(p.exits[SliceLimit]),unsigned(p.exits[WallLimit]),unsigned(p.exits[Keys]),unsigned(p.exits[Publication]),unsigned(p.exits[NoWork]),unsigned(p.yield0),unsigned(p.yield1),unsigned(state.overwritten),unsigned(state.dropped));
  if(!a||!b)state.dropped=increment(state.dropped);
 }
 state.active=false;
}
} // namespace cpattr
#define CPATTR(...) do { __VA_ARGS__; } while(0)
#define CPATTR_TIMER(name, field) cpattr::Timer name(cpattr::field)
#else
#define CPATTR(...) do {} while(0)
#define CPATTR_TIMER(name, field) do {} while(0)
#endif
