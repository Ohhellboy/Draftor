#include "order_allocation.hpp"
#include "tf_resources.hpp"
#include "io_profile.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits>
#include <cstring>
#include <cstdio>
namespace ime_device {
#ifndef ARDUINO
TfReader::ReadObserver TfReader::readObserver=nullptr;
#endif
TfReader::TfReader(std::shared_ptr<const Mount> mount,const std::string& path,size_t cacheLines):mount_(std::move(mount)),epoch_(mount_->epoch) {
 auto start=clockUs();fd_=::open(path.c_str(),O_RDONLY);metrics.max_open_us=std::max(metrics.max_open_us,clockUs()-start);
 if(fd_<0)return;
 metrics.openHandle(true);
 struct stat st{};
 if(::fstat(fd_,&st)!=0||!S_ISREG(st.st_mode)||st.st_size<0||uintmax_t(st.st_size)>std::numeric_limits<size_t>::max()){::close(fd_);fd_=-1;metrics.closeHandle(true);return;}
 bytes_=static_cast<size_t>(st.st_size);
 // A fresh private O_RDONLY descriptor starts at zero. No other owner seeks
 // this fd; cache hits never change this physical-position receipt.
 position_=0;position_known_=true;role_=fileRole(path);
#ifndef ARDUINO
 if(readObserver)readObserver(fd_,path.c_str(),0,bytes_);
#endif
 line_count_=std::min(cacheLines,CacheLines);
 if(line_count_){
  cache_.reset(draftor::memory::array<uint8_t>(cacheBytes()));
  if(!cache_){line_count_=0;++metrics.cache_alloc_failures;} // Correct uncached fallback.
  metrics.cache_live_bytes+=cacheBytes();metrics.cache_peak_bytes=std::max(metrics.cache_peak_bytes,metrics.cache_live_bytes);
 }
}
TfReader::~TfReader(){
#ifndef ARDUINO
 if(readObserver&&fd_>=0)readObserver(fd_,nullptr,SIZE_MAX,0);
#endif
 metrics.cache_live_bytes-=cacheBytes();if(fd_>=0){::close(fd_);metrics.closeHandle(true);}
}
bool TfReader::valid()const noexcept{return fd_>=0&&!failed_&&mount_->available&&mount_->epoch==epoch_;}
bool TfReader::read(size_t offset,void* dst,size_t count)const {
 return readBounded(offset,dst,count,MaximumRead);
}
bool TfReader::readBounded(size_t offset,void* dst,size_t count,size_t limit)const {
 if(!valid()||offset>bytes_||count>bytes_-offset||count>limit||(!dst&&count)||offset>size_t(std::numeric_limits<off_t>::max()))return false;
 if(!count)return true;
 const auto requestStart=clockUs();
 auto& role=ioProfile.roles[unsigned(role_)];++role.requests;
 ++role.sizes[IoProfile::bucket(count)];role.max_offset=std::max(role.max_offset,uint64_t(offset));
 IoCounters delta{};delta.requests=1;delta.requested_bytes=count;
 auto record=[&](){io_.add(delta);metrics.add(delta);};
 for(size_t i=0;i<line_count_;++i){auto& l=lines_[i];
  if(l.size&&offset>=l.offset&&offset-l.offset<=l.size&&count<=l.size-(offset-l.offset)){
   std::memcpy(dst,cache_.get()+i*MaximumRead+(offset-l.offset),count);l.used=++serial_;
   delta.hits=1;delta.hit_bytes=count;++role.hits;record();ioProfile.request(count,true);
   ioProfile.record(IoKind::CacheHit,count,count,clockUs()-requestStart,true);return true;
  }
 }
 delta.misses=1;
 ioProfile.request(count,false);
 size_t slot=0;
 for(size_t i=1;i<line_count_;++i)if(lines_[i].used<lines_[slot].used)slot=i;
 // Anchor at the request (metadata pages start at unaligned offsets). A miss
 // needs one fill, never two reads across artificial cache-line boundaries.
 const auto amount=line_count_?std::min(MaximumRead,bytes_-offset):count;
 void* buffer=line_count_?cache_.get()+slot*MaximumRead:dst;
 if(line_count_)lines_[slot].size=0; // Never publish a short/failed fill.
 const bool positioned=!position_known_||position_!=offset;
 if(!positioned)++role.position_matches;
 if(positioned){
  if(position_known_){const auto jump=position_>offset?position_-offset:offset-position_;
   ++role.jumps[RoleIo::jumpBucket(jump)];role.max_jump=std::max(role.max_jump,uint64_t(jump));
   if(offset<position_)++role.backward;else ++role.forward;
  }
  auto start=clockUs();auto sought=::lseek(fd_,static_cast<off_t>(offset),SEEK_SET);const auto elapsed=clockUs()-start;
  ++role.seeks;role.seek_us+=elapsed;role.max_seek_us=std::max(role.max_seek_us,elapsed);
  ++delta.seeks;delta.seek_us+=elapsed;metrics.max_seek_us=std::max(metrics.max_seek_us,elapsed);
  max_seek_us_=std::max(max_seek_us_,elapsed);ioProfile.record(IoKind::Seek,0,0,elapsed,sought==static_cast<off_t>(offset));
  if(sought!=static_cast<off_t>(offset)){++role.errors;position_known_=false;failed_=true;record();return false;}
 }
 const auto start=clockUs();const auto got=::read(fd_,buffer,amount);const auto elapsed=clockUs()-start;
 delta.reads=1;delta.bytes=got>0?size_t(got):0;delta.read_us=elapsed;
 metrics.max_read_us=std::max(metrics.max_read_us,elapsed);metrics.largest_read=std::max(metrics.largest_read,amount);
 position_known_=got>=0;position_=offset+(got>0?size_t(got):0);
 // No unbounded short-read retry (including EINTR); failed request is explicit.
 const bool ok=got>=0&&static_cast<size_t>(got)==amount&&valid();
 ++role.reads;role.bytes+=got>0?size_t(got):0;role.read_us+=elapsed;
 role.max_read_us=std::max(role.max_read_us,elapsed);role.errors+=!ok;
 if(!ok)failed_=true;
 max_read_us_=std::max(max_read_us_,elapsed);
 ioProfile.record(positioned?IoKind::Positioned:IoKind::Sequential,amount,delta.bytes,elapsed,ok);
#ifndef ARDUINO
 if(readObserver)readObserver(fd_,nullptr,offset,delta.bytes);
#endif
 if(ok&&line_count_){lines_[slot]={offset,amount,++serial_};std::memcpy(dst,buffer,count);}
 record();return ok;
}
Resources::~Resources(){retire();}
bool Resources::fail(const char* text,const char* detail){
 char message[192];std::snprintf(message,sizeof(message),"%s%s%s; install matched set/restart",text,*detail?": ":"",detail);error_=message;
 // Keep owners alive until DeviceIme stops its original tickets/transactions.
 state_=ResourceState::Error;return false;
}
void Resources::closeScheme(){
 scheme.deactivate(); // Detaches BaseOrder while both primary owners are still alive.
 if(verified_order_)verified_order_->deactivate();
 verified_order_.reset();order_index_.reset();order_reader_.reset();
 if(verified_scheme_)verified_scheme_->deactivate();
 verified_scheme_.reset();scheme_index_.reset();scheme_reader_.reset();
}
void Resources::retire(){
 admission_.cancel();closeScheme();shared.deactivate();
 if(verified_shared_)verified_shared_->deactivate();
 verified_shared_.reset();shared_index_.reset();shared_reader_.reset();
 state_=ResourceState::Dormant;stage_work_=0;admission_progress_={};
}
void Resources::releaseFailed(){if(state_==ResourceState::Error){retire();state_=ResourceState::Error;}}
size_t Resources::cacheBytes()const{
 const auto a=sharedMemory(),b=schemeMemory(),c=orderMemory();return a.data_payload_bytes+a.index_payload_bytes+b.data_payload_bytes+b.index_payload_bytes+c.data_payload_bytes+c.index_payload_bytes;
}
void Resources::start(hanime_extension::SchemeRules r){
 rules_=r;
 if(state_==ResourceState::Error)return; // Explicit restore/restart; never silently retry/full-scan.
 if(!pins_.supports(r)){fail("scheme trusted pins absent");return;}
 error_.clear();if(state_==ResourceState::Dormant)state_=ResourceState::OpenShared;else switchScheme(r);
}
void Resources::switchScheme(hanime_extension::SchemeRules r){
 rules_=r;if(state_==ResourceState::Error)return;
 if(!pins_.supports(r)){fail("scheme trusted pins absent");return;}
 error_.clear();closeScheme();
 if(state_==ResourceState::Ready||state_==ResourceState::OpenScheme||state_==ResourceState::OpenSchemeIndex||state_==ResourceState::ValidateScheme||state_==ResourceState::ActivateScheme||state_==ResourceState::OpenOrder||state_==ResourceState::OpenOrderIndex||state_==ResourceState::ValidateOrder||state_==ResourceState::ActivateOrder){state_=ResourceState::OpenScheme;stage_work_=0;}
}
const char* Resources::schemeFile(bool proof)const{
 if(rules_==hanime_extension::SchemeRules::DirectExact)return proof?pins_.direct->proof_file:pins_.direct->code_file;
 return rules_==hanime_extension::SchemeRules::R1Xiaohe?(proof?"xiaohe.hnv":"xiaohe.hnm"):(proof?"full_pinyin.hnv":"full_pinyin.hnm");
}
const char* Resources::orderFile(bool proof)const{
 if(rules_==hanime_extension::SchemeRules::DirectExact)return proof?pins_.direct->order_proof:pins_.direct->order_file;
 return rules_==hanime_extension::SchemeRules::R1Xiaohe?(proof?"xiaohe_base.hnv":"xiaohe_base.hbo"):(proof?"full_pinyin_base.hnv":"full_pinyin_base.hbo");
}
bool Resources::checkHealth(){
 if(!mount_->available)return fail("TF mount unavailable");
 if(shared_reader_&&!shared_reader_->healthy())return fail("shared.hns read/mount failed");
 if(shared_index_&&!shared_index_->healthy())return fail("shared.hnv read/mount failed");
 if(scheme_reader_&&!scheme_reader_->healthy())return fail(schemeFile(),"read/mount failed");
 if(scheme_index_&&!scheme_index_->healthy())return fail(schemeFile(true),"read/mount failed");
 if(order_reader_&&!order_reader_->healthy())return fail(orderFile(),"read/mount failed");
 if(order_index_&&!order_index_->healthy())return fail(orderFile(true),"read/mount failed");
 if(verified_order_&&!verified_order_->healthy())return fail(orderFile(),verified_order_->error());
 if(verified_shared_&&!verified_shared_->healthy())return fail("shared.hns/.hnv authentication",verified_shared_->error());
 if(verified_scheme_&&!verified_scheme_->healthy())return fail(schemeFile(),verified_scheme_->error());
 return true;
}
void Resources::service(){
 if(state_==ResourceState::Dormant||state_==ResourceState::Error)return;
 if(!checkHealth()||state_==ResourceState::Ready)return;
 const auto start=clockUs();const auto io=metrics.io();const auto oldState=state_;
 hanime::WorkCost cost;Phase phase=Phase::Open;
 switch(state_){
 case ResourceState::OpenShared:
  if(pins_.shared->verification_format==1){
   shared_reader_=std::make_shared<TfReader>(mount_,root_+"/shared.hns");
   if(!shared_reader_->valid()){fail("shared.hns open failed");break;}
  }else if(pins_.shared->verification_format!=2){fail("shared unsupported trusted format");break;}
  state_=ResourceState::OpenSharedIndex;break;
 case ResourceState::OpenSharedIndex:{
  shared_index_=std::make_shared<TfReader>(mount_,index_root_+"/shared.hnv");
  if(!shared_index_->valid()){fail("shared.hnv open failed");break;}
  // Packed authentication anchors belong only to trusted format 2.
  // Format 1 still uses its full verifier with the original data/proof quotas.
  auto config=SharedVerifiedConfig;
  if(pins_.shared->verification_format!=2)config.packed_anchor_slots=0;
  verified_shared_=std::make_shared<hanime::VerifiedResourceReader>(shared_reader_,shared_index_,*pins_.shared,config);
  if(!verified_shared_->prepare_bootstrap()){fail("shared bootstrap",verified_shared_->error());break;}
  state_=ResourceState::ValidateShared;break;}
 case ResourceState::ValidateShared:{
  phase=Phase::SharedValidation;const auto r=verified_shared_->advance_bootstrap(InitBudget);cost=r.cost;
  if(r.status==hanime::ResourceReadStatus::Ready)state_=ResourceState::ActivateShared;
  else if(r.status==hanime::ResourceReadStatus::Error)fail("shared bootstrap",verified_shared_->error());
  break;
 }
 case ResourceState::ActivateShared:
  if(!shared.open_verified(verified_shared_,mount_->epoch,{SharedCachePages,1024,true,SharedOutputReuse},&error_)){fail("shared verified open",error_.c_str());break;}
  if(admission_.begin(shared,64,128)!=hanime::TextAccessStatus::Pending){fail("release text certificate begin");break;}
  state_=ResourceState::AdmitText;break;
 case ResourceState::AdmitText:{
  phase=Phase::Admission;const auto r=admission_.advance(InitBudget);cost=r.cost;admission_progress_=r;
  if(r.status==hanime::TextAccessStatus::Ready)state_=ResourceState::OpenScheme;
  else if(r.status!=hanime::TextAccessStatus::Pending)fail("release text capacity/certificate incompatible");
  break;
 }
 case ResourceState::OpenScheme:
  if(pins_.scheme(rules_).verification_format==1){
  scheme_reader_=std::make_shared<TfReader>(mount_,(rules_==hanime_extension::SchemeRules::DirectExact?root_+"/"+schemeFile():root_+(rules_==hanime_extension::SchemeRules::R1Xiaohe?"/xiaohe.hnm":"/full_pinyin.hnm")));
  if(!scheme_reader_->valid()){fail(schemeFile(),"open failed");break;}
  }else if(pins_.scheme(rules_).verification_format!=2){fail("scheme unsupported trusted format");break;}
  state_=ResourceState::OpenSchemeIndex;break;
 case ResourceState::OpenSchemeIndex:
  scheme_index_=std::make_shared<TfReader>(mount_,(rules_==hanime_extension::SchemeRules::DirectExact?index_root_+"/"+schemeFile(true):index_root_+(rules_==hanime_extension::SchemeRules::R1Xiaohe?"/xiaohe.hnv":"/full_pinyin.hnv")));
  if(!scheme_index_->valid()){fail(schemeFile(true),"open failed");break;}
  verified_scheme_=std::make_shared<hanime::VerifiedResourceReader>(scheme_reader_,scheme_index_,pins_.scheme(rules_),VerifiedConfig);
  if(!verified_scheme_->prepare_bootstrap()){fail("scheme bootstrap",verified_scheme_->error());break;}
  state_=ResourceState::ValidateScheme;break;
 case ResourceState::ValidateScheme:{
  phase=Phase::SchemeValidation;const auto r=verified_scheme_->advance_bootstrap(InitBudget);cost=r.cost;
  if(r.status==hanime::ResourceReadStatus::Ready)state_=ResourceState::ActivateScheme;
  else if(r.status==hanime::ResourceReadStatus::Error)fail("scheme bootstrap",verified_scheme_->error());
  break;
 }
 case ResourceState::ActivateScheme:
  if(!scheme.open_verified(verified_scheme_,shared,{1,1024},&error_)){fail("scheme verified open",error_.c_str());break;}
  state_=ResourceState::OpenOrder;break;
 case ResourceState::OpenOrder:
  if(!pins_.order(rules_)){fail(orderFile(),"trusted pin absent");break;}
  if(pins_.order(rules_)->verification_format==1){
  order_reader_=std::make_shared<TfReader>(mount_,index_root_+"/"+orderFile());
  if(!order_reader_->valid()){fail(orderFile(),"open failed");break;}
  }else if(pins_.order(rules_)->verification_format!=2){fail("order unsupported trusted format");break;}
  state_=ResourceState::OpenOrderIndex;break;
 case ResourceState::OpenOrderIndex:
  order_index_=std::make_shared<TfReader>(mount_,index_root_+"/"+orderFile(true));
  if(!order_index_->valid()){fail(orderFile(true),"open failed");break;}
  verified_order_=std::make_shared<hanime::VerifiedResourceReader>(order_reader_,order_index_,*pins_.order(rules_),OrderConfig);
  if(!verified_order_->prepare_bootstrap()){fail(orderFile(),verified_order_->error());break;}
  state_=ResourceState::ValidateOrder;break;
 case ResourceState::ValidateOrder:{
  phase=Phase::OrderValidation;const auto r=verified_order_->advance_bootstrap(InitBudget);cost=r.cost;
  if(r.status==hanime::ResourceReadStatus::Ready)state_=ResourceState::ActivateOrder;
  else if(r.status==hanime::ResourceReadStatus::Error)fail(orderFile(),verified_order_->error());
  break;
 }
 case ResourceState::ActivateOrder:{
  OrderAllocationScope allocation_scope;
  if(!scheme.attach_base_order(std::make_shared<hanime::BaseOrderIndex>(verified_order_,scheme,shared))){fail(orderFile(),"binding rejected");break;}
  state_=ResourceState::Ready;break;
 }
 default:break;
 }
 if(phase==Phase::Open){const auto delta=metrics.io()-io;cost.read_calls=delta.requests;cost.read_bytes=delta.requested_bytes;cost.work_items=1;}
 metrics.step(phase,start,cost.read_calls,cost.read_bytes,cost.work_items,io);
 stage_work_=state_==oldState?stage_work_+cost.work_items:0;
}
}
