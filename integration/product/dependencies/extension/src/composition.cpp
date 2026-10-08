#include "hanime_extension/extension.hpp"
#include "fixed_member_array.hpp"
#ifndef HANIME_EXTENSION_NO_PLANNER
#include "segment_planner.hpp"
#endif
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hanime_extension {
using namespace hanime;
namespace {
bool same_view(const StateView& a, const StateView& b) {
  return a.owner && a.owner == b.owner && a.registry_namespace == b.registry_namespace &&
         a.count_revision == b.count_revision && a.availability_revision == b.availability_revision;
}
bool sane(const Config& c) {
  return c.raw_bytes && c.raw_bytes <= 256 && c.members && c.members <= 64 &&
    c.page_items && c.page_items <= 64 && c.raw_batch && c.raw_batch <= 64 &&
    c.word_bytes && c.word_bytes <= 4096 && c.name_bytes && c.name_bytes <= 512 &&
    c.text_bytes && c.text_bytes <= 16384 && c.request_bytes && c.request_bytes <= 4096 &&
    c.planning.ranges && c.planning.ranges <= 256 && c.planning.span_syllables && c.planning.span_syllables <= 256 &&
    c.planning.core_work && c.planning.read_bytes && c.planning.local_work;
}
Status rank_status(RankStatus s) {
  switch (s) {
    case RankStatus::Ready: return Status::PageReady;
    case RankStatus::InProgress: case RankStatus::WaitingState: return Status::Working;
    case RankStatus::NoMatches: return Status::NoMappingForPrefix;
    case RankStatus::RangeExhausted: return Status::RangeExhausted;
    case RankStatus::Stale: return Status::Stale;
    case RankStatus::Cancelled: return Status::Cancelled;
    case RankStatus::InvalidBudget: return Status::InvalidBudget;
    case RankStatus::CapacityExceeded: return Status::CapacityExceeded;
    case RankStatus::InvalidRequest: return Status::InvalidRequest;
    case RankStatus::Busy: return Status::Busy;
    default: return Status::ResourceError;
  }
}
}

DisabledCountSource::DisabledCountSource(RegistryNamespaceId ns) {
  view_.owner = std::make_shared<CountOwnerIdentity>(); view_.registry_namespace = ns;
}
StateView DisabledCountSource::view() const { return view_; }
bool DisabledCountSource::view_is_current(const StateView& v) const { return same_view(v, view_); }
CountTicket DisabledCountSource::begin_count(const StateView& v, const StableLexemeKey&) {
  ++begins_;
  if (!view_is_current(v) || sequence_ == std::numeric_limits<std::uint64_t>::max()) return {};
  active_ = ++sequence_; return {view_, active_};
}
CountResult DisabledCountSource::advance_count(const CountTicket& t, const WorkBudget&) {
  CountResult r; r.ticket = t;
  if (active_ && t.revision == active_ && view_is_current(t.view)) r.status = CountStatus::Unavailable;
  return r;
}
void DisabledCountSource::cancel_count(const CountTicket& t) {
  if (t.revision == active_ && same_view(t.view, view_)) active_ = 0;
}

struct Composition::Impl {
  struct Data {
    std::vector<char> raw, text, forms;
    detail::FixedMemberArray<Member> members;
    std::size_t n{0}, count{0}, text_n{0}, form_n{0};
    Data(const Config& c, StorageRole role, StorageBackend backend) : raw(c.raw_bytes + 1), text(c.text_bytes),
      forms(c.request_bytes), members(c.members, role, backend) {}
  };
  enum class Task { None, Planning, Probe, Rank, CopyPage, SelectOutput, TextOutput,
                    RecheckHint, RecheckMap, RecheckKey, RecheckOutput, CheckPack, Pack };
  PagedSchemeIndex& scheme;
  PagedSharedResource& shared;
  SchemeRules rules;
  Config cfg;
  DisabledCountSource counts;
  std::shared_ptr<const OwnerIdentity> owner{std::make_shared<OwnerIdentity>()};
  SharedActivationHandle activation;
  std::uint64_t scheme_epoch{0}, input{0}, revision{0}, op_sequence{0}, page_sequence{0};
  std::string scheme_id;
  Data a, b;
  Data* live{&a}; Data* stage{&b};
  std::vector<RankItem> work, result, page_items, page_staging;
  detail::FixedMemberArray<Member> plan;
  std::vector<std::size_t> sources;
  std::vector<char> names, output, request;
  std::unique_ptr<ExactRanker> ranker;
  std::unique_ptr<ExactQueryCursor> probe;
#ifndef HANIME_EXTENSION_NO_PLANNER
  std::unique_ptr<SegmentPlanner> planner;
  InputStamp plan_stamp;
  std::uint64_t plan_sequence{0};
#endif
  RankTicket rank_ticket;
  RankBoundary first, last;
  OutputReader reader;
  SharedReadCursor member_cursor;
  RuntimeLexemeRef member_ref{0};
  bool member_hint_matches{false};
  Task task{Task::None};
  Status last_status{Status::Ready};
  OutputStatus output_status{OutputStatus::Cancelled};
  std::size_t qbegin{0}, qend{0}, page_size{0}, copy_index{0}, plan_size{0};
  std::size_t recheck_index{0}, pack_index{0}, pack_char{0}, pack_phase{0};
  std::size_t request_size{0}, output_size{0}, command_bytes{0};
  OutputInvocation invocation{OutputInvocation::Profile};
  Member pending;
  bool page_more{false}, page_valid{false}, rechecking{false};
  bool submission_locked{false}, submission_unknown{false};
  bool busy() const { return task != Task::None || submission_locked || submission_unknown; }

  Impl(PagedSchemeIndex& s, PagedSharedResource& h, SchemeRules r, Config c, StorageBackend backend)
    : scheme(s), shared(h), rules(r), cfg(c), counts(h.info().registry_namespace),
      activation(h.activation_handle()), scheme_epoch(s.index_epoch()), scheme_id(s.info().scheme_id),
      a(c,StorageRole::CompositionMembersA,backend), b(c,StorageRole::CompositionMembersB,backend),
      work(c.page_items), result(c.page_items), page_items(c.page_items), page_staging(c.page_items),
      plan(c.members,StorageRole::CompositionPlan,backend), sources(c.members),
      names(c.name_bytes), output(c.word_bytes), request(c.name_bytes) {}
  ~Impl() { cleanup(); }
  bool current() const {
    return shared.activation_is_current(activation) && scheme.ready() &&
      scheme.index_epoch() == scheme_epoch && scheme.info().registry_namespace == activation.registry_namespace() &&
      scheme.info().required_shared_bundle_id == activation.shared_bundle_id();
  }
  std::size_t focus() const {
    std::size_t p = 0;
    for (std::size_t i = 0; i < live->count; ++i) {
      const auto& m = live->members[i];
      if (!m.valid || m.begin != p) break;
      p = m.end;
    }
    return p;
  }
  void cleanup() {
#ifndef HANIME_EXTENSION_NO_PLANNER
    if (planner) planner->cancel();
#endif
    reader.cancel();
    shared.cancel_read(&member_cursor);
    if (probe) { scheme.cancel_query(probe.get()); probe.reset(); }
    if (ranker) { ranker->abandon_range(rank_ticket); ranker.reset(); }
    rank_ticket = {}; first = {}; last = {};
  }
  void clear_page() { page_valid = false; page_size = 0; first = {}; last = {}; }
  Status finish(Status s, bool keep_rank = false) {
#ifndef HANIME_EXTENSION_NO_PLANNER
    if (planner && (s==Status::Stale || s==Status::ResourceError)) planner->invalidate();
    else if (planner && s==Status::Cancelled) planner->retire();
#endif
    reader.cancel();
    // Retained pages need both a live ranking lifetime and no outstanding
    // result borrow, including empty ranges. Release only our original ticket.
    if (keep_rank && s != Status::Stale && s != Status::Cancelled && page_valid &&
        current() && ranker && ranker->boundary_is_current(first) && ranker->boundary_is_current(last)) {
      ranker->release(rank_ticket);
    } else {
      clear_page(); cleanup();
    }
    task = Task::None; last_status = s; return s;
  }
  Status fail_output(Status s) {
    // Output/packing has not published stage: a valid page can still be used
    // for another choice. Revalidation has its own query and no retained page.
    const bool keep = task == Task::TextOutput || task == Task::SelectOutput ||
      (!rechecking && (task == Task::CheckPack || task == Task::Pack));
    return finish(s, keep);
  }
  bool begin_operation() {
    if (op_sequence == std::numeric_limits<std::uint64_t>::max()) return false;
    ++op_sequence; last_status = Status::Working; command_bytes = 0; return true;
  }
  void copy_state() {
    std::copy(live->raw.begin(), live->raw.end(), stage->raw.begin());
    std::copy(live->text.begin(), live->text.end(), stage->text.begin());
    std::copy(live->forms.begin(), live->forms.end(), stage->forms.begin());
    std::copy(live->members.begin(), live->members.end(), stage->members.begin());
    stage->n = live->n; stage->count = live->count;
    stage->text_n = live->text_n; stage->form_n = live->form_n;
    command_bytes += live->raw.size() + live->text.size() + live->forms.size() + live->members.size()*sizeof(Member);
  }
  void publish_state() { std::swap(live, stage); ++revision; clear_page(); }
  bool endpoint(std::size_t end) const {
    const auto p = focus();
    if (rules==SchemeRules::DirectExact) return p==0 && end==live->n && end>0;
    return end > p && end <= live->n && (rules != SchemeRules::R1Xiaohe || (end-p)%2 == 0);
  }
  Status begin_rank(std::size_t begin, std::size_t end, Task next) {
    cleanup(); qbegin = begin; qend = end;
    ranker = std::make_unique<ExactRanker>(scheme, shared, counts, work.data(), work.size(), cfg.raw_batch, cfg.raw_bytes);
    auto s = ranker->begin_input(std::string(live->raw.data()+begin, end-begin), activation.mount_epoch(), {}, ScoringMode::BaseOnly);
    if (s != RankStatus::InProgress) return finish(rank_status(s));
    s = ranker->request_range(RangeDirection::First, nullptr, cfg.page_items, result.data(), result.size(), &rank_ticket);
    if (s != RankStatus::InProgress) return finish(rank_status(s));
    task = next; return Status::Working;
  }
  Status begin_probe() {
    cleanup();
    probe = std::make_unique<ExactQueryCursor>();
    std::string error;
    if (!scheme.begin_exact_query(std::string(live->raw.data()+qbegin, qend-qbegin), activation.mount_epoch(), probe.get(), &error))
      return finish(Status::ResourceError);
    task = Task::Probe; return Status::Working;
  }
  bool capture_request(OutputRequest r) {
    if (r.form.size() > request.size()) return false;
    if (!r.form.empty()) std::copy(r.form.begin(), r.form.end(), request.begin());
    request_size = r.form.size(); invocation = r.invocation; command_bytes += r.form.size(); return true;
  }
  Status start_output(Task next) {
    reader.cancel(); output_size = 0;
    output_status = reader.begin(shared, activation, pending.ref, pending.key,
      {std::string_view(request.data(), request_size), invocation}, names.data(), names.size(), output.data(), output.size());
    task = next;
    if (output_status != OutputStatus::Pending)
      return fail_output(output_status==OutputStatus::Stale?Status::Stale:Status::InvalidRequest);
    return Status::Working;
  }
  Status start_recheck() {
    if (recheck_index == live->count) {
      publish_state(); return finish(Status::Ready);
    }
    pending = live->members[recheck_index];
    if (pending.begin >= pending.end || pending.end > live->n) return finish(Status::Unresolved);
    if (!capture_request({std::string_view(live->forms.data()+pending.form_begin,pending.form_size),pending.invocation}))
      return finish(Status::CapacityExceeded);
    // Membership does not require a ranked position. Every member is checked,
    // including those marked valid; an old ref is only a lookup hint.
    cleanup(); qbegin=pending.begin; qend=pending.end; member_hint_matches=false;
    auto status=begin_probe();
    if (status!=Status::Working) return status;
    task=Task::RecheckMap;
    if (pending.ref && pending.ref<=shared.info().sections[0].count) {
      if (shared.begin_read(SharedRecordKind::Lexeme,pending.ref,&member_cursor)!=SharedReadStatus::InProgress)
        return finish(Status::ResourceError);
      task=Task::RecheckHint;
    }
    return Status::Working;
  }
  Status prepare_pack() {
    pending.text_size = output_size; pending.form_size = request_size; pending.invocation = invocation; pending.valid = true;
    if (rechecking) { plan_size = 1; plan[0] = pending; sources[0] = cfg.members; }
    else {
      stage->count = 0; stage->text_n = 0; stage->form_n = 0;
    }
    if (stage->count + plan_size > cfg.members) return fail_output(Status::CapacityExceeded);
    pack_index=0; pack_char=stage->text_n; pack_phase=stage->form_n;
    task=Task::CheckPack; return Status::Working;
  }
  Status check_pack(std::size_t budget, std::size_t& used) {
    while (pack_index<plan_size && used<budget) {
      if (sources[pack_index]==cfg.members) plan[pack_index]=pending;
      pack_char+=plan[pack_index].text_size; pack_phase+=plan[pack_index].form_size;
      ++pack_index; ++used;
      if (pack_char>cfg.text_bytes || pack_phase>cfg.request_bytes) return fail_output(Status::CapacityExceeded);
    }
    if (pack_index==plan_size) { pack_index=0; pack_char=0; pack_phase=0; task=Task::Pack; }
    return Status::Working;
  }
  Status pack(std::size_t budget, std::size_t& used) {
    while (used < budget && pack_index < plan_size) {
      auto& m = plan[pack_index]; const auto src = sources[pack_index];
      if (pack_phase == 0) {
        Member d=m; d.text_begin=stage->text_n; d.form_begin=stage->form_n;
        stage->members[stage->count]=d; pack_phase=1; pack_char=0; ++used;
      } else if (pack_phase == 1) {
        if (pack_char < m.text_size) {
          const char* from = src==cfg.members ? output.data() : live->text.data()+m.text_begin;
          stage->text[stage->text_n++] = from[pack_char++]; ++used;
        } else { pack_phase=2; pack_char=0; }
      } else {
        if (pack_char < m.form_size) {
          const char* from = src==cfg.members ? request.data() : live->forms.data()+m.form_begin;
          stage->forms[stage->form_n++] = from[pack_char++]; ++used;
        } else { ++stage->count; ++pack_index; pack_phase=0; }
      }
    }
    if (pack_index != plan_size) return Status::Working;
    if (rechecking) { ++recheck_index; return start_recheck(); }
    publish_state(); return finish(Status::Ready);
  }
};

Composition::Composition(PagedSchemeIndex& s, PagedSharedResource& h, SchemeRules rules, Config c)
  :Composition(s,h,rules,c,StorageBackend{}) {}
Composition::Composition(PagedSchemeIndex& s, PagedSharedResource& h, SchemeRules rules, Config c, StorageBackend backend)
  :Composition(s,h,spelling_policy(rules),c,backend) {}
Composition::Composition(PagedSchemeIndex& s, PagedSharedResource& h, SchemePolicy policy, Config c, StorageBackend backend) {
  if (!sane(c)) throw std::invalid_argument("Invalid bounded extension configuration");
  if (policy.resource_scheme.empty() ||
      (policy.rules!=SchemeRules::FullPinyin && policy.rules!=SchemeRules::R1Xiaohe && policy.rules!=SchemeRules::DirectExact) ||
      (policy.rules!=SchemeRules::DirectExact && policy.resource_scheme!=spelling_policy(policy.rules).resource_scheme) ||
      (s.ready() && s.info().scheme_id != policy.resource_scheme))
    throw std::invalid_argument("Scheme rules do not match resource scheme");
  detail::validate_storage_backend(backend);
  p_ = std::make_unique<Impl>(s,h,policy.rules,c,backend);
}
Composition::~Composition() = default;
std::array<StorageAllocation,3> Composition::storage_usage() const noexcept {
  return {p_->a.members.allocation(),p_->b.members.allocation(),p_->plan.allocation()};
}
bool Composition::begin_submission() {
  if (p_->busy()) return false;
  p_->cleanup(); p_->clear_page(); p_->submission_locked=true; return true;
}
void Composition::end_submission(bool unknown) {
  p_->submission_locked=false; p_->submission_unknown=p_->submission_unknown||unknown;
}
bool Composition::valid() const noexcept { return p_->current(); }
RawInput Composition::raw_input() const {
  RawInput r;const auto& p=*p_;
  r.raw={p.live->raw.data(),p.live->n};r.rules=p.rules;
  r.stamp.owner_=p.owner;r.stamp.input_=p.input;r.stamp.revision_=p.revision;r.stamp.epoch_=p.scheme_epoch;
  return r;
}
bool Composition::input_is_current(const InputStamp& s) const {
  const auto& p=*p_;
  return s.owner_.lock()==p.owner&&s.input_==p.input&&s.revision_==p.revision&&
    s.epoch_==p.scheme_epoch&&p.current();
}
Snapshot Composition::snapshot() const {
  const auto& d=*p_->live; Snapshot s;
  s.input_id=p_->input; s.revision=p_->revision; s.raw={d.raw.data(),d.n}; s.selected_text={d.text.data(),d.text_n};
  s.output_forms={d.forms.data(),d.form_n}; s.activation=p_->activation; s.scheme_epoch=p_->scheme_epoch;
  s.members=d.members.data(); s.member_count=d.count; s.focus=p_->focus();
  if (d.n) {
    s.state = s.focus==d.n ? CompositionState::AllSelected : CompositionState::PartialSelected;
    for (std::size_t i=0;i<d.count;++i) if (!d.members[i].valid || !p_->current()) s.state=CompositionState::NeedsRecheck;
  }
  return s;
}
Page Composition::page() const {
  if (!p_->page_valid || !p_->current()) return {};
  return {p_->page_items.data(),p_->page_size,p_->qbegin,p_->qend,p_->page_more,p_->page_sequence};
}
Selection Composition::selection(std::size_t slot) const {
  Selection h; if (!page().items || slot>=p_->page_size) return h;
  h.owner_=p_->owner; h.input_=p_->input; h.revision_=p_->revision; h.page_=p_->page_sequence;
  h.begin_=p_->qbegin; h.end_=p_->qend; h.item_=p_->page_items[slot]; return h;
}
Status Composition::status() const { return p_->last_status; }
Operation Composition::operation() const { Operation o; o.owner_=p_->owner; o.sequence_=p_->op_sequence; return o; }

Status Composition::edit_raw(std::size_t begin, std::size_t end, std::string_view replacement) {
  auto& p=*p_; const auto& d=*p.live;
  if (p.busy()) return Status::Busy;
  if (begin>end || end>d.n) return Status::InvalidRequest;
  if (begin==end && replacement.empty()) return Status::Ready;
  if (replacement.size()>p.cfg.raw_bytes-(d.n-(end-begin))) return Status::CapacityExceeded;
  for (char ch:replacement) if (ch<'a'||ch>'z') return Status::InvalidRequest;
  if (p.revision==UINT64_MAX || (!d.n && p.input==UINT64_MAX)) return Status::CapacityExceeded;
  // Copy first: replacement is permitted to borrow the old raw buffer.
  p.command_bytes=0; p.copy_state(); auto& n=*p.stage;
  const auto tail=d.n-end;
  if (!replacement.empty()) std::copy(replacement.begin(),replacement.end(),n.raw.begin()+static_cast<std::ptrdiff_t>(begin));
  std::copy_n(d.raw.data()+end,tail,n.raw.data()+begin+replacement.size());
  n.n=d.n-(end-begin)+replacement.size(); n.raw[n.n]=0;
  const auto delta=static_cast<std::ptrdiff_t>(replacement.size())-static_cast<std::ptrdiff_t>(end-begin);
  n.count=0;
  // An exact-code selection covers the entire old code. Even an append at
  // its endpoint invalidates that member; spelling schemes retain prefixes.
  if(p.rules!=SchemeRules::DirectExact) for (std::size_t i=0;i<d.count;++i) {
    auto m=d.members[i];
    const bool overlaps=begin==end ? m.begin<begin && begin<m.end : m.begin<end && m.end>begin;
    if (overlaps) continue;
    if (m.begin>=end) { m.begin=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(m.begin)+delta);
      m.end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(m.end)+delta); m.valid=false; }
    n.members[n.count++]=m;
  }
  // Text/form pools are repacked on the next selection/revalidation. Expose only
  // current member slices; stale pool tails never count as selected text.
  n.text_n=0;
  for (std::size_t i=0;i<n.count;++i) {
    auto& m=n.members[i];
    std::copy_n(d.text.data()+m.text_begin,m.text_size,n.text.data()+n.text_n);
    m.text_begin=n.text_n; n.text_n+=m.text_size;
  }
  p.command_bytes+=replacement.size()+tail+d.count*sizeof(Member)+n.text_n;
  p.cleanup(); if (!d.n && n.n) ++p.input;
  p.publish_state(); return p.finish(Status::Ready);
}
Status Composition::choose_prefix(std::size_t end) {
  auto& p=*p_;
  if (p.busy()) return Status::Busy;
  if (!p.current()) return Status::Stale;
  if (!p.endpoint(end)) return Status::InvalidRequest;
#ifndef HANIME_EXTENSION_NO_PLANNER
  // Retire the old report and operation. Completed facts are rechecked by
  // SegmentPlanner::begin against owner-local input, raw, resources and context.
  if (p.planner) p.planner->retire();
#endif
  p.clear_page(); if (!p.begin_operation()) return Status::CapacityExceeded;
  try { return p.begin_rank(p.focus(),end,Impl::Task::Rank); }
  catch (const std::bad_alloc&) { return p.finish(Status::CapacityExceeded); }
}
Status Composition::query_longest() {
  if (p_->rules==SchemeRules::DirectExact) return choose_prefix(p_->live->n);
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.current()) return Status::Stale;
#ifndef HANIME_EXTENSION_NO_PLANNER
  if (p.planner) p.planner->invalidate();
