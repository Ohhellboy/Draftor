#!/usr/bin/env python3
"""Actual periodic/record and bounded Buffer; deterministic heap/clock seams."""
from pathlib import Path
import argparse,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--mode',choices=['normal','sanitize'],required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'TestBuilds/memory_periodic'/a.mode;out.mkdir(parents=True,exist_ok=True)
ev=r/'evidence/memory_periodic'/a.mode;ev.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(out),TMP=str(out),TEMP=str(out),ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
source=r/'product/platform/stability_diagnostics.cpp';s=source.read_text();ranges=[]
def fn(sig):
 at=s.index(sig);end=s.index('{',at)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 ranges.append(dict(signature=sig,start=s.count('\n',0,at)+1,end=s.count('\n',0,end)+1,body_verbatim=True))
 return s[at:end]
code=r'''#include "stability_diagnostics.hpp"
#include "stability_buffer.hpp"
#include <cassert>
#include <string>
#include <vector>
static uint32_t now=0,stats=0;static std::vector<unsigned> capsSeen;
uint64_t esp_timer_get_time(){return uint64_t(now)*1000;}
unsigned heap_caps_get_free_size(unsigned caps){++stats;capsSeen.push_back(caps);return caps==0x804?321:654;}
unsigned heap_caps_get_largest_free_block(unsigned caps){++stats;capsSeen.push_back(caps);return caps==0x804?123:456;}
unsigned heap_caps_get_minimum_free_size(unsigned caps){++stats;capsSeen.push_back(caps);return caps==0x804?111:222;}
namespace s3app::stability {
constexpr unsigned Internal=0x804,External=0x404;
BufferControl control;Buffer actual(control);Buffer* buffer=&actual;
std::uint32_t lastSampleMs=0,sampleSequence=0;
'''+fn('Record record(')+'\n'+fn('void periodic(')+r'''
}
using namespace s3app::stability;
static void sample(uint32_t time,uint32_t phase=9){now=time;periodic(time,phase);}
static std::string drain(){std::string log;while(actual.service(false,[&](const char*p,unsigned n){assert(n<=64);log.append(p,n);return n;})){}return log;}
int main(){
 sample(1999);assert(stats==0);sample(2000);assert(stats==6);
 auto text=drain();assert(text.find("kind=13 a=1 b=9 c=0 sampled=1 i_free=321 i_largest=123 i_min=111 p_free=654 p_largest=456 p_min=222 loop_hwm_bytes=0 ble_worker_hwm_bytes=0 integrity=0 dropped=0")!=std::string::npos);
 assert((capsSeen==std::vector<unsigned>{0x804,0x804,0x804,0x404,0x404,0x404}));
 sample(3999);assert(stats==6);sample(10000);assert(stats==12);sample(10000);assert(stats==12);
 text=drain();assert(text.find("a=2 b=9 c=3")!=std::string::npos); // one sample, no catch-up burst
 lastSampleMs=0xfffffff0;sample(uint32_t(0xfffffff0+1999));assert(stats==12);sample(uint32_t(0xfffffff0+2000));assert(stats==18);drain();
 for(unsigned i=1;i<=17;++i)sample(lastSampleMs+2000);
 assert(actual.dropped()==1&&stats==120);text=drain();assert(text.find("dropped=1")!=std::string::npos);
 sample(lastSampleMs+2000);text=drain();assert(text.find("a=21 ")!=std::string::npos); // omitted sequence identifies dropped sample
 // Failed boot buffer still admits no allocations, and rate limiting is retained.
 buffer=nullptr;sample(lastSampleMs+2000);assert(stats==132);
 puts("MEMORY_PERIODIC_PASS six same-caps stats, no stack/integrity symbols, 2s/wrap/no-catchup/loss/absent-buffer");
}
'''
cpp=out/'probe.cpp';cpp.write_text(code);exe=out/'probe';flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.mode=='sanitize' else []
cmd=['clang++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(r/'product/platform'),str(cpp),'-o',str(exe),*flags];runs=[]
for label,argv in [('build',cmd),('run',[str(exe)])]:
 q=subprocess.run(argv,cwd=out,env=env,capture_output=True,text=True,timeout=60);log=q.stdout+q.stderr;(ev/(label+'.log')).write_text(log);runs.append(dict(argv=argv,exit=q.returncode))
 (ev/'RESULTS.json').write_text(json.dumps(dict(source=str(source),ranges=ranges,runs=runs,device='NOT_RUN'),indent=2)+'\n')
 assert q.returncode==0 and not any(x in log for x in ['AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer']),log
print('PASS',a.mode,'actual periodic + bounded buffer; full stop gate separately tested')
