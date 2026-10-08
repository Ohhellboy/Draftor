#include "hanime/state_v3.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace hanime {
void StateCost::add(const StateCost& c) {
  read_calls+=c.read_calls; write_calls+=c.write_calls; read_bytes+=c.read_bytes;
  write_bytes+=c.write_bytes; control_calls+=c.control_calls; work_items+=c.work_items;
}
namespace {
constexpr std::uint64_t header_bytes=128, record_bytes=36, frame_bytes=64;
constexpr std::uint32_t fnv_seed=2166136261U;
using Buffer=std::array<std::uint8_t,64>;
std::uint32_t hash(const std::uint8_t* p,std::size_t n,std::uint32_t h=fnv_seed) {
  for (std::size_t i=0;i<n;++i) h=(h^p[i])*16777619U;
  return h;
}
void put(Buffer& b,std::size_t at,std::uint64_t v,unsigned n) {
  for (unsigned j=0;j<n;++j) b[at+j]=static_cast<std::uint8_t>(v>>(8*j));
}
std::uint64_t get(const Buffer& b,std::size_t at,unsigned n) {
  std::uint64_t v=0; for (unsigned j=0;j<n;++j) v|=std::uint64_t(b[at+j])<<(8*j); return v;
}
bool magic(const Buffer& b,std::size_t at,const char* s) {return std::memcmp(b.data()+at,s,8)==0;}
void mark(Buffer& b,std::size_t at,const char* s) {std::memcpy(b.data()+at,s,8);}
struct Value {StableLexemeKey key; std::uint32_t count{0};};
Value value(const Buffer& b) {
  Value v; std::copy_n(b.begin(),32,v.key.bytes.begin()); v.count=static_cast<std::uint32_t>(get(b,32,4)); return v;
}
Buffer encode_value(const Value& v) {
  Buffer b{}; std::copy(v.key.bytes.begin(),v.key.bytes.end(),b.begin()); put(b,32,v.count,4); return b;
}
Buffer make_header(const StateV3Config& c,std::uint64_t generation) {
  Buffer b{}; mark(b,0,"HNSTATE3"); put(b,8,3,4); put(b,12,kStableKeyFormat,4);
  put(b,16,c.registry_namespace,8); put(b,24,c.lineage,8); put(b,32,generation,8);
  put(b,40,record_bytes,4); put(b,44,frame_bytes,4); put(b,60,hash(b.data(),60),4); return b;
}
Buffer make_seal(const StateV3Config& c,std::uint64_t gen,std::uint64_t count,std::uint64_t seq,std::uint32_t digest) {
  Buffer b{}; mark(b,0,"HNSSEAL3"); put(b,8,count,8); put(b,16,seq,8); put(b,24,digest,4);
  put(b,32,gen,8); put(b,40,c.lineage,8); put(b,52,hash(b.data(),52),4); mark(b,56,"SEALED3!"); return b;
}
Buffer make_frame(const Value& v,std::uint64_t seq) {
  auto b=encode_value(v); put(b,40,seq,8); put(b,48,hash(b.data(),48),4); mark(b,56,"TAILV3!!"); return b;
}
bool frame_ok(const Buffer& b,std::uint64_t seq) {
  return magic(b,56,"TAILV3!!") && get(b,40,8)==seq && get(b,48,4)==hash(b.data(),48) && !value(b).key.empty();
}
struct Meta {
  bool committed{false}; std::uint64_t gen{0}, keys{0}, seq{0}, tails{0}, size{0};
  std::uint32_t digest{fnv_seed};
  std::uint64_t tail_start() const {return header_bytes+keys*record_bytes;}
};
struct Transfer {Buffer b{}; std::size_t pos{0}; void reset() {pos=0; b={};}};
}
struct StateV3::Impl {
  StateFilePort& port; StateV3Config cfg;
  std::shared_ptr<const StateIoIdentity> activation;
  StateStatus state{StateStatus::Pending};
  std::array<bool,2> opened{},missing{};
  std::array<Meta,2> meta{}; unsigned active{0};
  int open_phase{-1}; unsigned open_file{0}; bool create{false};
  std::uint64_t physical_revision{1};
  struct Validator {
    unsigned file{0}; int phase{0}; Transfer x; Meta m;
    std::uint64_t index{0},seq{0}; std::uint32_t digest{fnv_seed}; StableLexemeKey previous;
    void begin(unsigned f,std::uint64_t size) {*this={}; file=f; m.size=size;}
  } validator;
  struct Lookup {
    std::uint64_t revision{0}, physical{0}; bool active{false}; int phase{0};
    StableLexemeKey key; Meta m; unsigned file{0};
    std::uint64_t lo{0},hi{0},mid{0},tail{0}; Transfer x;
    bool found{false}; std::uint32_t count{0};
  } reads[2];
  struct Writer {
    bool active{false}; int phase{0}; Transfer x,check;
    std::uint64_t offset{0},seq{0}; Value value;
  } writer;
  struct Maintenance {
    bool active{false}; int phase{0}; unsigned target{0}; Meta old;
    std::uint64_t scan{0},snap_index{0},out{0};
    StableLexemeKey previous_tail; Value tail,snap,emit;
    bool tail_valid{false},tail_done{false},snap_valid{false};
    std::uint32_t digest{fnv_seed}; Transfer x;
  } maintenance;
  explicit Impl(StateFilePort& p,StateV3Config c):port(p),cfg(c) {}
  bool config_ok() const {
    return cfg.registry_namespace && cfg.lineage && cfg.max_single_io && cfg.max_single_io<=4096 &&
        cfg.tail_threshold && cfg.max_file_bytes>=header_bytes+frame_bytes &&
        cfg.max_file_bytes<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  }
  static bool work(const StateBudget& b,StateStep& r) {
    if (!b.work_items) {r.status=StateStatus::InvalidBudget; return false;}
    ++r.cost.work_items; return true;
  }
  static bool control(const StateBudget& b,StateStep& r) {
    if (!b.control_calls) {r.needs_budget=true; return false;}
    ++r.cost.control_calls; return true;
  }
  // At most ONE syscall per advance. Charge requested, not returned, bytes.
  bool transfer(Transfer& x,unsigned f,std::uint64_t off,std::size_t len,bool write,
                const StateBudget& b,StateStep& r,bool allow_eof=false) {
    if (x.pos==len) return true;
    const auto calls=write ? b.write_calls : b.read_calls;
    const auto bytes=write ? b.write_bytes : b.read_bytes;
    if (!calls || !bytes) {r.needs_budget=true; return false;}
    const auto n=std::min({len-x.pos,bytes,cfg.max_single_io});
    if (write) {++r.cost.write_calls; r.cost.write_bytes+=n;}
    else {++r.cost.read_calls; r.cost.read_bytes+=n;}
    const auto t=write ? port.write(f,off+x.pos,x.b.data()+x.pos,n) : port.read(f,off+x.pos,x.b.data()+x.pos,n);
    if (t.bytes>n || !t.ok || (!t.bytes && !allow_eof)) {r.status=StateStatus::Unavailable; return false;}
    if (!t.bytes && allow_eof) {x.pos=len; return true;}
    x.pos+=t.bytes; return x.pos==len;
  }
  StateStep validate(const StateBudget& b) {
    StateStep r; if (!work(b,r)) return r;
    auto& v=validator;
    if (!v.m.size || v.m.size>cfg.max_file_bytes) {r.status=StateStatus::Corrupt; return r;}
    if (v.phase==0) {
      if (transfer(v.x,v.file,0,static_cast<std::size_t>(std::min<std::uint64_t>(64,v.m.size)),false,b,r)) v.phase=1;
    } else if (v.phase==1) {
      const auto& h=v.x.b;
      if (!magic(h,0,"HNSTATE3") || get(h,8,4)!=3 || get(h,12,4)!=kStableKeyFormat ||
          get(h,16,8)!=cfg.registry_namespace || get(h,24,8)!=cfg.lineage ||
          get(h,40,4)!=record_bytes || get(h,44,4)!=frame_bytes) r.status=StateStatus::Incompatible;
      else if (v.m.size<header_bytes || get(h,60,4)!=hash(h.data(),60)) r.status=StateStatus::Corrupt;
      else {v.m.gen=get(h,32,8); v.x.reset(); v.phase=2;}
    } else if (v.phase==2) {
      if (transfer(v.x,v.file,64,64,false,b,r)) v.phase=3;
    } else if (v.phase==3) {
      const auto& s=v.x.b;
      if (s==Buffer{}) {r.status=StateStatus::Ready; return r;} // Proven unsealed staging.
      if (!magic(s,0,"HNSSEAL3") || !magic(s,56,"SEALED3!") || get(s,52,4)!=hash(s.data(),52) ||
          get(s,32,8)!=v.m.gen || get(s,40,8)!=cfg.lineage || !v.m.gen) {r.status=StateStatus::Corrupt; return r;}
      v.m.committed=true; v.m.keys=get(s,8,8); v.m.seq=get(s,16,8); v.m.digest=static_cast<std::uint32_t>(get(s,24,4));
      if (v.m.keys>(cfg.max_file_bytes-header_bytes)/record_bytes || v.m.tail_start()>v.m.size) {r.status=StateStatus::Corrupt; return r;}
      if ((v.m.size-v.m.tail_start())%frame_bytes) {r.status=StateStatus::TailUncertain; return r;}
      v.m.tails=(v.m.size-v.m.tail_start())/frame_bytes; v.seq=v.m.seq;
      if (v.m.tails>std::numeric_limits<std::uint64_t>::max()-v.seq) {r.status=StateStatus::Corrupt; return r;}
      v.phase=4; v.x.reset();
    } else if (v.phase==4) {
      if (v.index==v.m.keys) {
        if (v.digest!=v.m.digest) r.status=StateStatus::Corrupt;
        else {v.index=0; v.phase=6;}
      } else if (transfer(v.x,v.file,header_bytes+v.index*record_bytes,record_bytes,false,b,r)) v.phase=5;
    } else if (v.phase==5) {
      auto val=value(v.x.b);
      if (val.key.empty() || (v.index && !(v.previous<val.key))) r.status=StateStatus::Corrupt;
      else {v.previous=val.key; v.digest=hash(v.x.b.data(),record_bytes,v.digest); ++v.index; v.x.reset(); v.phase=4;}
    } else if (v.phase==6) {
      if (v.index==v.m.tails) {r.status=StateStatus::Ready; return r;}
      if (transfer(v.x,v.file,v.m.tail_start()+v.index*frame_bytes,frame_bytes,false,b,r)) v.phase=7;
    } else {
      if (!frame_ok(v.x.b,++v.seq)) r.status=StateStatus::TailUncertain;
      else {++v.index; v.x.reset(); v.phase=6;}
    }
    return r;
  }
  void reset_lookup(Lookup& q) {
    q.physical=physical_revision; q.file=active; q.m=meta[active]; q.phase=0;
    q.lo=0; q.hi=q.m.keys; q.tail=0; q.found=false; q.count=0; q.x.reset();
  }
};

