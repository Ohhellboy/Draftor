#pragma once
#include "hanime/bounded_user_state.hpp"
#if BGIME_KEY_FENCE
#include "hanime/key_fence.hpp"
#endif
#include "hanime_extension/extension.hpp"
#include "tf_resources.hpp"
#include "r2_storage.hpp"
#include "r2_memory.hpp"
#include <array>
#include <memory>
#include <string>
#if BGIME_ARCH3_R1
#include "personal_index.hpp"
#endif

namespace ime_device {
// TEST_ONLY device integration. One loop-task owner; neither dictionary files
// nor the document storage owner are reachable through this port.
struct LearningIo {
 uint64_t reads=0,writes=0,read_bytes=0,write_bytes=0,controls=0,syncs=0,seeks=0,errors=0;
 uint64_t read_us=0,write_us=0,sync_us=0,control_us=0,max_read_us=0,max_write_us=0,max_sync_us=0,max_control_us=0;
 unsigned handles=0,peak_handles=0;int last_errno=0;
};
class LearningFiles final:public hanime::StateFilePort {
 std::array<std::string,2> paths_;std::array<int,2> fd_{{-1,-1}};
 std::shared_ptr<const Mount> mount_;uint64_t epoch_;r2::FileOps storage_;bool valid_paths_=false;
 void validatePaths(){valid_paths_=paths_[0]!=paths_[1]&&r2::allowedStatePath(paths_[0])&&r2::allowedStatePath(paths_[1]);}
 bool available(unsigned)const;bool offset(uint64_t,size_t=0)const;
 void measured(unsigned,uint64_t,bool,size_t=0);
 public:
#if BGIME_KEYFENCE_S3_AB
 bool test_selected=false;
 bool bindTestSession(unsigned n){if(n<1||n>3||test_selected||io.controls||io.reads||io.writes||io.handles)return false;const std::string root=std::string(BGIME_TEST_STATE_ROOT)+"/session"+char('0'+n);paths_={{root+"/a.hns",root+"/b.hns"}};validatePaths();test_selected=valid_paths_;return test_selected;}
#endif
 LearningIo io;
 LearningFiles(std::string,std::string,std::shared_ptr<const Mount>);
 ~LearningFiles() override;
 bool current()const override{return available(0);}
 hanime::FileResult open(unsigned,bool) override;
 bool size(unsigned,uint64_t&) override;
 hanime::FileTransfer read(unsigned,uint64_t,void*,size_t) override;
 hanime::FileTransfer write(unsigned,uint64_t,const void*,size_t) override;
 bool sync(unsigned) override;bool truncate(unsigned,uint64_t) override;bool close(unsigned) override;
};
enum class LearningPhase:uint8_t { Dormant,Opening,Ready,Closing,Closed,Error };
struct LearningTrialStats {
 uint64_t steps=0,max_step_us=0,accepted=0,body_accepted=0,requested_members=0,ram_members=0,saturated_members=0,saved_members=0,paused_members=0;
 uint64_t confirmed_writes=0,observation_overflow=0,max_pending=0,max_dirty=0,max_batches=0,max_pending_us=0,oldest_pending_us=0;
 hanime::StateCost open_cost{},close_cost{};
};
class LearningTrial : public r2::ExternalObject {
 struct Watch {hanime::CompositionLearningTicket ticket;std::array<uint8_t,64> observed{};size_t members=0,done=0;uint64_t accepted_us=0;uint64_t observation_commit=0;};
 std::shared_ptr<const Mount> files_mount_;LearningFiles files_;hanime::StateV3 state_;std::unique_ptr<hanime::BoundedUserState> owner_;
 std::unique_ptr<hanime::SparseCorrections> sparse_;
#if BGIME_ARCH3_R1
 std::unique_ptr<r1::PersonalIndex> personal_;std::string personal_path_;bool personal_closing_started_=false;
#endif
 std::array<Watch,4> watches_{};size_t watch_=0,member_=0;
 LearningPhase phase_=LearningPhase::Dormant;hanime::StateStatus status_=hanime::StateStatus::MissingStore;
 uint32_t lambda_=1000;bool record_=false,apply_=false;
 hanime::CountTicket reset_ticket_;hanime::CountResetStatus reset_result_=hanime::CountResetStatus::Idle;
 void observe(Watch&,size_t);void observeStep();void peaks();
 public:
 static constexpr size_t H=4,B=4,M=64,Replay=512;
 static constexpr hanime::StateBudget Budget{1,1,256,256,1,32};
 LearningTrialStats stats;
 LearningTrial(std::string,std::string,std::shared_ptr<const Mount>);
#if BGIME_KEYFENCE_S3_AB
 bool selectTestSession(unsigned n){return phase_==LearningPhase::Dormant&&!owner_&&files_.bindTestSession(n);}
#endif
#if BGIME_ARCH3_R1
 r1::PersonalIndex* personal(){return personal_.get();}
 r1::PersonalIndex& personal( Resources& r){if(!personal_)personal_=std::make_unique<r1::PersonalIndex>(*owner_,r,personal_path_);return *personal_;}
#endif
 bool open();bool beginClose();bool service(bool foreground_work);
 hanime::CountResetStatus beginReset(const hanime::StableLexemeKey&);
 hanime::CountResetStatus resetResult()const{return reset_result_;}
 bool resetBusy()const{return reset_result_==hanime::CountResetStatus::Pending;}
 void acknowledgeReset(){if(!resetBusy())reset_result_=hanime::CountResetStatus::Idle;}
 bool hasWork()const;bool ready()const{return phase_==LearningPhase::Ready;}
 LearningPhase phase()const{return phase_;}hanime::StateStatus status()const{return status_;}
 hanime::BoundedUserState* counts(){return owner_.get();}
 const LearningFiles& files()const{return files_;}
 hanime::StateFileInfo fileInfo()const{return state_.info();}
 hanime::StateCost costs()const{return owner_?owner_->costs():hanime::StateCost{};}
 auto stateDiagnostics()const{return state_.diagnostics();}
 auto stateMemory()const{return state_.memory_usage();}
 hanime::UserStateHealth health()const{return owner_?owner_->health():hanime::UserStateHealth{};}
 uint32_t lambda()const{return lambda_;}bool recording()const{return record_;}bool applying()const{return apply_;}
 bool configure(uint32_t,bool,bool);void configure(hanime_extension::Config&,Resources* = nullptr);
 hanime::SparseStatistics sparseStatistics()const{return sparse_?sparse_->statistics():hanime::SparseStatistics{};}
 const char* sparseFailure()const{return sparse_?sparse_->failure_reason():"disabled";}
#if BGIME_KEY_FENCE
 hanime::KeyFenceStatistics fenceStatistics()const{const auto* p=static_cast<const hanime::KeyFenceCorrections*>(sparse_.get());return p?p->acquisition_statistics():hanime::KeyFenceStatistics{};}
#endif
 void accepted(const hanime::CompositionResult&,bool body);
};
}
