#include "learning_trial.hpp"
#include "hanime/score_policy.hpp"
#include "device_metrics.hpp"
#include "execution_budget.hpp"
#include "response_observe.hpp"
#include "ui_trace.hpp"
#define LEARN_MEMBER(...) L3OBS(traceLearningMember(__VA_ARGS__))
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <limits>
#include <algorithm>

namespace ime_device {
using namespace hanime;
LearningFiles::LearningFiles(std::string a,std::string b,std::shared_ptr<const Mount> m)
 :paths_{{std::move(a),std::move(b)}},mount_(std::move(m)),epoch_(mount_?mount_->epoch:0),storage_(mount_){validatePaths();}
LearningFiles::~LearningFiles(){for(unsigned f=0;f<2;++f)if(fd_[f]>=0)close(f);}
// Paths are private and validated at construction/rebinding. Mount availability
// and epoch remain live checks on every use; FileOps also guards each syscall.
bool LearningFiles::available(unsigned f)const{return f<2&&valid_paths_&&mount_&&mount_->available&&mount_->epoch==epoch_&&epoch_;}
bool LearningFiles::offset(uint64_t off,size_t n)const{const auto max=uint64_t(std::numeric_limits<off_t>::max());return off<=max&&n<=max-off;}
void LearningFiles::measured(unsigned op,uint64_t start,bool ok,size_t bytes){
 const auto us=clockUs()-start;
 if(!ok){++io.errors;io.last_errno=errno;}
 if(op==0){++io.reads;io.read_bytes+=bytes;io.read_us+=us;io.max_read_us=std::max(io.max_read_us,us);}
 else if(op==1){++io.writes;io.write_bytes+=bytes;io.write_us+=us;io.max_write_us=std::max(io.max_write_us,us);}
 else {++io.controls;io.control_us+=us;io.max_control_us=std::max(io.max_control_us,us);
  if(op==2){++io.syncs;io.sync_us+=us;io.max_sync_us=std::max(io.max_sync_us,us);}}
}
FileResult LearningFiles::open(unsigned f,bool create){
 if(!available(f)||fd_[f]>=0)return FileResult::Error;
 const auto start=clockUs();fd_[f]=storage_.open(paths_[f],O_RDWR|(create?O_CREAT|O_EXCL:0),0600);
 const auto missing=fd_[f]<0&&errno==ENOENT;measured(3,start,fd_[f]>=0||missing);
 if(fd_[f]>=0){++io.handles;io.peak_handles=std::max(io.peak_handles,io.handles);return FileResult::Ok;}
 return missing?FileResult::Missing:FileResult::Error;
}
bool LearningFiles::size(unsigned f,uint64_t& out){
 if(!available(f)||fd_[f]<0)return false;
 struct stat s{};const auto start=clockUs();const bool ok=storage_.size(fd_[f],&s)==0&&S_ISREG(s.st_mode)&&s.st_size>=0;
 measured(3,start,ok);if(ok)out=uint64_t(s.st_size);return ok;
}
FileTransfer LearningFiles::read(unsigned f,uint64_t off,void* p,size_t n){
 if(!available(f)||fd_[f]<0||!offset(off,n))return {};
 const auto start=clockUs();++io.seeks;
 const auto result=storage_.read(fd_[f],p,n,off_t(off));
 measured(0,start,result>=0,result>0?size_t(result):0);return {result>=0,result>0?size_t(result):0};
}
FileTransfer LearningFiles::write(unsigned f,uint64_t off,const void* p,size_t n){
 if(!available(f)||fd_[f]<0||!offset(off,n))return {};
 const auto start=clockUs();++io.seeks;
 const auto result=storage_.write(fd_[f],p,n,off_t(off));
 measured(1,start,result>=0,result>0?size_t(result):0);return {result>=0,result>0?size_t(result):0};
}
bool LearningFiles::sync(unsigned f){if(!available(f)||fd_[f]<0)return false;const auto start=clockUs();const bool ok=storage_.sync(fd_[f])==0;measured(2,start,ok);return ok;}
bool LearningFiles::truncate(unsigned f,uint64_t n){if(!available(f)||fd_[f]<0||!offset(n))return false;const auto start=clockUs();const bool ok=storage_.truncate(fd_[f],off_t(n))==0;measured(3,start,ok);return ok;}
bool LearningFiles::close(unsigned f){
 if(f>1)return false;
 if(fd_[f]<0)return true;
 const auto start=clockUs();const auto fd=fd_[f];fd_[f]=-1;--io.handles;const bool ok=storage_.close(fd)==0;measured(3,start,ok);return ok;
}
namespace {
StateV3Config stateConfig(){StateV3Config c;c.registry_namespace=Resources::Namespace;c.lineage=0x4c33545232363130ULL;c.max_single_io=256;c.read_window_bytes=256;c.read_window_slots=16;return c;}
bool working(StateStatus s){return s==StateStatus::Pending||s==StateStatus::Ready||s==StateStatus::Found||s==StateStatus::NotFound||s==StateStatus::HostStorageConfirmed;}
}
LearningTrial::LearningTrial(std::string a,std::string b,std::shared_ptr<const Mount> mount)
 :files_mount_(mount),files_(a,std::move(b),std::move(mount)),state_(files_,stateConfig()){
#if BGIME_ARCH3_R1
 personal_path_=a+".relations";
#endif
}
bool LearningTrial::open(){
 if(phase_!=LearningPhase::Dormant&&phase_!=LearningPhase::Closed)return false;
#if BGIME_KEYFENCE_S3_AB
 if(!files_.test_selected)return false;
 constexpr bool allow_create=false;
#else
 #if BGIME_ARCH3_R1
 const auto guard_start=clockUs();struct stat derived_info{};const int derived_result=r2::FileOps(files_mount_).stat(personal_path_,&derived_info);
 bool allow_create=derived_result<0&&errno==ENOENT;
 ++files_.io.controls;
 if(allow_create){const int pending_result=r2::FileOps(files_mount_).stat(personal_path_+".pending",&derived_info);++files_.io.controls;allow_create=pending_result<0&&errno==ENOENT;}
 files_.io.control_us+=clockUs()-guard_start;
#else
 constexpr bool allow_create=true;
#endif
#endif
#if BGIME_ARCH3_R1
 personal_.reset();
#endif
 reset_ticket_={};reset_result_=CountResetStatus::Idle;
 sparse_.reset();owner_.reset();status_=state_.begin_open(allow_create);phase_=status_==StateStatus::Pending?LearningPhase::Opening:LearningPhase::Error;record_=false;return phase_==LearningPhase::Opening;
}
bool LearningTrial::beginClose(){
 if((!ready()&&phase_!=LearningPhase::Error)||!owner_)return false;
 if(resetBusy()){owner_->cancel_count(reset_ticket_);reset_ticket_={};reset_result_=CountResetStatus::Closed;}
 record_=false;apply_=false;
 if(phase_==LearningPhase::Error){status_=StateStatus::CloseUnsaved;return true;}
#if BGIME_KEY_FENCE
 if(sparse_)sparse_->cancel();
#endif
#if BGIME_ARCH3_R1
 personal_closing_started_=false;
#else
 owner_->begin_close();
#endif
 phase_=LearningPhase::Closing;return true;
}
void LearningTrial::peaks(){
 if(!owner_)return;
 const auto h=owner_->health();stats.max_pending=std::max(stats.max_pending,h.pending);stats.max_dirty=std::max(stats.max_dirty,uint64_t(h.dirty));stats.max_batches=std::max(stats.max_batches,uint64_t(h.batches_pending));
 stats.oldest_pending_us=0;const auto now=clockUs();for(const auto& w:watches_)if(w.members)stats.oldest_pending_us=std::max(stats.oldest_pending_us,now-w.accepted_us);
 stats.max_pending_us=std::max(stats.max_pending_us,stats.oldest_pending_us);
}
void LearningTrial::observe(Watch& w,size_t i){
 if(i>=w.members||w.observed[i]&4)return;
 const auto r=w.ticket.member(i);auto& seen=w.observed[i];
 if((r.status==LearningStatus::RecordedDirty||r.status==LearningStatus::NoChangeSaturated)&&!(seen&1)){
  LEARN_MEMBER(w.observation_commit,i,w.members,1,r);seen|=1;++stats.ram_members;if(r.status==LearningStatus::NoChangeSaturated)++stats.saturated_members;
 }
 if(r.persisted){LEARN_MEMBER(w.observation_commit,i,w.members,2,r);++stats.saved_members;seen|=4;++w.done;}
 else if(r.status!=LearningStatus::Pending&&r.status!=LearningStatus::NotNotified&&!(seen&1)){++stats.paused_members;seen|=4;++w.done;}
 if(w.done==w.members){stats.max_pending_us=std::max(stats.max_pending_us,clockUs()-w.accepted_us);w=Watch{};}
}
void LearningTrial::observeStep(){
 auto& w=watches_[watch_];if(w.members){observe(w,member_);++member_;}
 if(!w.members||member_>=w.members){member_=0;watch_=(watch_+1)%watches_.size();}
}
bool LearningTrial::hasWork()const{
 if(phase_==LearningPhase::Opening||phase_==LearningPhase::Closing)return true;
 if(!ready()||!owner_)return false;
 if(resetBusy())return true;
#if BGIME_ARCH3_R1
 if(personal_&&personal_->pending())return true;
#endif
 const auto h=owner_->health();if(h.dirty||h.reserved||h.intent_pending||h.frozen_write||h.batches_pending)return true;
 for(const auto& w:watches_)if(w.members)return true;
 return false;
}
CountResetStatus LearningTrial::beginReset(const StableLexemeKey& key){
 if(resetBusy())return CountResetStatus::Busy;
 if(!ready()||!owner_||key.empty())return reset_result_=CountResetStatus::Unavailable;
 const auto h=owner_->health();
 if(h.closing||h.intent_pending||h.batch_members_pending)return reset_result_=CountResetStatus::Busy;
 reset_ticket_=owner_->begin_count(owner_->view(),key);
 if(!reset_ticket_.revision)return reset_result_=CountResetStatus::Stale;
 return reset_result_=CountResetStatus::Pending;
}
bool LearningTrial::service(bool foreground){
 if(phase_==LearningPhase::Dormant||phase_==LearningPhase::Closed||phase_==LearningPhase::Error)return false;
 if(resetBusy()){
  const auto start=clockUs();RankingCallBudget deadline(start+1000);
  const auto r=owner_->advance_count(reset_ticket_,deadline.get({1,256,32}));
  if(r.status==CountStatus::Found||r.status==CountStatus::NotFound)reset_result_=owner_->reset_count(reset_ticket_);
  else if(r.status!=CountStatus::Pending)reset_result_=r.status==CountStatus::Stale?CountResetStatus::Stale:CountResetStatus::Unavailable;
  if(!resetBusy()){owner_->cancel_count(reset_ticket_);reset_ticket_={};}
  ++stats.steps;stats.max_step_us=std::max(stats.max_step_us,clockUs()-start);return true;
 }

 if(ready()&&foreground&&!owner_->background_due()){owner_->foreground_advanced();return false;}
 if(ready()&&!hasWork()&&!owner_->background_due())return false;
 const auto start=clockUs();
#if BGIME_ARCH3_R1
 if(personal_&&(ready()||phase_==LearningPhase::Closing))personal_->service();
#endif
 if(phase_==LearningPhase::Opening){
  const auto s=state_.advance_open(Budget);status_=s.status;stats.open_cost.add(s.cost);
  if(status_==StateStatus::Ready){owner_=std::make_unique<BoundedUserState>(state_,Resources::Namespace,UserStateConfig{H,64,8,B,M,true});phase_=owner_->valid()?LearningPhase::Ready:LearningPhase::Error;if(ready())L3OBS(l3diag::opened());}
  else if(status_!=StateStatus::Pending)phase_=LearningPhase::Error;
 }else{
  bool closing_now=phase_==LearningPhase::Closing;
#if BGIME_ARCH3_R1
  if(closing_now&&!personal_closing_started_){
   const auto h=owner_->health();
   if(!files_.current()||h.write_health==StateStatus::Unavailable||h.write_health==StateStatus::Corrupt||h.write_health==StateStatus::TailUncertain||(!h.dirty&&!h.reserved&&!h.intent_pending&&!h.frozen_write&&!h.batches_pending&&(!personal_||!personal_->pending()))){owner_->begin_close();personal_closing_started_=true;}
  }
  closing_now=closing_now&&personal_closing_started_;
#endif
  const auto s=closing_now?owner_->advance_close(Budget):owner_->advance_state(Budget);status_=s.io.status;
  if(s.event.confirmed)++stats.confirmed_writes;
  if(phase_==LearningPhase::Closing){stats.close_cost.add(s.io.cost);if(status_==StateStatus::ClosedSaved){phase_=LearningPhase::Closed;if(personal_)personal_->releaseClosed();L3OBS(l3diag::closed());}else if(status_==StateStatus::CloseUnsaved)phase_=LearningPhase::Error;}
  else if(!working(status_)&&status_!=StateStatus::MaintenanceBlocked)phase_=LearningPhase::Error;
  observeStep();if(phase_==LearningPhase::Closed)for(auto& w:watches_)for(size_t i=0;w.members&&i<w.members;++i)observe(w,i);peaks();
 }
 ++stats.steps;stats.max_step_us=std::max(stats.max_step_us,clockUs()-start);return true;
}
bool LearningTrial::configure(uint32_t lambda,bool record,bool apply){
 if(lambda!=100&&lambda!=1000)return false;
 if((record||apply)&&!ready())return false;
 lambda_=lambda;record_=record;apply_=apply;return true;
}
void LearningTrial::configure(hanime_extension::Config& c,Resources* resources){
#if BGIME_ARCH3_R1
 if((apply_||record_)&&ready()&&resources){try{auto& index=personal(*resources);if(apply_)c.sparse_corrections=&index;}catch(const std::bad_alloc&){c.sparse_corrections=nullptr;} // Optional owner unavailable: accurate E retains real Count.
 }
#endif
#if BGIME_KEY_FENCE
 if(apply_&&ready()&&resources&&resources->optimized()) {
  auto index=resources->scheme.base_order();
  auto* fence=static_cast<hanime::KeyFenceCorrections*>(sparse_.get());
  if(!fence||!fence->matches(*index)) {
   // Same optional envelope: original E/reader allowance + original reserve.
   try {sparse_=std::make_unique<hanime::KeyFenceCorrections>(*owner_,resources->shared,index,*resources,1024*1024-64*1024-256*1024);}
   catch(const std::bad_alloc&){sparse_.reset();} // Optional control allocation: accurate E remains available.
  }
  c.sparse_corrections=sparse_.get();
 }
#elif BGIME_RANKING_ARM == 2
 if(apply_&&ready()&&resources){
  if(!sparse_){hanime::SparseConfig cfg;cfg.allocate=&draftor::memory::allocate;cfg.release=&draftor::memory::release;
   sparse_=std::make_unique<hanime::SparseCorrections>(*owner_,*resources,cfg);}
  c.sparse_corrections=sparse_.get();
 }
#else
 (void)resources;
#endif
 c.learning_counts=owner_.get();c.apply_learning=apply_&&ready();c.rank_replay_capacity=Replay;c.exact_from_base_order=true;
#if BGIME_ARCH3_R1
 c.rank_replay_capacity=0;
 c.planning.exact_learning_members=c.apply_learning&&c.learning_counts;
#endif
 #if BGIME_L1_A_REV1_TEST_ONLY
 RankRule::make(hanime::ARev1PolicyRevision,0,&c.rank_rule);
#else
 RankRule::make(1,lambda_,&c.rank_rule);
#endif
}
void LearningTrial::accepted(const CompositionResult& result,bool body){
 if(result.status!=CompositionStatus::Accepted)return;
 ++stats.accepted;if(body)++stats.body_accepted;
 auto ticket=result.learning_ticket();const auto receipt=ticket.receipt();if(!receipt.members)return;
#if BGIME_ARCH3_R1
 if(body&&personal_)personal_->accepted(ticket);
#endif
 stats.requested_members+=receipt.members;
 // Reap terminal histories before accepting another observation. Four active
 // tickets match the four owner slots; fully rejected tickets need no slot.
 for(auto& w:watches_)for(size_t i=0;w.members&&i<w.members;++i)observe(w,i);
 if(receipt.paused==receipt.members){stats.paused_members+=receipt.paused;return;}
 for(auto& w:watches_)if(!w.members){w.ticket=ticket;w.members=receipt.members;w.accepted_us=clockUs();w.observation_commit=stats.accepted;for(size_t i=0;w.members&&i<w.members;++i)observe(w,i);peaks();return;}
 ++stats.observation_overflow; // Evidence failure, never replay the accepted text.
}
}