#endif
  p.qbegin=p.focus(); p.qend=p.live->n;
  if (p.rules==SchemeRules::R1Xiaohe) p.qend-=(p.qend-p.qbegin)%2;
  if (p.qend==p.qbegin) return Status::NoPrefixMatch;
  p.clear_page(); if (!p.begin_operation()) return Status::CapacityExceeded;
  try { return p.begin_probe(); } catch (const std::bad_alloc&) { return p.finish(Status::CapacityExceeded); }
}
Status Composition::query_next(OutputRequest request,std::uint64_t context) {
  if (p_->rules==SchemeRules::DirectExact) {
    if (request.form.size()>p_->cfg.name_bytes) return Status::CapacityExceeded;
    if (request.invocation!=OutputInvocation::Profile&&request.invocation!=OutputInvocation::Explicit)
      return Status::InvalidRequest;
    return choose_prefix(p_->live->n);
  }
#ifdef HANIME_EXTENSION_NO_PLANNER
  (void)request;(void)context;return Status::Unresolved;
#else
  auto& p=*p_;
  if (p.busy()) return Status::Busy;
  if (!p.current()) return Status::Stale;
  if (p.focus()==p.live->n) return Status::InvalidRequest;
  if (request.form.size()>p.cfg.name_bytes) return Status::CapacityExceeded;
  if (request.invocation!=OutputInvocation::Profile&&request.invocation!=OutputInvocation::Explicit)
    return Status::InvalidRequest;
  p.cleanup();p.clear_page();
  if (!p.begin_operation()) return Status::CapacityExceeded;
  try {
    if (!p.planner) p.planner=std::make_unique<SegmentPlanner>(p.scheme,p.shared,p.counts,p.cfg);
    const auto input=raw_input();
    auto status=p.planner->begin(input,p.focus(),request,context);
    p.plan_stamp=input.stamp;p.plan_sequence=p.op_sequence;p.command_bytes+=p.planner->command_work();
    if (status!=Status::Working) return p.finish(status);
    p.task=Impl::Task::Planning;return status;
  } catch (const std::bad_alloc&) { return p.finish(Status::CapacityExceeded); }
#endif
}
PlanningReport Composition::planning_report() const {
#ifndef HANIME_EXTENSION_NO_PLANNER
  if (p_->planner&&input_is_current(p_->plan_stamp)) {
    auto report=p_->planner->report();report.source=p_->plan_stamp;report.generation=p_->plan_sequence;return report;
  }
#endif
  return {};
}
bool Composition::plan_is_current(const PlanningReport& report) const {
  const auto current=planning_report();
  return report.valid&&current.valid&&input_is_current(report.source)&&report.generation==current.generation&&
    report.context_revision==current.context_revision&&report.policy==current.policy;
}
PlanningMemory Composition::planning_memory_usage() const {
#ifndef HANIME_EXTENSION_NO_PLANNER
  if (p_->planner) return p_->planner->memory();
#endif
  return {};
}
Status Composition::drop_planning_cache() {
  if (p_->busy()) return Status::Busy;
#ifndef HANIME_EXTENSION_NO_PLANNER
  p_->planner.reset();p_->plan_stamp={};
#endif
  return Status::Ready;
}
Status Composition::request_page(RangeDirection direction) {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.current() || !p.page_valid || !p.ranker) return Status::Stale;
  const RankBoundary* b=direction==RangeDirection::After?&p.last:direction==RangeDirection::Before?&p.first:nullptr;
  if (b && !p.ranker->boundary_is_current(*b)) return Status::Stale;
  if (!p.begin_operation()) return Status::CapacityExceeded;
  auto s=p.ranker->request_range(direction,b,p.cfg.page_items,p.result.data(),p.result.size(),&p.rank_ticket);
  if (s!=RankStatus::InProgress) return p.finish(rank_status(s),true);
  p.task=Impl::Task::Rank; return Status::Working;
}

