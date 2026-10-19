#pragma once
#ifndef DRAFTOR_DISPLAY_TX_OBSERVE
#define DRAFTOR_DISPLAY_TX_OBSERVE 0
#endif
#if DRAFTOR_DISPLAY_TX_OBSERVE
#include "idle_log.hpp"
#include "ST7306_OsptekLpm8.h"
namespace display_tx_observe {
inline std::uint32_t dropped=0;
// One summary after the original feedback/candidate/body completion producers.
// No per-band lines, new queue or retry. Leave at least six lines available for
// the existing CP summary. done is the actual synchronous return timestamp.
inline bool emit(ime_device::IdleLog& out,std::uint32_t id,bool sent,
                 const ST7306OsptekLpm8::TransferStats& s,
                 std::uint64_t begin,std::uint64_t done,
                 std::uint32_t action,std::uint32_t query,std::uint32_t frame,
                 std::uint32_t body,std::uint64_t revision){
  if(ime_device::IdleLog::Capacity-out.size()<7){++dropped;return false;}
  using U=unsigned long long;
  const bool ok=out.printf("[DISPLAY_TX] id=%u begin=%llu done=%llu a=%u q=%u frame=%u body=%u revision=%llu sent=%u windows=%u payload=%u wire=%u compare_span=%u staging_copy=%u staging_fill=%u dropped=%u\n",
    unsigned(id),U(begin),U(done),unsigned(action),unsigned(query),unsigned(frame),
    unsigned(body),U(revision),unsigned(sent),sent?unsigned(s.windows):0,
    sent?unsigned(s.payload_bytes):0,sent?unsigned(s.wire_bytes):0,
    sent?unsigned(s.compare_span_bytes):0,sent?unsigned(s.staging_copy_bytes):0,
    sent?unsigned(s.staging_fill_bytes):0,
    unsigned(dropped));
  if(!ok)++dropped;
  return ok;
}
}
#endif
