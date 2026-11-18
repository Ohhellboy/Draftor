#pragma once
#include "hanime/execution_policy.hpp"
#include "device_metrics.hpp"
#include "hanime/core.hpp"
namespace ime_device {
inline uint64_t rankingDeadline=0;
// A surrounding main-loop window supplies its existing deadline. Host callers
// without that scope receive one normal 1ms slice, not a new deadline per layer.
struct RankingExecutionWindow {
 uint64_t previous;
 explicit RankingExecutionWindow(uint64_t end):previous(rankingDeadline){rankingDeadline=end;}
 ~RankingExecutionWindow(){rankingDeadline=previous;}
};
struct RankingCallBudget {
 uint64_t end;
 explicit RankingCallBudget(uint64_t fallback):end(rankingDeadline?rankingDeadline:fallback){}
 static bool stop(void* p){return clockUs()>=static_cast<RankingCallBudget*>(p)->end;}
 hanime::WorkBudget get(hanime::WorkBudget b){
#if BGIME_RANKING_ARM >= 1
  b.stop_requested=stop;b.stop_context=this;
#endif
  return b;
 }
};
}