Status Composition::read_text(const Selection& h, OutputRequest r) {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.current() || h.owner_!=p.owner || !p.page_valid || h.input_!=p.input || h.revision_!=p.revision || h.page_!=p.page_sequence)
    return Status::Stale;
  if (!p.capture_request(r)) return Status::CapacityExceeded;
  if (!p.begin_operation()) return Status::CapacityExceeded;
  p.pending={}; p.pending.key=h.item_.key; p.pending.ref=h.item_.ref;
  return p.start_output(Impl::Task::TextOutput);
}
Status Composition::select(const Selection& h, OutputRequest r) {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.current() || h.owner_!=p.owner || !p.page_valid || h.input_!=p.input || h.revision_!=p.revision || h.page_!=p.page_sequence || h.begin_!=p.focus())
    return Status::Stale;
  if (p.revision==UINT64_MAX) return Status::CapacityExceeded;
  if (!p.capture_request(r) || r.form.size()>p.cfg.request_bytes) return Status::CapacityExceeded;
  p.plan_size=0; bool inserted=false;
  auto add=[&](Member m,std::size_t src) {
    if (p.plan_size==p.cfg.members) return false;
    p.plan[p.plan_size]=m; p.sources[p.plan_size++]=src; return true;
  };
  Member chosen; chosen.key=h.item_.key; chosen.ref=h.item_.ref; chosen.begin=h.begin_; chosen.end=h.end_;
  for (std::size_t i=0;i<p.live->count;++i) {
    auto m=p.live->members[i];
    if (!inserted && m.begin>=chosen.begin) { if (!add(chosen,p.cfg.members)) return Status::CapacityExceeded; inserted=true; }
    if (m.begin<chosen.end && m.end>chosen.begin) continue;
    if (!add(m,i)) return Status::CapacityExceeded;
  }
  if (!inserted && !add(chosen,p.cfg.members)) return Status::CapacityExceeded;
  if (!p.begin_operation()) return Status::CapacityExceeded;
  p.copy_state(); p.pending=chosen; p.rechecking=false;
  return p.start_output(Impl::Task::SelectOutput);
}
std::string_view Composition::last_text() const { return {p_->output.data(),p_->output_size}; }

