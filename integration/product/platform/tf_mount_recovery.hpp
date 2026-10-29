#pragma once
#include <algorithm>
#include <cstdint>
namespace s3tf {
// Boot-only: the caller must not have created font/IME/document readers yet.
// Port owns this SD/SPI pair. A successful mount transfers it to the app.
struct MountAttempt {
  uint32_t elapsed_ms=0;
  bool mounted=false,timed_out=false;
  const char* reason="not_started";
  int fat_result=-1;
};
struct MountResult { bool mounted=false;unsigned attempts=0;uint32_t elapsed_ms=0,max_call_ms=0;bool cleanup_failed=false; };
constexpr unsigned MaximumAttempts=3;
constexpr uint32_t TotalBudgetMs=15000,AttemptBudgetMs=4000,RetryPauseMs=250;
template<class Port> MountResult recoverMount(Port& port) {
  MountResult result;
  const auto start=port.now();
  // No runtime remount: retaining a mounted port or a reader is a caller error.
  if(!port.unowned())return result;
  if(!port.park()){result.cleanup_failed=true;result.elapsed_ms=uint32_t(port.now()-start);return result;}
  while(result.attempts<MaximumAttempts) {
    const auto used=uint32_t(port.now()-start);
    if(used>=TotalBudgetMs)break;
    const auto budget=std::min(AttemptBudgetMs,TotalBudgetMs-used);
    const auto call=port.now();
    const auto attempt=port.mount(budget);
    ++result.attempts;
    result.max_call_ms=std::max(result.max_call_ms,uint32_t(port.now()-call));
    port.record(result.attempts,attempt);
    if(attempt.mounted) { result.mounted=true;break; }
    // The original SD/SPI port retains its cleanup and pin parking. SDMMC
    // reports uncertain cleanup here; do not retry or replace its ownership.
    if(!port.park()){result.cleanup_failed=true;break;}
    const auto elapsed=uint32_t(port.now()-start);
    if(result.attempts==MaximumAttempts||elapsed>=TotalBudgetMs)break;
    port.pause(std::min(RetryPauseMs,TotalBudgetMs-elapsed));
  }
  result.elapsed_ms=uint32_t(port.now()-start);
  return result;
}
}
