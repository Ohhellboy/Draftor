#include "stability_buffer.hpp"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace s3app::stability;
int main(){
 BufferControl control;Buffer b(control);Record r;r.kind=4;r.a=66;r.sampled=1;r.internal_free=92;
 for(unsigned i=0;i<Buffer::Capacity;++i){r.ms=i;CHECK(b.push(r));}
 CHECK(!b.push(r));CHECK(b.dropped()==1);
 unsigned writes=0;CHECK(!b.service(true,[&](const char*,unsigned){++writes;return 0u;}));CHECK(writes==0);
 CHECK(b.service(false,[&](const char*,unsigned){++writes;return 0u;}));CHECK(writes==1);
 CHECK(b.service(true,[&](const char*,unsigned){++writes;return 0u;}));CHECK(writes==1);
 std::string bytes;bool chunksBounded=true;
 for(unsigned n=0;n<10000;++n){const bool owns=b.service(false,[&](const char* p,unsigned count){if(count>64)chunksBounded=false;const unsigned take=count>7?7:count;bytes.append(p,take);return int(take);});if(!owns)break;}
 CHECK(chunksBounded);
 CHECK(bytes.find("kind=4 a=66")!=std::string::npos);CHECK(bytes.find("i_free=92")!=std::string::npos);CHECK(bytes.find("dropped=1")!=std::string::npos);
 unsigned lines=0;for(char c:bytes)if(c=='\n')++lines;CHECK(lines==Buffer::Capacity);
 // Preserve the first bounded batch; capacity/contended losses stay explicit.
 FailureMailbox mailbox;std::vector<std::thread> threads;
 for(unsigned t=1;t<=4;++t)threads.emplace_back([&,t]{for(unsigned n=0;n<10000;++n)mailbox.put(t,t+100,t+200);});
 for(auto& t:threads)t.join();AllocationFailure f;bool detail=false;
 for(unsigned i=0;i<FailureMailbox::Capacity;++i){
  CHECK(mailbox.take(f,detail)&&detail);CHECK(f.count==i+1);
  CHECK(f.caps==f.size+100&&f.function==f.size+200);
 }
 CHECK(mailbox.take(f,detail)&&!detail);CHECK(f.count+f.lost==40000);
 std::printf("PASS bounded/blocked/partial UART, explicit loss, coherent allocation mailbox; Buffer=%zu internal_mailbox=%zu\n",sizeof(Buffer),sizeof(FailureMailbox));
}
