from pathlib import Path
import subprocess,os,json,sys
root=Path(__file__).resolve().parents[2]
out=root/'TestBuilds/epoch';out.mkdir(parents=True,exist_ok=True)
s=(root/'product/platform/ble_keyboard_host.cpp').read_text()
def fn(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
source=r'''
#include "hid_keyboard.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <cassert>
#include <cstdio>
std::mutex stateMux;
#define portENTER_CRITICAL(p) (p)->lock()
#define portEXIT_CRITICAL(p) (p)->unlock()
std::atomic<uint32_t> epoch{1};
std::atomic<bool> accepting{true};
s3hid::Tracker tracker;
bool requireRelease=false;
'''+fn('void invalidate()')+'\n'+fn('bool admit(')+r'''
void reset(){epoch=1;accepting=true;tracker.clear();requireRelease=false;}
int main(){
 s3hid::Event event{};event.epoch=1;
 reset();const uint32_t batchEpoch=epoch.load();invalidate();
 assert(event.epoch==batchEpoch); // exact original stale epoch decision
 uint32_t seen=0;assert(!admit(event,seen)&&seen==2);
 std::puts("PASS original batch snapshot accepts old epoch; actual new admit rejects it");
 reset();bool admitted=admit(event,seen);invalidate();assert(admitted&&seen==1&&epoch==2);
 std::puts("PASS admit-before-invalidate retains raw admission; App command acceptance remains separate");
 reset();accepting=false;assert(!admit(event,seen)&&seen==1);
 std::puts("PASS early accepting=false before epoch bump prevents new admission");
 reset();invalidate();accepting=true;event.epoch=2;assert(admit(event,seen)&&seen==2);
 event.epoch=1;assert(!admit(event,seen));
 std::puts("PASS new connection epoch accepted; queued old epoch rejected");
 for(unsigned i=0;i<100;++i){
  reset();std::mutex gate;std::condition_variable cv;bool go=false,invalidated=false;
  std::thread host([&]{std::unique_lock<std::mutex> l(gate);cv.wait(l,[&]{return go;});l.unlock();invalidate();l.lock();invalidated=true;l.unlock();cv.notify_one();});
  {std::unique_lock<std::mutex> l(gate);go=true;cv.notify_one();cv.wait(l,[&]{return invalidated;});}
  assert(!admit(event,seen)&&seen==2);host.join();
 }
 std::puts("PASS 100 controlled cross-thread invalidate-before-admit schedules");
 std::puts("LIMIT: Host mutex adapts port critical sections; no FreeRTOS/MCU or full deviceLoop execution claimed");
}
'''
(out/'epoch.cpp').write_text(source)
env=os.environ.copy();env.update(TMPDIR=str(root/'TestBuilds/.tmp'),TMP=str(root/'TestBuilds/.tmp'),TEMP=str(root/'TestBuilds/.tmp'))
res=[]
modes=[False,True]
if '--mode' in sys.argv:modes=[sys.argv[sys.argv.index('--mode')+1]=='sanitize']
for san in modes:
 exe=out/('epoch_san' if san else 'epoch_normal')
 cmd=['clang++','-std=c++17','-O1','-g','-pthread','-I'+str(root/'product/platform'),'-I'+str(root/'product/dependencies/common'),str(out/'epoch.cpp'),str(root/'product/platform/hid_keyboard.cpp'),str(root/'product/dependencies/common/body_key_repeat.cpp'),'-o',str(exe)]
 if san:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 r=subprocess.run(cmd,cwd=root,env=env,text=True,capture_output=True);(root/'evidence'/('build_'+exe.name+'.log')).write_text(r.stdout+r.stderr);res.append({'command':cmd,'exit':r.returncode});assert r.returncode==0,r.stderr
 r=subprocess.run([str(exe)],cwd=root,env=env,text=True,capture_output=True);(root/'evidence'/('run_'+exe.name+'.log')).write_text(r.stdout+r.stderr);res.append({'command':[str(exe)],'exit':r.returncode});print(r.stdout,r.stderr);assert r.returncode==0
(root/('evidence/EPOCH_TEST_RESULTS'+('_'+sys.argv[sys.argv.index('--mode')+1] if '--mode' in sys.argv else '')+'.json')).write_text(json.dumps(res,indent=2)+'\n')
