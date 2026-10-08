#pragma once
#include <cstdint>
#include <cstring>
namespace keyboard {
// One connection attempt per client; retirement remains owned by the caller.
struct RetryPlan {
 bool scanNext=false;
 bool scan(bool known){bool result=!known||scanNext;scanNext=!result;return result;}
 void ready(){scanNext=false;}
};
inline bool sameIdentity(uint8_t aType,const uint8_t* a,uint8_t bType,const uint8_t* b){
 // NimBLE reports resolved identity types 2/3; stored identities use 0/1.
 return (aType&1)==(bType&1)&&std::memcmp(a,b,6)==0;
}
inline bool pairingDurable(bool identityWritten,bool storedKey,unsigned failures){
 return identityWritten&&storedKey&&failures==0;
}
}
