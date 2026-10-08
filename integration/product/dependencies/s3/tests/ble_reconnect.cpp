#include "ble_reconnect_policy.hpp"
#include <cassert>
#include <cstdio>
int main(){
 keyboard::RetryPlan plan;
 for(unsigned i=0;i<50;++i){assert(!plan.scan(true));assert(plan.scan(true));}
 // Keyboard arrives after any number of failed known/scan pairs, still gets a direct attempt.
 assert(!plan.scan(true));plan.ready();assert(!plan.scan(true));
 assert(plan.scan(false)&&plan.scan(false));
 const uint8_t identity[6]={1,2,3,4,5,6},other[6]={1,2,3,4,5,7};
 assert(keyboard::sameIdentity(1,identity,3,identity)); // resolved identity, no advertised name required
 assert(!keyboard::sameIdentity(0,identity,1,identity));
 assert(!keyboard::sameIdentity(1,identity,1,other));
 assert(!keyboard::pairingDurable(true,true,1u<<1));
 assert(!keyboard::pairingDurable(false,true,0));
 assert(!keyboard::pairingDurable(true,false,0));
 assert(keyboard::pairingDurable(true,true,0));
 std::puts("PASS reconnect shared policy: bounded alternating fallback, late keyboard, known/resolved nameless identity, wrong peer/type rejection, bond/identity write failure is not durable pairing; RF and controller resolution not simulated");
}
