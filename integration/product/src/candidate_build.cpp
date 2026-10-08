#include "candidate_build.hpp"
#include "candidate_window.hpp"
namespace ime_device {
size_t CandidateBuildDiagnostics::poolBytes(){
#if HANIME_N1_ROWS == 4
 return sizeof(CandidateWindow::PreparedProbes);
#else
 return 0;
#endif
}
ProbePoolReceipt CandidateBuildDiagnostics::pool(const CandidateWindow* w){
#if HANIME_N1_ROWS == 4
 return w?ProbePoolReceipt{w->prepared_.get(),poolBytes(),w->prepared_attempted_}:ProbePoolReceipt{};
#else
 (void)w;return {};
#endif
}
}