Status Composition::retract_last() {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.live->count) return Status::InvalidRequest;
  if (p.revision==UINT64_MAX) return Status::CapacityExceeded;
  p.command_bytes=0; p.copy_state(); --p.stage->count;
  p.stage->text_n=p.live->members[p.stage->count].text_begin;
  p.cleanup(); p.publish_state(); return p.finish(Status::Ready);
}
Status Composition::reopen_segment(std::size_t i) {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (i>=p.live->count) return Status::InvalidRequest;
  if (p.revision==UINT64_MAX) return Status::CapacityExceeded;
  p.command_bytes=0; p.copy_state();
  for (std::size_t j=i;j<p.stage->count;++j) p.stage->members[j].valid=false;
  p.cleanup(); p.publish_state(); return p.finish(Status::Ready);
}
Status Composition::revalidate() {
  auto& p=*p_; if (p.busy()) return Status::Busy;
  if (!p.current()) return Status::Stale;
  if (!p.live->count) return Status::Ready;
  if (p.revision==UINT64_MAX || !p.begin_operation()) return Status::CapacityExceeded;
  p.clear_page(); p.copy_state(); p.stage->count=0; p.stage->text_n=0; p.stage->form_n=0;
  p.recheck_index=0; p.rechecking=true;
  try { return p.start_recheck(); } catch (const std::bad_alloc&) { return p.finish(Status::CapacityExceeded); }
}
Status Composition::rebind() {
  auto& p=*p_;
  if (p.submission_locked || p.submission_unknown) return Status::Busy;
  if (!p.shared.ready() || !p.scheme.ready()) return Status::Stale;
  auto a=p.shared.activation_handle();
  if (a.registry_namespace()!=p.activation.registry_namespace() || p.scheme.info().scheme_id!=p.scheme_id ||
      p.scheme.info().required_shared_bundle_id!=a.shared_bundle_id() || p.scheme.info().registry_namespace!=a.registry_namespace())
    return Status::InvalidRequest;
  if (p.revision==UINT64_MAX) return Status::CapacityExceeded;
  p.cleanup(); p.task=Impl::Task::None; p.command_bytes=0; p.copy_state();
  for (std::size_t i=0;i<p.stage->count;++i) p.stage->members[i].valid=false;
  p.activation=a; p.scheme_epoch=p.scheme.index_epoch(); p.publish_state(); return p.finish(Status::Ready);
}
Status Composition::cancel(const Operation& o) {
  auto& p=*p_; if (o.owner_!=p.owner || o.sequence_!=p.op_sequence) return Status::Stale;
  if (p.task==Impl::Task::None) return p.last_status;
  p.clear_page(); return p.finish(Status::Cancelled);
}
Status Composition::cancel_input(std::uint64_t id) {
  auto& p=*p_; if (id!=p.input) return Status::Stale;
  if (p.submission_locked || p.submission_unknown) return Status::Busy;
  if (p.revision==UINT64_MAX) return Status::CapacityExceeded;
  p.cleanup(); p.clear_page(); p.live->n=p.live->count=p.live->text_n=p.live->form_n=0;
  p.live->raw[0]=0; ++p.revision; return p.finish(Status::Cancelled);
}
Status Composition::confirm_composition() {
  if (p_->busy()) return Status::Busy;
  if (!p_->current()) return Status::Stale;
  if (snapshot().state!=CompositionState::AllSelected) return Status::InvalidRequest;
  return Status::CompositionCommitPending;
}

