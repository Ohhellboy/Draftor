// TEST_ONLY endpoint format reused from the completed finite B1 experiment.
// No B1 parser, resource, cache or scheduling changes are included.
#pragma once
#include "release_config.hpp"
#include "ui_trace.hpp"
namespace ime_device {
bool serviceCompletion(UiTraceKind);
bool serviceEndpoint(UiTraceKind,uint8_t detail);
void snapshotServiceEndpoint(UiTraceRecord&,const hanime_extension::Composition*,const CandidateWindow*);
size_t formatServiceEndpoint(const UiTraceRecord&,unsigned,char*,size_t);
void traceServiceFeedback(const hanime_extension::Composition*,const CandidateWindow*,uint64_t,uint8_t);
}