StateV3::StateV3(StateFilePort& p,StateV3Config c):impl_(new Impl(p,c)) {}
StateV3::~StateV3()=default;
StateStatus StateV3::begin_open(bool create) {
  auto& p=*impl_;
  if (!p.config_ok() || p.writer.active || p.maintenance.active || (p.open_phase>=0 && p.state==StateStatus::Pending)) return StateStatus::InvalidRequest;
  if (p.physical_revision==std::numeric_limits<std::uint64_t>::max()) return StateStatus::InvalidRequest;
  p.activation=std::make_shared<StateIoIdentity>(); ++p.physical_revision;
  p.reads[0].active=p.reads[1].active=false;
  p.create=create; p.open_phase=0; p.open_file=0; p.missing={}; p.meta={}; p.state=StateStatus::Pending;
  return p.state;
}
StateStep StateV3::advance_open(const StateBudget& b) {
  auto& p=*impl_; StateStep r; r.status=p.state;
  if (p.state!=StateStatus::Pending || p.open_phase<0) return r;
  if (p.open_phase==6) {
    r=p.validate(b);
    if (r.status==StateStatus::Ready) {
      p.meta[p.open_file]=p.validator.m;
      if (++p.open_file<2) {p.validator.begin(p.open_file,p.meta[p.open_file].size); r.status=StateStatus::Pending;}
      else {
        const auto& a=p.meta[0]; const auto& c=p.meta[1];
        if ((!a.committed && !c.committed) || (a.committed && c.committed && a.gen==c.gen)) r.status=StateStatus::Corrupt;
        else {p.active=c.committed && (!a.committed || c.gen>a.gen) ? 1 : 0; p.open_phase=-1;}
        p.state=r.status;
      }
    } else if (r.status!=StateStatus::Pending && r.status!=StateStatus::InvalidBudget) p.state=r.status;
    return r;
  }
  r.status=StateStatus::Pending; if (!Impl::work(b,r)) return r;
  const auto f=p.open_file;
  if (p.open_phase==0) {
    if (p.opened[f]) {
      if (!Impl::control(b,r)) return r;
      if (!p.port.close(f)) {p.state=r.status=StateStatus::Unavailable; return r;}
      p.opened[f]=false;
    }
    if (++p.open_file==2) {p.open_file=0; p.open_phase=1;}
  } else if (p.open_phase==1) {
    if (!Impl::control(b,r)) return r;
    const auto result=p.port.open(f,false);
    if (result==FileResult::Error) p.state=r.status=StateStatus::Unavailable;
    else {p.missing[f]=result==FileResult::Missing; p.opened[f]=!p.missing[f]; p.open_phase=2;}
  } else if (p.open_phase==2) {
    if (!p.missing[f]) {
      if (!Impl::control(b,r)) return r;
      if (!p.port.size(f,p.meta[f].size)) {p.state=r.status=StateStatus::Unavailable; return r;}
    }
    if (++p.open_file==2) {p.open_file=0; p.open_phase=3;} else p.open_phase=1;
  } else if (p.open_phase==3) {
    if (p.missing[0] && p.missing[1]) {
      if (!p.create) p.state=r.status=StateStatus::MissingStore;
      else p.open_phase=10;
    } else if (p.missing[0] || p.missing[1]) p.state=r.status=StateStatus::Corrupt;
    else {p.validator.begin(0,p.meta[0].size); p.open_phase=6;}
  } else if (p.open_phase==10) {
    if (!Impl::control(b,r)) return r;
    if (p.port.open(f,true)!=FileResult::Ok) p.state=r.status=StateStatus::Unavailable;
    else {p.opened[f]=true; p.validator.x.reset(); p.validator.x.b=make_header(p.cfg,f==0 ? 1 : 0); p.open_phase=11;}
  } else if (p.open_phase==11) {
    if (p.transfer(p.validator.x,f,0,64,true,b,r)) {p.validator.x.reset(); p.validator.x.b=f==0 ? make_seal(p.cfg,1,0,0,fnv_seed) : Buffer{}; p.open_phase=12;}
  } else if (p.open_phase==12) {
    if (p.transfer(p.validator.x,f,64,64,true,b,r)) p.open_phase=13;
  } else if (p.open_phase==13) {
    if (!Impl::control(b,r)) return r;
    if (!p.port.sync(f)) p.state=r.status=StateStatus::Unavailable;
    else {
      p.meta[f].size=128;
      if (++p.open_file==2) {p.open_file=0; p.validator.begin(0,128); p.open_phase=6;}
      else p.open_phase=10;
    }
  }
  if (r.status==StateStatus::Unavailable) p.state=r.status;
  return r;
}
StateStatus StateV3::status() const {return impl_->state;}
std::shared_ptr<const StateIoIdentity> StateV3::activation() const {return impl_->activation;}
RegistryNamespaceId StateV3::registry_namespace() const {return impl_->cfg.registry_namespace;}
bool StateV3::ready() const {return impl_->state==StateStatus::Ready;}
StateIoTicket StateV3::begin_lookup(StateReadRole role,const StableLexemeKey& key) {
  auto& p=*impl_; const auto i=static_cast<unsigned>(role);
  if (!ready() || i>1 || key.empty() || p.reads[i].revision==std::numeric_limits<std::uint64_t>::max()) return {};
  auto& q=p.reads[i]; ++q.revision; q.active=true; q.key=key; p.reset_lookup(q);
  return {p.activation,role,q.revision};
}
bool StateV3::read_is_owned(const StateIoTicket& t) const {
  const auto i=static_cast<unsigned>(t.role); const auto& p=*impl_;
  return i<2 && t.activation && t.activation==p.activation && t.revision && p.reads[i].active && p.reads[i].revision==t.revision;
}
void StateV3::cancel_read(const StateIoTicket& t) {if (read_is_owned(t)) impl_->reads[static_cast<unsigned>(t.role)].active=false;}
StateStep StateV3::advance_lookup(const StateIoTicket& t,const StateBudget& b) {
  auto& p=*impl_; StateStep r;
  if (!read_is_owned(t)) {r.status=StateStatus::Stale; return r;}
  if (!ready()) {r.status=StateStatus::Unavailable; return r;}
  if (!Impl::work(b,r)) return r;
  auto& q=p.reads[static_cast<unsigned>(t.role)];
  if (q.physical!=p.physical_revision) {p.reset_lookup(q); return r;}
  if (q.phase==0) {
    if (q.lo==q.hi) q.phase=3;
    else {q.mid=q.lo+(q.hi-q.lo)/2; q.phase=1;}
  } else if (q.phase==1) {
    if (p.transfer(q.x,q.file,header_bytes+q.mid*record_bytes,record_bytes,false,b,r)) q.phase=2;
  } else if (q.phase==2) {
    auto v=value(q.x.b);
    if (v.key==q.key) {q.found=true; q.count=v.count; q.phase=3;}
    else {if (v.key<q.key) q.lo=q.mid+1; else q.hi=q.mid; q.phase=0;}
    q.x.reset();
  } else if (q.phase==3) {
    if (q.tail==q.m.tails) {r.status=q.found ? StateStatus::Found : StateStatus::NotFound; r.count=q.count;}
    else if (p.transfer(q.x,q.file,q.m.tail_start()+q.tail*frame_bytes,frame_bytes,false,b,r)) q.phase=4;
  } else {
    if (!frame_ok(q.x.b,q.m.seq+q.tail+1)) r.status=StateStatus::TailUncertain;
    else {auto v=value(q.x.b); if (v.key==q.key) {q.found=true; q.count=v.count;} ++q.tail; q.x.reset(); q.phase=3;}
  }
  return r;
}
StateStatus StateV3::begin_write(const StableLexemeKey& key,std::uint32_t count) {
  auto& p=*impl_; const auto& m=p.meta[p.active];
  if (!ready() || key.empty() || p.writer.active || p.maintenance.active) return StateStatus::InvalidRequest;
  if (m.tails>=p.cfg.tail_threshold || m.size>p.cfg.max_file_bytes-frame_bytes) return StateStatus::MaintenanceBlocked;
  if (m.seq==std::numeric_limits<std::uint64_t>::max()-m.tails) return StateStatus::MaintenanceBlocked;
  auto& w=p.writer; w={}; w.active=true; w.value={key,count}; w.seq=m.seq+m.tails+1; w.offset=m.size;
  w.x.b=make_frame(w.value,w.seq); return StateStatus::Pending;
}
StateStep StateV3::advance_write(const StateBudget& b) {
  auto& p=*impl_; auto& w=p.writer; StateStep r;
  if (!w.active) {r.status=StateStatus::InvalidRequest; return r;}
  if (!Impl::work(b,r)) return r;
  if (w.phase==0) {
    if (p.transfer(w.x,p.active,w.offset,64,true,b,r)) w.phase=1;
    if (r.status==StateStatus::Unavailable) {w.check.reset(); w.phase=3;}
  } else if (w.phase==1) {
    if (!Impl::control(b,r)) return r;
    if (!p.port.sync(p.active)) {r.status=StateStatus::Unavailable; w.check.reset(); w.phase=3;}
    else {w.check.reset(); w.phase=2;}
  } else if (w.phase==2 || w.phase==3) {
    if (p.transfer(w.check,p.active,w.offset,64,false,b,r,w.phase==3)) {
      if (w.check.b==w.x.b) {
        if (w.phase==3) w.phase=1; // Must still sync after an uncertain write.
        else {auto& m=p.meta[p.active]; ++m.tails; m.size+=64; w.active=false; r.status=StateStatus::HostStorageConfirmed;}
      } else {w.x.pos=0; w.phase=0;} // Only this frozen writer's unconfirmed region.
    }
  }
  return r;
}
StateStatus StateV3::begin_maintenance() {
  auto& p=*impl_; const auto& old=p.meta[p.active];
  if (ready() && !old.tails && old.size>p.cfg.max_file_bytes-frame_bytes) return StateStatus::MaintenanceBlocked;
  if (!ready() || p.writer.active || p.maintenance.active) return StateStatus::InvalidRequest;
  if (old.gen==std::numeric_limits<std::uint64_t>::max() || p.physical_revision==std::numeric_limits<std::uint64_t>::max()) return StateStatus::MaintenanceBlocked;
  auto& m=p.maintenance; m={}; m.active=true; m.target=1-p.active; m.old=old; return StateStatus::Pending;
}
StateStep StateV3::advance_maintenance(const StateBudget& b) {
  auto& p=*impl_; auto& m=p.maintenance; StateStep r;
  if (!m.active) {r.status=StateStatus::InvalidRequest; return r;}
  if (m.phase==16) {
    r=p.validate(b);
    if (r.status==StateStatus::Ready) {
      if (!p.validator.m.committed) r.status=StateStatus::Corrupt;
      else {p.meta[m.target]=p.validator.m; p.active=m.target; ++p.physical_revision; m.active=false;}
    }
    return r;
  }
  if (!Impl::work(b,r)) return r;
  if (m.phase==0) {
    // Only the known inactive generation may be reused. Old active untouched.
    if (!Impl::control(b,r)) return r;
    if (!p.port.truncate(m.target,128)) r.status=StateStatus::Unavailable;
    else {m.x.reset(); m.x.b=make_header(p.cfg,m.old.gen+1); m.phase=1;}
  } else if (m.phase==1) {
    if (p.transfer(m.x,m.target,0,64,true,b,r)) {m.x.reset(); m.phase=2;}
  } else if (m.phase==2) {
    if (p.transfer(m.x,m.target,64,64,true,b,r)) {m.x.reset(); m.phase=3;}
  } else if (m.phase==3) {
    // Repeated tail minimum selection; scan ALL tail frames each round.
    if (m.scan==m.old.tails) {m.tail_done=!m.tail_valid; m.phase=6;}
    else if (p.transfer(m.x,p.active,m.old.tail_start()+m.scan*64,64,false,b,r)) m.phase=4;
  } else if (m.phase==4) {
    if (!frame_ok(m.x.b,m.old.seq+m.scan+1)) {r.status=StateStatus::TailUncertain; return r;}
    const auto v=value(m.x.b);
    if (m.previous_tail<v.key && (!m.tail_valid || !(m.tail.key<v.key))) {m.tail=v; m.tail_valid=true;}
    ++m.scan; m.x.reset(); m.phase=3;
  } else if (m.phase==6) {
    if (!m.snap_valid && m.snap_index<m.old.keys) {
      if (p.transfer(m.x,p.active,128+m.snap_index*36,36,false,b,r)) m.phase=7;
    } else m.phase=8;
  } else if (m.phase==7) {m.snap=value(m.x.b); m.snap_valid=true; m.x.reset(); m.phase=8;}
  else if (m.phase==8) {
    if (!m.snap_valid && m.tail_done) {m.phase=12; return r;}
    if (m.out>=(p.cfg.max_file_bytes-128)/36) {r.status=StateStatus::MaintenanceBlocked; return r;}
    const bool take_tail=m.tail_valid && (!m.snap_valid || !(m.snap.key<m.tail.key));
    if (take_tail) {
      m.emit=m.tail;
      if (m.snap_valid && m.snap.key==m.tail.key) {m.snap_valid=false; ++m.snap_index;}
      m.previous_tail=m.tail.key; m.tail_valid=false; m.scan=0;
    } else {m.emit=m.snap; m.snap_valid=false; ++m.snap_index;}
    m.x.reset(); m.x.b=encode_value(m.emit); m.phase=9;
  } else if (m.phase==9) {
    if (p.transfer(m.x,m.target,128+m.out*36,36,true,b,r)) m.phase=10;
  } else if (m.phase==10) {
    m.digest=hash(m.x.b.data(),36,m.digest); ++m.out; m.x.reset();
    m.phase=!m.tail_valid && !m.tail_done ? 3 : 6;
  } else if (m.phase==12) {
    // Full snapshot first; commit seal LAST. Partial seal is conservative error
    // after a process crash; the live writer can retry its exact bytes.
    if (!Impl::control(b,r)) return r;
    if (!p.port.sync(m.target)) r.status=StateStatus::Unavailable;
    else {m.x.reset(); m.x.b=make_seal(p.cfg,m.old.gen+1,m.out,m.old.seq+m.old.tails,m.digest); m.phase=13;}
  } else if (m.phase==13) {if (p.transfer(m.x,m.target,64,64,true,b,r)) m.phase=14;}
  else if (m.phase==14) {
    if (!Impl::control(b,r)) return r;
    if (!p.port.sync(m.target)) r.status=StateStatus::Unavailable;
    else {p.validator.begin(m.target,128+m.out*36); m.phase=16;}
  }
  return r;
}
StateStep StateV3::advance_close(const StateBudget& b) {
  auto& p=*impl_; StateStep r;
  if (p.writer.active || p.maintenance.active) {r.status=StateStatus::CloseUnsaved; return r;}
  if (!Impl::work(b,r)) return r;
  for (unsigned f=0;f<2;++f) if (p.opened[f]) {
    if (!Impl::control(b,r)) return r;
    if (!p.port.close(f)) r.status=StateStatus::CloseUnsaved;
    else p.opened[f]=false;
    return r;
  }
  p.reads[0].active=p.reads[1].active=false; p.state=r.status=StateStatus::ClosedSaved; return r;
}
StateFileInfo StateV3::info() const {
  const auto& p=*impl_; const auto& m=p.meta[p.active];
  return {m.gen,m.keys,m.tails,m.seq+m.tails,m.size,p.active,p.maintenance.active,p.writer.active};
}
StateV3Memory StateV3::memory_usage() const {
  return {sizeof(*this),sizeof(Impl),sizeof(impl_->reads),sizeof(Impl::Validator),sizeof(Impl::Maintenance),sizeof(Impl::Writer)};
}
} // namespace hanime
