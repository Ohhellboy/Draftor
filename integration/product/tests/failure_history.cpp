#include "stability_buffer.hpp"
#include <cstdio>
using namespace s3app::stability;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 86;}}while(0)
int main(){
 FailureMailbox mailbox;AllocationFailure f;bool detail=false;
 auto take=[&]{
#ifdef T03_PARENT_MAILBOX
  detail=true;return mailbox.take(f);
#else
  return mailbox.take(f,detail);
#endif
 };
 mailbox.put(64,0x804,11);mailbox.put(4096,0x804,22);
 CHECK(take()&&detail&&f.size==64&&f.count==1&&f.function==11);
 CHECK(take()&&detail&&f.size==4096&&f.count==2&&f.function==22&&f.lost==0);
 CHECK(take()&&!detail&&f.count==2&&f.lost==0);
 // Overflow drops new details, never overwrites the retained first failures.
 for(unsigned i=0;i<6;++i)mailbox.put(i+100,0x804,i+200);
 for(unsigned i=0;i<4;++i)CHECK(take()&&detail&&f.count==i+3&&f.size==i+100&&f.function==i+200);
 CHECK(take()&&!detail&&f.count==8&&f.lost==0); // 8 - 6 = 2 missing details.
 mailbox.put(256,0x804,33);
 CHECK(take()&&detail&&f.count==9&&f.size==256&&f.function==33);
 CHECK(take()&&!detail&&f.count==9);
 std::printf("PASS ordered failure pair, explicit overflow, drain and reuse; mailbox=%zu\n",sizeof(FailureMailbox));
}
