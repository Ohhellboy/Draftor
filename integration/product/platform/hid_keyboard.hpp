#pragma once
#include <array>
#include "body_key_repeat.hpp"
#include <cstddef>
#include <cstdint>
namespace s3hid {
struct State { std::array<uint8_t,32> keys{}; uint8_t mods=0;
 bool has(unsigned u)const{return u<256&&(keys[u/8]&(1u<<(u%8)));}
 void set(unsigned u){if(u>=224&&u<=231)mods|=1u<<(u-224);else if(u<224&&u>3)keys[u/8]|=1u<<(u%8);}
};
struct Event {uint32_t epoch=0,ms=0;uint8_t usage=0,mods=0;bool down=false;uint32_t route_epoch=0;uint32_t ime_frame=0;uint32_t ime_attempt=0;uint32_t event_id=0;uint64_t received_us=0;uint32_t ime_context=0;uint32_t resource_owner=1;};
struct Field {uint16_t offset=0,count=0,min=0,max=0;uint8_t size=0,n=0;bool variable=false;std::array<uint16_t,16> usages{};};
struct Report {uint8_t id=0,fields=0;uint16_t bits=0;std::array<Field,16> field{};};
class Map {
 public:
 bool parse(const uint8_t* bytes,size_t n) noexcept;
 const Report* report(uint8_t id)const noexcept;
 static bool decode(const Report&,const uint8_t*,size_t,State&)noexcept;
 private:std::array<Report,8> reports_{};uint8_t count_=0;
};
// Diff one report, preserving state from other keyboard report characteristics.
class Tracker {
 public:
 template<class Emit> void update(unsigned slot,const State& state,uint32_t epoch,uint32_t ms,Emit emit){
  if(slot>=states_.size())return;
  states_[slot]=state; State merged;
  for(auto& s:states_){merged.mods|=s.mods;for(size_t i=0;i<32;++i)merged.keys[i]|=s.keys[i];}
  for(unsigned i=0;i<8;++i)if((held_.mods^merged.mods)&(1u<<i))emit(Event{epoch,ms,uint8_t(224+i),merged.mods,bool(merged.mods&(1u<<i))});
  for(unsigned i=4;i<224;++i)if(held_.has(i)&&!merged.has(i))emit(Event{epoch,ms,uint8_t(i),merged.mods,false});
  for(unsigned i=4;i<224;++i)if(!held_.has(i)&&merged.has(i))emit(Event{epoch,ms,uint8_t(i),merged.mods,true});
  held_=merged;
 }
 bool allReleased()const noexcept{if(held_.mods)return false;for(auto k:held_.keys)if(k)return false;return true;}
 void clear() noexcept{held_={};states_={};}
 private: State held_{};std::array<State,8> states_{};
};
// Captured once, under the BLE state lock, for one decoded report. A queued
// event never borrows the context that happens to be current when dequeued.
// Resource-owner retirement is independent of raw edits and accepted successors.
// Zero is unassigned. Only the main loop advances/publishes this generation.
struct IngressContext {uint32_t route=1,frame=0,attempt=0,ime_context=0,resource_owner=1;};
template<class Emit> void dispatchReport(Tracker& tracker,unsigned slot,const State& state,
 uint32_t epoch,uint32_t ms,const IngressContext context,Emit emit){
 tracker.update(slot,state,epoch,ms,[&](Event event){
  event.route_epoch=context.route;event.ime_frame=context.frame;event.ime_attempt=context.attempt;event.ime_context=context.ime_context;event.resource_owner=context.resource_owner;emit(event);
 });
}
// Physical UK ISO layout. Ctrl/GUI shortcuts use raw usage separately.
uint32_t uk(uint8_t usage,uint8_t mods)noexcept;
class Repeat {
 public:
 void clear()noexcept{repeater_.cancel();usage_=mods_=0;}
 void actionResult(bool changed)noexcept{repeater_.noteActionResult(changed,!changed);}
 void context(uint64_t session,bool body_focused)noexcept{if(session!=session_||!body_focused)clear();session_=session;focused_=body_focused;}
 void event(const Event& e)noexcept;
 bool poll(uint32_t now,uint32_t epoch,Event& out)noexcept;
 private:hanime::body_extension::BodyKeyRepeater repeater_;uint64_t session_=0;uint32_t epoch_=0;uint8_t usage_=0,mods_=0;bool focused_=true;
};
}
