#include "tf_probe.hpp"
#include "tf_probe_expected.hpp"
namespace ime_device {
TfProbe::TfProbe(std::string path,std::shared_ptr<const Mount> mount):path_(std::move(path)),mount_(std::move(mount)),epoch_(mount_->epoch),requested_(clockUs()){}
void TfProbe::fail(const char* text){error_=text;finished_=true;reader_.reset();}
size_t TfProbe::sampleOffset(unsigned c,unsigned i){
 if(cached(c))i/=2;
 return c<4||c==12?200+i*requestSize(c):size_t(((uint64_t(i)*2654435761ULL+8191)%(ProbeSharedBytes-(c>=12?4096:1024)))/512)*512;
}
void TfProbe::service(){
 expireAt(clockUs());
 if(finished_||pending_)return;
 if(!mount_->available||mount_->epoch!=epoch_){fail("original TF mount expired");return;}
 if(!reader_){
  result_={};result_.id=case_;result_.request_bytes=requestSize(case_);result_.expected=ProbeCrc[case_];result_.first_us=clockUs();
  const auto before=metrics.io();
  reader_=std::make_unique<TfReader>(mount_,path_,cached(case_)?TfReader::CacheLines:0);
  if(!reader_->valid()||reader_->size()!=ProbeSharedBytes){fail("shared.hnv unavailable/size mismatch; install matched set");return;}
  if(cached(case_)&&!reader_->cacheBytes()){fail("probe cache allocation failed; no uncached comparison masquerading as cache");return;}
  metrics.step(Phase::TfProbe,result_.first_us,0,0,0,before);result_.exec_us=clockUs()-result_.first_us;
  sample_=0;crc_=0xffffffffU;return;
 }
 const auto start=clockUs();const auto before=metrics.io();
 const bool ok=case_>=12?reader_->readBlock(sampleOffset(case_,sample_),buffer_.data(),result_.request_bytes):reader_->read(sampleOffset(case_,sample_),buffer_.data(),result_.request_bytes);
 if(ok)for(size_t i=0;i<result_.request_bytes;++i){crc_^=buffer_[i];for(unsigned bit=0;bit<8;++bit)crc_=(crc_>>1)^(0xedb88320U&(0U-(crc_&1U)));}
 metrics.step(Phase::TfProbe,start,1,result_.request_bytes,result_.request_bytes,before);
 result_.exec_us+=clockUs()-start;
 if(!ok){fail("shared.hnv sample read failed; preserve documents");return;}
 if(++sample_<(cached(case_)?Samples*2:Samples))return;
 result_.last_us=clockUs();result_.crc=~crc_;result_.io=reader_->io();
 result_.max_read_us=reader_->maxReadUs();result_.max_seek_us=reader_->maxSeekUs();
 result_.ok=result_.crc==result_.expected;reader_.reset();pending_=true;
 if(!result_.ok){fail("shared.hnv sample CRC mismatch; reinstall matched set");return;}
 finished_=++case_==Cases;
}
}