Step Composition::tick(const Operation& o, const WorkBudget& budget) {
  auto& p=*p_; Step r;
  if (o.owner_!=p.owner || o.sequence_!=p.op_sequence) { r.status=Status::Stale; return r; }
  if (p.task==Impl::Task::None) { r.status=p.last_status; return r; }
  if (!budget.max_read_calls || !budget.max_read_bytes || !budget.max_work_items) { r.status=Status::InvalidBudget; return r; }
  if (!p.current()) { p.clear_page(); r.status=p.finish(Status::Stale); return r; }
  r.status=Status::Working;
  try {
    if (p.task==Impl::Task::Planning) {
#ifndef HANIME_EXTENSION_NO_PLANNER
      r=p.planner->advance(budget);
      if (r.status==Status::Ready) {
        const auto report=p.planner->report();
        r.status=p.begin_rank(report.begin,report.end,Impl::Task::Rank);
      } else if (r.status!=Status::Working&&r.status!=Status::InvalidBudget) r.status=p.finish(r.status);
#else
      r.status=p.finish(Status::Unresolved);
#endif
    } else if (p.task==Impl::Task::Probe) {
      auto s=p.scheme.advance_exact_query(p.probe.get(),budget,1); r.core_cost=s.cost;
      if (!s.batch.hits.empty()) {
        if (!p.scheme.batch_is_current(s.batch)) r.status=p.finish(Status::Stale);
        else r.status=p.begin_rank(p.qbegin,p.qend,Impl::Task::Rank);
      } else if (s.status==ExactQueryStepStatus::NoResults || s.status==ExactQueryStepStatus::Complete) {
        const auto step=p.rules==SchemeRules::R1Xiaohe?2U:1U;
        if (p.qend-p.qbegin<=step) r.status=p.finish(Status::NoPrefixMatch);
        else { p.qend-=step; r.status=p.begin_probe(); }
      } else if (s.status==ExactQueryStepStatus::Stale) r.status=p.finish(Status::Stale);
      else if (s.status==ExactQueryStepStatus::Cancelled) r.status=p.finish(Status::Cancelled);
      else if (s.status==ExactQueryStepStatus::Error) r.status=p.finish(Status::ResourceError);
      else if (s.status==ExactQueryStepStatus::InvalidBudget) r.status=Status::InvalidBudget;
    } else if (p.task==Impl::Task::Rank) {
      auto s=p.ranker->advance(p.rank_ticket,budget); r.core_cost=s.cost;
      if (s.status==RankStatus::Ready) {
        p.copy_index=0; p.task=Impl::Task::CopyPage;
      } else if (s.status!=RankStatus::InProgress && s.status!=RankStatus::WaitingState) {
        auto state=rank_status(s.status);
#ifndef HANIME_EXTENSION_NO_PLANNER
        if(s.status==RankStatus::NoMatches&&p.planner) {
          const auto report=p.planner->report();
          p.planner->accept_direct_fact(p.qbegin,p.qend,nullptr);
          if(report.valid&&report.direct_range)state=(report.incomplete_tail||report.unknown_tail)?Status::Unresolved:Status::NoPrefixMatch;
        }
#endif
        r.status=p.finish(state,state==Status::RangeExhausted);
      }
    } else if (p.task==Impl::Task::RecheckMap || p.task==Impl::Task::RecheckHint || p.task==Impl::Task::RecheckKey) {
      // A superseded raw query cannot authorize even a cached hint or output.
      if (!p.probe || !p.scheme.query_is_current(p.scheme.query_context(*p.probe))) {
        r.status=p.finish(Status::Stale);
      } else if (p.task==Impl::Task::RecheckMap) {
        auto s=p.scheme.advance_exact_query(p.probe.get(),budget,1); r.core_cost=s.cost;
        if (!s.batch.hits.empty()) {
          if (!p.scheme.batch_is_current(s.batch)) r.status=p.finish(Status::Stale);
          else {
            p.member_ref=s.batch.hits[0].runtime_ref;
            if (p.member_hint_matches) {
              if (p.member_ref==p.pending.ref) {
                p.cleanup(); r.status=p.start_output(Impl::Task::RecheckOutput);
              }
            } else {
              const auto state=p.shared.begin_read(SharedRecordKind::Lexeme,p.member_ref,&p.member_cursor);
              if (state!=SharedReadStatus::InProgress) r.status=p.finish(Status::ResourceError);
              else p.task=Impl::Task::RecheckKey;
            }
          }
        } else if (s.status==ExactQueryStepStatus::NoResults || s.status==ExactQueryStepStatus::Complete) {
          r.status=p.finish(Status::Unresolved);
        } else if (s.status==ExactQueryStepStatus::Stale) r.status=p.finish(Status::Stale);
        else if (s.status==ExactQueryStepStatus::Cancelled) r.status=p.finish(Status::Cancelled);
        else if (s.status==ExactQueryStepStatus::InvalidBudget) r.status=Status::InvalidBudget;
        else if (s.status==ExactQueryStepStatus::Error) r.status=p.finish(Status::ResourceError);
      } else {
        const bool hint=p.task==Impl::Task::RecheckHint;
        auto s=p.shared.advance_read(&p.member_cursor,budget); r.core_cost=s.cost;
        if (s.status==SharedReadStatus::Ready) {
          const bool matches=s.lexeme.identity.stable_key==p.pending.key;
          p.shared.cancel_read(&p.member_cursor);
          if (hint) { p.member_hint_matches=matches; p.task=Impl::Task::RecheckMap; }
          else if (matches) {
            p.pending.ref=p.member_ref; p.cleanup(); r.status=p.start_output(Impl::Task::RecheckOutput);
          } else p.task=Impl::Task::RecheckMap;
        } else if (s.status==SharedReadStatus::Stale) r.status=p.finish(Status::Stale);
        else if (s.status==SharedReadStatus::Cancelled) r.status=p.finish(Status::Cancelled);
        else if (s.status==SharedReadStatus::InvalidBudget) r.status=Status::InvalidBudget;
        else if (s.status!=SharedReadStatus::InProgress) r.status=p.finish(Status::ResourceError);
      }
    } else if (p.task==Impl::Task::CopyPage) {
      if (!p.ranker->range_is_current(p.rank_ticket)) r.status=p.finish(Status::Stale);
      else {
        auto v=p.ranker->result(p.rank_ticket);
        while (p.copy_index<v.size && r.local_work<budget.max_work_items) {
          p.page_staging[p.copy_index]=v.items[p.copy_index]; ++p.copy_index; ++r.local_work;
        }
        if (p.copy_index==v.size) {
          if (p.page_sequence==UINT64_MAX) r.status=p.finish(Status::CapacityExceeded);
          else {
            #ifndef HANIME_EXTENSION_NO_PLANNER
            if(p.planner&&v.size)p.planner->accept_direct_fact(p.qbegin,p.qend,&v.items[0]);
#endif
            p.first=p.ranker->boundary(p.rank_ticket,0); p.last=p.ranker->boundary(p.rank_ticket,v.size-1);
            p.page_items.swap(p.page_staging);
            p.page_size=v.size; p.page_more=v.has_more; p.page_valid=true; ++p.page_sequence;
            r.status=p.finish(Status::PageReady,true);
          }
        }
      }
    } else if (p.task==Impl::Task::CheckPack) r.status=p.check_pack(budget.max_work_items,r.local_work);
    else if (p.task==Impl::Task::Pack) r.status=p.pack(budget.max_work_items,r.local_work);
    else {
      const auto kind=p.task;
      auto s=p.reader.advance(budget); r.core_cost=s.cost; p.output_status=s.status; r.output=s.status;
      if (output_has_text(s.status)) {
        p.output_size=s.text.size(); p.reader.cancel();
        if (kind==Impl::Task::TextOutput) r.status=p.finish(Status::Ready,true);
        else r.status=p.prepare_pack();
      } else if (s.status!=OutputStatus::Pending) {
        const auto status=s.status==OutputStatus::Stale?Status::Stale:s.status==OutputStatus::TooLarge?Status::CapacityExceeded:
          s.status==OutputStatus::ReadError?Status::ResourceError:Status::OutputRestricted;
        r.status=p.fail_output(status);
      }
    }
  } catch (const std::bad_alloc&) { r.status=p.fail_output(Status::CapacityExceeded); }
  return r;
}

