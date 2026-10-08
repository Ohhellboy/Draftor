#pragma once
#include <array>
#include "hanime/exact_ranker.hpp"
#include <cstddef>
#include <cstdint>
namespace observe {
struct Range {std::size_t begin{},end{};bool hit{};int terminal{-1};std::uint64_t work{},reads{},bytes{};std::size_t units{};int weight{};bool matched{};hanime::RankItem item{};std::size_t source_begin{},source_end{};};
struct Counters {
 std::array<std::uint64_t,15> phases{};
 std::uint64_t legal_intervals{},primary_intervals{},offers{},slot_checks{},dp_edges{},dp_updates{},retained{},retired{},prefix{},direct{},formal{},feasibility{};
 std::uint64_t prefix_checks{},prefix_resets{},prefix_offers{},prefix_evictions{},prefix_saves{};
 bool reusable{};std::array<Range,2048> ranges{};std::size_t count{};bool overflow{};
 void range(std::size_t b,std::size_t e,bool hit){if(count==ranges.size()){overflow=true;return;}ranges[count++]={b,e,hit,hit?3:-1,0,0,0};}
 void full_fact(const hanime::RankItem& item){if(count)ranges[count-1].item=item;}
 void source(std::size_t b,std::size_t e){if(count){ranges[count-1].source_begin=b;ranges[count-1].source_end=e;}}
 void fact(std::size_t units,int weight,bool matched){if(count){auto& x=ranges[count-1];x.units=units;x.weight=weight;x.matched=matched;}}
 void cost(std::size_t w,std::size_t r,std::size_t b){if(count&&!ranges[count-1].hit){auto& x=ranges[count-1];x.work+=w;x.reads+=r;x.bytes+=b;}}
 void done(int t){if(count&&!ranges[count-1].hit)ranges[count-1].terminal=t;}
};
inline Counters counters;
}
