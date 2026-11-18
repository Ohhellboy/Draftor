#pragma once
// 0: exact Fix R1 behavior; 1: EXACT_BATCH; 2: SPARSE_MERGE plus batch execution.
#ifndef BGIME_RANKING_ARM
#define BGIME_RANKING_ARM 0
#endif
static_assert(BGIME_RANKING_ARM>=0 && BGIME_RANKING_ARM<=2,"invalid ranking arm");
