#include "hanime/base_order.hpp"
#include "base_order_wire.hpp"
#include "base_order_block.hpp"
#include <algorithm>
#include <limits>
#include <new>

#ifndef HANIME_BASE_ORDER_LOCATE_ENTRIES
#define HANIME_BASE_ORDER_LOCATE_ENTRIES 8
#endif

namespace hanime {
struct BaseOrderCursor::Impl {
  Impl()=default;
  Impl(Impl&&)=default;
  Impl& operator=(Impl&&)=default;
  ResourceReadContext context;
  std::unique_ptr<detail::BlockCursor> block;
  std::weak_ptr<const VerifiedResourceReader> reader;
  std::array<char,detail::kBaseMaxCode> code{};
  std::array<std::uint8_t,detail::kBaseMaxCode+16> scratch{};
  std::size_t code_size{0};
  std::uint32_t lo{0},hi{0},mid{0},start{0},count{0},paths{0};
  std::uint32_t member_lo{0},member_hi{0},member_mid{0},member_pos{0},item_pos{0};
  RuntimeLexemeRef member{0};
  bool located{false},missing{false},decode{false},member_started{false},member_done{false};
  bool item_started{false},cancelled{false};
  std::size_t cache_scan{0};
  bool cache_checked{false};
  // Query-owned traversal; existing scratch and Reader caches hold page bytes.
  std::uint32_t page{0}, level{0}, expected_level{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t next_page{std::numeric_limits<std::uint32_t>::max()};
  bool page_loaded{false};
  bool prefix_enabled{false}, next_covers_extensions{false};
  std::uint32_t prefix_page{0}, prefix_level{0}, prefix_work{0};
  void release() {if (auto r=reader.lock()) r->cancel_read_budgeted(context);}
  ~Impl() {release();}
};
BaseOrderCursor::BaseOrderCursor():impl_(std::make_unique<Impl>()) {}
BaseOrderCursor::~BaseOrderCursor()=default;
std::uint32_t BaseOrderCursor::count() const {return impl_->count;}
std::uint32_t BaseOrderCursor::member_position() const {return impl_->member_pos;}
std::size_t BaseOrderCursor::memory_bytes() const {return sizeof(*this)+sizeof(Impl)+(impl_->block?sizeof(detail::BlockCursor):0);}
struct BaseOrderIndex::Impl {
  struct Entry {
    std::array<char,detail::kBaseMaxCode> code{};
    std::uint32_t size{0},start{0},count{0},paths{0};
  };
  std::shared_ptr<const VerifiedResourceReader> reader;
  PagedSchemeIndex& scheme;
  PagedSharedResource& shared;
  QueryBatchContext scheme_activation;
  SharedActivationHandle shared_activation;
  std::shared_ptr<const int> identity{std::make_shared<const int>(0)};
  detail::BaseHeader header;
  std::unique_ptr<detail::BlockState> block;
  std::size_t requested_capacity{0};
  std::uint64_t revision{0};
  bool valid{false};
  std::unique_ptr<Entry[]> entries;
  BaseOrderLocateStats stats;
  std::size_t next{0};
  static void charge(WorkCost& total,const WorkCost& cost) {
    total.read_calls+=cost.read_calls;total.read_bytes+=cost.read_bytes;total.work_items+=cost.work_items;
  }
  Impl(std::shared_ptr<const VerifiedResourceReader> r,PagedSchemeIndex& s,PagedSharedResource& h)
      :reader(std::move(r)),scheme(s),shared(h),scheme_activation(s.activation_context()),shared_activation(h.activation_handle()) {}
};
BaseOrderIndex::BaseOrderIndex(std::shared_ptr<const VerifiedResourceReader> r,
    PagedSchemeIndex& scheme,PagedSharedResource& shared)
    :BaseOrderIndex(std::move(r),scheme,shared,HANIME_BASE_ORDER_LOCATE_ENTRIES) {}
BaseOrderIndex::BaseOrderIndex(std::shared_ptr<const VerifiedResourceReader> r,
    PagedSchemeIndex& scheme,PagedSharedResource& shared,std::size_t capacity)
    :impl_(std::make_unique<Impl>(std::move(r),scheme,shared)) {
  auto& p=*impl_;
  const auto* s=scheme.verified_release(); const auto* h=shared.verified_release();
  if (!p.reader || !p.reader->bootstrap_ready() || !s || !h) return;
  const auto& pin=p.reader->spec();
  if (pin.role!=ResourceReleaseRole::BaseOrder || pin.ranking_rule_version!=1 ||
      pin.required_scheme_sha256!=s->file_sha256 || pin.required_shared_sha256!=h->file_sha256 ||
      pin.scheme_id!=s->scheme_id || pin.registry_namespace!=h->registry_namespace ||
      pin.shared_bundle_id!=h->shared_bundle_id || pin.runtime_ref_count!=h->runtime_ref_count ||
      pin.text_capacity!=h->text_capacity || pin.name_capacity!=h->name_capacity) return;
  std::array<std::uint8_t,detail::kBaseHeader> bytes{};
  p.requested_capacity=capacity;
  p.stats.entry_bytes=sizeof(Impl::Entry);
  if(pin.resource_format==105) {
    p.block.reset(new(std::nothrow) detail::BlockState);
    if(p.block) {
      p.block->load_context={std::make_shared<const int>(0),1};
      p.block->stats.workspace_bytes=1024;
      p.block->stats.private_owner_bytes=sizeof(detail::BlockState);
      p.block->stats.cursor_bytes=sizeof(detail::BlockCursor);
    }
    return;
  }
  p.valid=p.reader->read(0,bytes.data(),bytes.size()) && detail::base_header(bytes.data(),p.reader->size(),p.header) &&
      p.header.format==pin.resource_format;
  p.stats.entry_bytes=sizeof(Impl::Entry);
  if(p.valid && capacity && capacity<=8) {
    p.entries.reset(new(std::nothrow) Impl::Entry[capacity]);
    if(p.entries) {p.stats.capacity=capacity;p.stats.pool_bytes=capacity*sizeof(Impl::Entry);}
  }
}
BaseOrderIndex::~BaseOrderIndex() {
  if (impl_->reader) impl_->reader->cancel_read_budgeted({impl_->identity,impl_->revision});
  if (impl_->block && impl_->reader) impl_->reader->cancel_read_budgeted(impl_->block->load_context);
}
bool BaseOrderIndex::ready() const {
  const auto& p=*impl_;
  return p.valid && p.reader->healthy() && p.scheme.activation_is_current(p.scheme_activation) &&
      p.shared.activation_is_current(p.shared_activation);
}
bool BaseOrderIndex::matches(const PagedSchemeIndex& s) const {return &s==&impl_->scheme && ready();}
bool BaseOrderIndex::matches(const PagedSharedResource& s) const {return &s==&impl_->shared && ready();}
bool BaseOrderIndex::current(const BaseOrderCursor& c) const {
  return ready() && !c.impl_->cancelled && c.impl_->context.owner==impl_->identity &&
      c.impl_->context.revision==impl_->revision;
}
BaseOrderStatus BaseOrderIndex::begin(std::string_view code,BaseOrderCursor& cursor) {
  auto& p=*impl_;
  if (!ready()) return BaseOrderStatus::Stale;
  if (code.empty() || code.size()>detail::kBaseMaxCode || !is_valid_utf8(code) ||
      p.revision==std::numeric_limits<std::uint64_t>::max()) return BaseOrderStatus::InvalidRequest;
  p.reader->cancel_read_budgeted({p.identity,p.revision}); cursor.impl_->release();
  // One owned local cursor allocation, reused by serial begins. No per-prefix
  // heap pool and no allocator churn on every input character.
  auto block_cursor=std::move(cursor.impl_->block);
  *cursor.impl_=BaseOrderCursor::Impl{};
  auto& c=*cursor.impl_; c.context={p.identity,++p.revision}; c.reader=p.reader;
  std::copy(code.begin(),code.end(),c.code.begin()); c.code_size=code.size(); c.hi=p.header.codes;
  if(p.block) {
    c.block=std::move(block_cursor);
    if(c.block)c.block->reset();
    else c.block.reset(new(std::nothrow) detail::BlockCursor);
    if(!c.block){c.cancelled=true;return BaseOrderStatus::Error;}
    c.block->hi=p.block->h[5];
  }
  return BaseOrderStatus::Pending;
}
BaseOrderStatus BaseOrderIndex::begin_prefix(std::string_view code, BaseOrderCursor& cursor,
                                             BaseOrderPrefixState& state) {
  // The checkpoint identifies this immutable index, not its latest operation.
  // Other serial queries cannot change its authenticated directory facts.
  const bool same_resource = ready() && state.identity_.lock() == impl_->identity;
  const bool extension = same_resource && state.size_ && code.size() > state.size_ &&
      std::equal(state.code_.begin(), state.code_.begin() + state.size_, code.begin());
  const auto prior_size = state.size_;
  const auto page = state.page_;
  const auto level = state.level_;
  state.reset();
  const auto status = begin(code, cursor);
  if (status != BaseOrderStatus::Pending) return status;
  auto& c = *cursor.impl_;
  c.prefix_enabled = true;
  c.prefix_work = 1; // Full bounded prefix comparison and entry copy, no I/O.
  if(impl_->block) {
    c.prefix_work=static_cast<std::uint32_t>(prior_size+code.size()+1);
    if(extension && page && page<=impl_->block->h[5]) {
      c.block->hint=page-1;c.block->hint_restart=level;c.block->phase=detail::BlockCursor::Phase::HintLower;
    }
  }
  if (extension && impl_->header.format == 2 && page) {
    c.page = c.prefix_page = page;
    c.expected_level = c.prefix_level = level;
  }
  return status;
}
BaseOrderStep BaseOrderIndex::save_prefix(const BaseOrderCursor& cursor,
                                        BaseOrderPrefixState& state) {
  return save_prefix_impl(cursor,state,false);
}
BaseOrderStep BaseOrderIndex::save_prefix_impl(const BaseOrderCursor& cursor,
                                             BaseOrderPrefixState& state, bool released) {
  state.reset();
  const auto& c = *cursor.impl_;
  const bool same_operation = ready() && c.context.owner == impl_->identity &&
      c.context.revision == impl_->revision && (!c.cancelled || released);
  if (!same_operation || !c.located || !c.prefix_enabled || (impl_->header.format != 2 && !impl_->block))
    return {BaseOrderStatus::Stale,{},{}};
  // A zero page has no useful proven subtree; it stays an empty hint.
  if (c.prefix_page) {
    state.identity_ = impl_->identity;
    state.code_ = c.code;
    state.size_ = static_cast<std::uint32_t>(c.code_size);
    state.page_ = c.prefix_page;
    state.level_ = c.prefix_level;
  }
  const WorkCost cost{0,0,1};
  Impl::charge(impl_->stats.directory, cost);
  return {BaseOrderStatus::Ready,cost,{}};
}
bool BaseOrderIndex::continue_range(BaseOrderCursor& cursor) {
  auto& p=*impl_;auto& c=*cursor.impl_;
  if(!ready() || !c.located || c.context.owner!=p.identity || c.context.revision!=p.revision ||
     p.revision==std::numeric_limits<std::uint64_t>::max())return false;
  p.reader->cancel_read_budgeted(c.context);c.context.revision=++p.revision;
  c.cancelled=false;c.item_started=false;c.decode=false;c.member_started=false;c.member_done=false;
  return true;
}
BaseOrderStep BaseOrderIndex::locate(BaseOrderCursor& cursor,const WorkBudget& b) {
  auto& c=*cursor.impl_; auto& p=*impl_;
  if (!current(cursor)) return {};
  if (!b.max_work_items || !b.max_read_calls || !b.max_read_bytes) return {BaseOrderStatus::InvalidRequest,{},{}};
  if(p.block) {
    WorkCost used;
    auto finish=[&](BaseOrderStatus status) {Impl::charge(p.stats.directory,used);return BaseOrderStep{status,used,{}};};
    const auto work=std::min<std::size_t>(c.prefix_work,b.max_work_items);
    c.prefix_work-=static_cast<std::uint32_t>(work);used.work_items+=work;
    if(c.located)return finish(c.missing?BaseOrderStatus::NotFound:BaseOrderStatus::Ready);
    if(c.block->phase==detail::BlockCursor::Phase::HintLower || c.block->phase==detail::BlockCursor::Phase::HintUpper) {
      if(!detail::block_prepare_hint(*p.block,*c.block,{c.code.data(),c.code_size},b,used))return finish(BaseOrderStatus::Pending);
      if(c.block->hinted){c.prefix_page=c.block->block+1;c.prefix_level=c.block->hint_restart;}
    }
    while(used.work_items<b.max_work_items && !c.cache_checked) {
      if(c.cache_scan<p.stats.resident) {
        const auto& e=p.entries[c.cache_scan++];++used.work_items;
        if(e.size==c.code_size && std::equal(c.code.begin(),c.code.begin()+c.code_size,e.code.begin())) {
          if(!e.count || e.start>p.header.groups || e.count>p.header.groups-e.start || e.paths<e.count) {
            p.valid=false;return finish(BaseOrderStatus::Error);
          }
          c.located=true;c.start=e.start;c.count=e.count;c.paths=e.paths;++p.stats.hits;
          return finish(BaseOrderStatus::Ready);
        }
      } else {c.cache_checked=true;++p.stats.misses;}
    }
    if(used.work_items==b.max_work_items)return finish(BaseOrderStatus::Pending);
    const auto step=detail::block_locate(*p.block,*c.block,*p.reader,c.context,
        {c.code.data(),c.code_size},detail::block_remaining(b,used));
    Impl::charge(used,step.cost);
    if(step.status==BaseOrderStatus::Error) {
      p.valid=false;p.block->init=detail::BlockState::Init::Error;
      p.reader->cancel_read_budgeted(c.context);
    }
    if(step.status==BaseOrderStatus::Ready || step.status==BaseOrderStatus::NotFound) {
      c.located=true;c.missing=step.status==BaseOrderStatus::NotFound;
      if(c.block->block!=detail::kBlockNone) {
        c.prefix_page=c.block->block+1;c.prefix_level=c.block->restart;
      }
      if(!c.missing) {
        c.start=c.block->start;c.count=c.block->count;c.paths=c.block->paths;
        if(p.stats.capacity) {
          auto& e=p.entries[p.next];
          if(p.stats.resident==p.stats.capacity)++p.stats.evictions;else ++p.stats.resident;
          e.code=c.code;e.size=static_cast<std::uint32_t>(c.code_size);e.start=c.start;e.count=c.count;e.paths=c.paths;
          p.next=(p.next+1)%p.stats.capacity;
        }
      }
    }
    return finish(step.status);
  }
  if (c.prefix_work) {
    --c.prefix_work;
    const WorkCost cost{0,0,1};
    Impl::charge(p.stats.directory,cost);
    return {BaseOrderStatus::Pending,cost,{}};
  }
  if (c.located) return {c.missing ? BaseOrderStatus::NotFound : BaseOrderStatus::Ready,{},{}};
  const WorkCost local{0,0,1};
  if(!c.cache_checked) {
    if(c.cache_scan<p.stats.resident) {
      const auto& e=p.entries[c.cache_scan++];
      Impl::charge(p.stats.directory,local);
      if(e.size==c.code_size && std::equal(c.code.begin(),c.code.begin()+c.code_size,e.code.begin())) {
        // A fact is not a ticket: current() above checks the new query lineage.
        if(!e.count || e.start>p.header.groups || e.count>p.header.groups-e.start || e.paths<e.count) {
          p.valid=false;return {BaseOrderStatus::Error,local,{}};
        }
        c.located=true;c.start=e.start;c.count=e.count;c.paths=e.paths;
        ++p.stats.hits;return {BaseOrderStatus::Ready,local,{}};
      }
      return {BaseOrderStatus::Pending,local,{}};
    }
    c.cache_checked=true;++p.stats.misses;
  }
  if (p.header.format==2 && !c.page_loaded) {
    if (!c.decode) {
      const auto step=p.reader->read_budgeted(1024+std::uint64_t(c.page)*1024,
          c.scratch.data(),16,b,c.context);
      Impl::charge(p.stats.directory,step.cost);
      if (step.status==ResourceReadStatus::Error) {
        p.valid=false;
        return {BaseOrderStatus::Error,step.cost,{}};
      }
      if (step.status==ResourceReadStatus::Ready) c.decode=true;
      return {BaseOrderStatus::Pending,step.cost,{}};
    }
    c.decode=false;
    Impl::charge(p.stats.directory,local);
    c.level=detail::bo32(c.scratch.data());
    const auto entries=detail::bo32(c.scratch.data()+4);
    const bool wrong_level=c.expected_level!=std::numeric_limits<std::uint32_t>::max() &&
        c.level!=c.expected_level;
    if (c.level>31 || wrong_level || !entries || entries>(1024-16)/p.header.width ||
        detail::bo64(c.scratch.data()+8)) {
      p.valid=false;
      return {BaseOrderStatus::Error,local,{}};
    }
    c.lo=0;
    c.hi=entries;
    c.page_loaded=true;
    c.next_page=std::numeric_limits<std::uint32_t>::max();
    return {BaseOrderStatus::Pending,local,{}};
  }
  if (p.header.format==2 && c.lo==c.hi && c.level &&
      c.next_page!=std::numeric_limits<std::uint32_t>::max()) {
    // Descend only after authenticated comparison. New begin, cancellation and
    // activation retirement invalidate this query-owned traversal as before.
    if (c.prefix_enabled && c.next_covers_extensions) {
      c.prefix_page = c.next_page;
      c.prefix_level = c.level - 1;
    }
    c.page=c.next_page;
    c.expected_level=c.level-1;
    c.page_loaded=false;
    Impl::charge(p.stats.directory,local);
    return {BaseOrderStatus::Pending,local,{}};
  }
  if (c.lo==c.hi) {Impl::charge(p.stats.directory,local);c.located=true; c.missing=true; return {BaseOrderStatus::NotFound,local,{}};}
  if (!c.decode) {
    c.mid=c.lo+(c.hi-c.lo)/2;
    const auto offset=p.header.format==2 ?
        1024+std::uint64_t(c.page)*1024+16+std::uint64_t(c.mid)*p.header.width :
        detail::kBaseHeader+std::uint64_t(c.mid)*p.header.width;
    auto step=p.reader->read_budgeted(offset,
        c.scratch.data(),p.header.width,b,c.context);
    Impl::charge(p.stats.directory,step.cost);
    if (step.status==ResourceReadStatus::Error) {p.valid=false; return {BaseOrderStatus::Error,step.cost,{}};}
    if (step.status==ResourceReadStatus::Ready) c.decode=true;
    return {BaseOrderStatus::Pending,step.cost,{}};
  }
  c.decode=false;
  Impl::charge(p.stats.directory,local);
  const auto n=detail::bo32(c.scratch.data());
  const auto start=detail::bo32(c.scratch.data()+4), count=detail::bo32(c.scratch.data()+8), paths=detail::bo32(c.scratch.data()+12);
  const bool branch=p.header.format==2 && c.level;
  const bool invalid_record=branch ?
      (count || paths || start<=c.page || start>=p.header.directory_pages) :
      (!count || start>p.header.groups || count>p.header.groups-start || paths<count);
  if (!n || n>p.header.max_code || invalid_record) {
    p.valid=false; return {BaseOrderStatus::Error,local,{}};
  }
  const std::string_view found(reinterpret_cast<const char*>(c.scratch.data()+16),n);
  const int cmp=found.compare({c.code.data(),c.code_size});
  if (branch) {
    if (cmp<0) c.lo=c.mid+1;
    else {
      c.hi=c.mid;c.next_page=start;
      if (c.prefix_enabled) {
        // This is provisional until lower_bound finishes. A fence sharing all
        // of A is insufficient: extensions can cross it despite an exact miss.
        c.next_covers_extensions = cmp > 0 &&
            found.substr(0,c.code_size) != std::string_view(c.code.data(),c.code_size);
        c.prefix_work = 1;
      }
    }
    return {BaseOrderStatus::Pending,local,{}};
  }
  if (cmp<0) c.lo=c.mid+1;
  else if (cmp>0) c.hi=c.mid;
  else {
    c.located=true;c.start=start;c.count=count;c.paths=paths;
    if(p.stats.capacity) {
      auto& e=p.entries[p.next];
      if(p.stats.resident==p.stats.capacity)++p.stats.evictions;
      else ++p.stats.resident;
      e.code=c.code;e.size=static_cast<std::uint32_t>(c.code_size);e.start=start;e.count=count;e.paths=paths;
      p.next=(p.next+1)%p.stats.capacity;
    }
    return {BaseOrderStatus::Ready,local,{}};
  }
  return {BaseOrderStatus::Pending,local,{}};
}
BaseOrderStep BaseOrderIndex::read_item(BaseOrderCursor& cursor,std::uint32_t position,const WorkBudget& b) {
  auto& c=*cursor.impl_; auto& p=*impl_;
  if (!current(cursor)) return {};
  if (!b.max_work_items || !b.max_read_calls || !b.max_read_bytes || !c.located || c.missing || position>=c.count)
    return {BaseOrderStatus::InvalidRequest,{},{}};
  if (!c.item_started) {c.item_started=true; c.item_pos=position; c.decode=false;}
  if (c.item_pos!=position) return {BaseOrderStatus::InvalidRequest,{},{}};
  if (!c.decode) {
    const auto step=p.reader->read_budgeted(p.header.rank_offset+std::uint64_t(c.start+position)*detail::kBaseItem,
        c.scratch.data(),detail::kBaseItem,b,c.context);
    Impl::charge(p.stats.items,step.cost);
    if (step.status==ResourceReadStatus::Error) {p.valid=false; return {BaseOrderStatus::Error,step.cost,{}};}
    if (step.status==ResourceReadStatus::Ready) c.decode=true;
    return {BaseOrderStatus::Pending,step.cost,{}};
  }
  c.item_started=false; c.decode=false; RankItem item;
  Impl::charge(p.stats.items,{0,0,1});
  if (!detail::base_item(c.scratch.data(),p.reader->spec().runtime_ref_count,item)) {
    p.valid=false; return {BaseOrderStatus::Error,{0,0,1},{}};
  }
  return {BaseOrderStatus::Ready,{0,0,1},item};
}
BaseOrderStep BaseOrderIndex::find_member(BaseOrderCursor& cursor,RuntimeLexemeRef ref,const WorkBudget& b) {
  auto& c=*cursor.impl_; auto& p=*impl_;
  if (!current(cursor)) return {};
  if (!c.located) return {BaseOrderStatus::InvalidRequest,{},{}};
  if (c.missing) return {BaseOrderStatus::NotFound,{},{}};
  if (!b.max_work_items || !b.max_read_calls || !b.max_read_bytes || !ref) return {BaseOrderStatus::InvalidRequest,{},{}};
  if (!c.member_started) {c.member=ref; c.member_hi=c.count; c.member_started=true; c.decode=false;}
  if (ref!=c.member) return {BaseOrderStatus::InvalidRequest,{},{}};
  if (c.member_done) {
    auto result=read_item(cursor,c.member_pos,b);
    if (result.status==BaseOrderStatus::Ready && result.item.ref!=ref) {p.valid=false;result.status=BaseOrderStatus::Error;}
    return result;
  }
  if (c.member_lo==c.member_hi) {Impl::charge(p.stats.members,{0,0,1});return {BaseOrderStatus::NotFound,{0,0,1},{}};}
  if (!c.decode) {
    c.member_mid=c.member_lo+(c.member_hi-c.member_lo)/2;
    const auto step=p.reader->read_budgeted(p.header.member_offset+std::uint64_t(c.start+c.member_mid)*detail::kBaseMember,
        c.scratch.data(),detail::kBaseMember,b,c.context);
    Impl::charge(p.stats.members,step.cost);
    if (step.status==ResourceReadStatus::Error) {p.valid=false;return {BaseOrderStatus::Error,step.cost,{}};}
    if (step.status==ResourceReadStatus::Ready) c.decode=true;
    return {BaseOrderStatus::Pending,step.cost,{}};
  }
  c.decode=false; const auto found=detail::bo32(c.scratch.data());
  Impl::charge(p.stats.members,{0,0,1});
  if (found<ref) c.member_lo=c.member_mid+1;
  else if (found>ref) c.member_hi=c.member_mid;
  else {
    c.member_pos=detail::bo32(c.scratch.data()+4);
    if (c.member_pos>=c.count) {p.valid=false;return {BaseOrderStatus::Error,{0,0,1},{}};}
    c.member_done=true;
  }
  return {BaseOrderStatus::Pending,{0,0,1},{}};
}
void BaseOrderIndex::cancel(BaseOrderCursor& cursor) {
  auto& c=*cursor.impl_;
  if (c.context.owner!=impl_->identity || c.context.revision!=impl_->revision) return;
  impl_->reader->cancel_read_budgeted(c.context); c.cancelled=true;
}
std::size_t BaseOrderIndex::memory_bytes() const {return sizeof(*this)+sizeof(Impl)+impl_->stats.pool_bytes+(impl_->block?sizeof(detail::BlockState)+(impl_->block->nav?impl_->block->stats.nav_bytes:0):0);}
BaseOrderLocateStats BaseOrderIndex::locate_stats() const {return impl_->stats;}

namespace detail {
bool BaseOrderBlockAccess::is_block(const BaseOrderIndex& index) {return index.impl_->reader && index.impl_->reader->spec().resource_format==105;}
bool BaseOrderBlockAccess::cursor_descriptor(const BaseOrderIndex& index,const BaseOrderCursor& cursor,
    std::uint32_t& start,std::uint32_t& count,std::uint32_t& paths) {
  if(!index.current(cursor) || !cursor.impl_->located || cursor.impl_->missing)return false;
  start=cursor.impl_->start;count=cursor.impl_->count;paths=cursor.impl_->paths;return true;
}
BaseOrderStep BaseOrderBlockAccess::advance_initialization(BaseOrderIndex& index,const WorkBudget& b,
    void* (*allocate)(std::size_t),void (*release)(void*)) {
  auto& p=*index.impl_;
  if(!p.block)return {BaseOrderStatus::Error,{},{}};
  if(!p.reader->healthy() || !p.scheme.activation_is_current(p.scheme_activation) ||
     !p.shared.activation_is_current(p.shared_activation)) {
    cancel_initialization(index);return {BaseOrderStatus::Stale,{},{}};
  }
  // Once initialized, any later query/rank/member failure is permanent.
  // Re-entering this private loader cannot regrant a revoked index.
  if(p.block->init==BlockState::Init::Ready && !p.valid) {
    p.block->init=BlockState::Init::Error;
    p.reader->cancel_read_budgeted({p.identity,p.revision});
    return {BaseOrderStatus::Error,{},{}};
  }
  const auto out=p.block->advance(*p.reader,b,allocate,release);
  if(out.status==BaseOrderStatus::Ready && !p.valid) {
    const auto& h=p.block->h;
    p.header={0,h[2],h[3],h[4],h[18],h[19],h[20],105,h[5]};
    if(p.requested_capacity && p.requested_capacity<=8) {
      p.entries.reset(new(std::nothrow) BaseOrderIndex::Impl::Entry[p.requested_capacity]);
      if(p.entries){p.stats.capacity=p.requested_capacity;p.stats.pool_bytes=p.requested_capacity*sizeof(BaseOrderIndex::Impl::Entry);}
    }
    p.valid=true;
  }
  return out;
}
void BaseOrderBlockAccess::cancel_initialization(BaseOrderIndex& index) {
  auto& p=*index.impl_;if(!p.block)return;
  p.reader->cancel_read_budgeted(p.block->load_context);p.reader->cancel_read_budgeted({p.identity,p.revision});
  p.valid=false;p.block->init=BlockState::Init::Cancelled;p.block->stats.cancelled=true;p.block->clear_nav();
}
BlockInitializationStats BaseOrderBlockAccess::initialization_stats(const BaseOrderIndex& index) {
  return index.impl_->block?index.impl_->block->stats:BlockInitializationStats{};
}
}
} // namespace hanime