MemoryUsage Composition::memory_usage() const {
  const auto& p=*p_; MemoryUsage m; m.owner_bytes=sizeof(*this); m.impl_bytes=sizeof(Impl);
  for (const auto* d : {&p.a,&p.b}) m.composition_buffers+=d->raw.capacity()+d->text.capacity()+d->forms.capacity()+d->members.capacity()*sizeof(Member);
  m.composition_buffers+=p.plan.capacity()*sizeof(Member)+p.sources.capacity()*sizeof(std::size_t);
  m.rank_arrays=(p.work.capacity()+p.result.capacity())*sizeof(RankItem);
  m.page_array=(p.page_items.capacity()+p.page_staging.capacity())*sizeof(RankItem);
  m.output_buffers=p.names.capacity()+p.output.capacity()+p.request.capacity();
  if (p.ranker) m.rank=p.ranker->memory_usage();
  if (p.probe) m.raw_query=p.probe->memory_usage();
  return m;
}
std::size_t Composition::command_work() const { return p_->command_bytes; }
std::size_t Composition::count_begin_calls() const { return p_->counts.begin_calls(); }
const char* name(Status s) {
  switch(s) {
#define EX_NAME(x) case Status::x: return #x
    EX_NAME(Ready); EX_NAME(Working); EX_NAME(PageReady); EX_NAME(NoMappingForPrefix); EX_NAME(NoPrefixMatch);
    EX_NAME(RangeExhausted); EX_NAME(CapacityExceeded); EX_NAME(InvalidRequest); EX_NAME(InvalidBudget); EX_NAME(Busy);
    EX_NAME(Stale); EX_NAME(ResourceError); EX_NAME(OutputRestricted); EX_NAME(Unresolved); EX_NAME(Cancelled);
    EX_NAME(CompositionCommitPending);
#undef EX_NAME
  }
  return "InvalidStatus";
}
} // namespace hanime_extension
