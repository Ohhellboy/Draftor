#include "hanime/bounded_output.hpp"
#include "hanime/verified_resource.hpp"
#include "output_reuse.hpp"
#include <cstring>
#include <limits>

namespace hanime {
namespace {
bool budget_valid(const WorkBudget& b) { return b.max_read_calls && b.max_read_bytes && b.max_work_items; }
void add(WorkCost& a, const WorkCost& b) {
  a.read_calls+=b.read_calls; a.read_bytes+=b.read_bytes; a.work_items+=b.work_items;
}
OutputStatus read_status(SharedReadStatus s) {
  if (s==SharedReadStatus::Stale || s==SharedReadStatus::Cancelled) return OutputStatus::Stale;
  if (s==SharedReadStatus::CapacityExceeded) return OutputStatus::TooLarge;
  return OutputStatus::ReadError;
}
}
bool TextAccessCertificate::current(const PagedSharedResource& r,std::size_t l,std::size_t f) const {
  return ready_ && l_==l && f_==f && r.activation_is_current(activation_);
}
TextAccessStatus TextAccessCheck::begin(PagedSharedResource& r,std::size_t l,std::size_t f) {
  if (!l || !f) return TextAccessStatus::InvalidRequest;
  cancel(); resource_=&r; certificate_={}; certificate_.activation_=r.activation_handle();
  certificate_.l_=l; certificate_.f_=f; index_=0; section_=0; descriptor_pending_=reading_=false;
  result_={};
  if (!r.activation_is_current(certificate_.activation_)) result_.status=TextAccessStatus::Stale;
  return result_.status;
}
void TextAccessCheck::cancel() {
  if (resource_ && reading_) resource_->cancel_read(&cursor_);
  reading_=false; certificate_.ready_=false; result_.status=TextAccessStatus::Stale;
}
TextAccessStep TextAccessCheck::advance(const WorkBudget& b) {
  if (!budget_valid(b)) {auto s=result_; s.cost={}; s.status=TextAccessStatus::InvalidBudget; return s;}
  result_.cost={};
  if (!resource_ || !resource_->activation_is_current(certificate_.activation_)) {
    result_.status=TextAccessStatus::Stale; return result_;
  }
  if (result_.status!=TextAccessStatus::Pending) return result_;
  if (const auto* release=resource_->verified_release()) {
    result_.cost.work_items=1;
    if (certificate_.l_<release->text_capacity || certificate_.f_<release->name_capacity) {
      result_.status=TextAccessStatus::Incompatible;
      result_.required_bytes=certificate_.l_<release->text_capacity ? release->text_capacity : release->name_capacity;
    } else { certificate_.ready_=true; result_.status=TextAccessStatus::Ready; }
    return result_;
  }
  const auto& info=resource_->info();
  if (!reading_) {
    result_.cost.work_items=1;
    if (section_==3) {certificate_.ready_=true; result_.status=TextAccessStatus::Ready; return result_;}
    if (!descriptor_pending_ && index_==info.sections[section_].count) {++section_; index_=0; return result_;}
    const auto kind=descriptor_pending_ ? SharedRecordKind::StringDescriptor : static_cast<SharedRecordKind>(section_);
    const auto number=descriptor_pending_ ? descriptor_ : index_+(section_==0 ? 1U : 0U);
    result_.location_kind=static_cast<SharedRecordKind>(section_);
    result_.location_index=index_+(section_==0 ? 1U : 0U);
    if (resource_->begin_read(kind,number,&cursor_)!=SharedReadStatus::InProgress) result_.status=TextAccessStatus::ReadError;
    else reading_=true;
    return result_;
  }
  const auto s=resource_->advance_read(&cursor_,b); result_.cost=s.cost;
  if (s.status==SharedReadStatus::InProgress) return result_;
  reading_=false;
  if (s.status!=SharedReadStatus::Ready || !resource_->result_is_current(s)) {
    result_.status=s.status==SharedReadStatus::Stale ? TextAccessStatus::Stale : TextAccessStatus::ReadError; return result_;
  }
  if (descriptor_pending_ || section_==0) {
    const auto limit=section_==2 ? certificate_.f_ : certificate_.l_;
    if (s.string_descriptor.length>limit) {
      result_.status=TextAccessStatus::Incompatible; result_.required_bytes=s.string_descriptor.length; return result_;
    }
    descriptor_pending_=false; ++index_;
  } else if (section_==1) {
    descriptor_=s.relation.target_payload;
    if (descriptor_==kNoSharedString) ++index_;
    else descriptor_pending_=true;
  } else {descriptor_=s.selector.name; descriptor_pending_=true;}
  return result_;
}

bool output_has_text(OutputStatus s) {
  return s==OutputStatus::Default || s==OutputStatus::Exact || s==OutputStatus::FallbackDefault;
}
OutputStatus OutputReader::begin(PagedSharedResource& r,const SharedActivationHandle& activation,
    RuntimeLexemeRef ref,const StableLexemeKey& key,OutputRequest request,
    char* names,std::size_t f,char* destination,std::size_t l) {
  if (!names || !destination || !f || !l || request.form.size()>f ||
      (!request.form.empty() && !is_valid_utf8(request.form)) ||
      (request.invocation!=OutputInvocation::Profile && request.invocation!=OutputInvocation::Explicit &&
       request.invocation!=OutputInvocation::Expanded)) return OutputStatus::InvalidRequest;
  if (!r.activation_is_current(activation)) return OutputStatus::Stale;
  cancel(); resource_=&r; activation_=activation; ref_=ref; expected_key_=key; request_=request;
  names_=names; f_=f; destination_=destination; l_=l;
  matches_=0; relation_index_=0; payload_=kNoSharedString; length_=0;
  capacity_={};
  phase_=Phase::LexemeStart; status_=OutputStatus::Pending;
  reuse_=r.output_reuse_cache(); reuse_slot_=reuse_position_=0; reuse_revision_=0;
  if (reuse_) {++reuse_->stats.lookups; phase_=Phase::ReuseLookup;}
  return status_;
}
void OutputReader::cancel() {
  if (resource_ && reading_record_) resource_->cancel_read(&record_cursor_);
  if (resource_ && reading_text_) resource_->cancel_payload_read(&text_cursor_);
  reading_record_=reading_text_=false; phase_=Phase::Done; status_=OutputStatus::Cancelled;
  reuse_=nullptr; reuse_revision_=0;
}
OutputStep OutputReader::advance(const WorkBudget& budget) {
  auto b=budget;
  OutputStep out;
  if (!budget_valid(b)) {out.status=OutputStatus::InvalidRequest; return out;}
  if (!resource_ || !resource_->activation_is_current(activation_)) {out.status=OutputStatus::Stale; return out;}
  if (reuse_ && resource_->output_reuse_cache()!=reuse_) {
    out.status=OutputStatus::ReadError; phase_=Phase::Done; status_=out.status; reuse_=nullptr; return out;
  }
  if (phase_==Phase::Done) {
    out.status=status_; out.payload=payload_;
    if (output_has_text(status_)) out.text={destination_,length_};
    return out;
  }
  auto finish=[&](OutputStatus s) {status_=s; phase_=Phase::Done;};
  auto start_record=[&](SharedRecordKind kind,std::uint32_t index,Phase next) {
    if (resource_->begin_read(kind,index,&record_cursor_)!=SharedReadStatus::InProgress) finish(OutputStatus::ReadError);
    else {reading_record_=true; phase_=next;}
  };
  const bool reuse_step=phase_>=Phase::ReuseLookup;
  WorkCost accumulated;
  for (;;) {
#if HANIME_TEXT_CHAIN
  // Metadata tickets and cache copies need only work. A lower-level advance
  // requires all three nonzero allowances under the existing shared API.
  const bool lower=phase_==Phase::LexemeRead || phase_==Phase::RelationRead ||
      phase_==Phase::SelectorRead || phase_==Phase::NameRead || phase_==Phase::TextRead;
  if (lower && (!b.max_read_calls || !b.max_read_bytes)) break;
  bool lower_pending=false;
#endif
  out.cost={0,0,1};
  switch (phase_) {
    case Phase::ReuseLookup:
      if (reuse_slot_==reuse_->config.entries) {
        ++reuse_->stats.misses; phase_=Phase::LexemeStart;
      } else if (reuse_->candidate(reuse_slot_,ref_,expected_key_,request_,f_)) {
        reuse_revision_=reuse_->entries[reuse_slot_].revision;
        reuse_position_=0; phase_=Phase::ReuseCompare;
      } else ++reuse_slot_;
      break;
    case Phase::ReuseCompare: {
      if (!reuse_->current(reuse_slot_,reuse_revision_,true)) {finish(OutputStatus::Stale); break;}
      const auto* form=reuse_->forms.get()+reuse_slot_*reuse_->config.form_bytes;
      if (reuse_position_<request_.form.size()) {
        const auto n=std::min(request_.form.size()-reuse_position_,b.max_work_items);
        bool match=true;
        for (std::size_t i=0;i<n;++i) if (form[reuse_position_+i]!=request_.form[reuse_position_+i]) match=false;
        reuse_position_+=n; out.cost.work_items=n;
        if (!match) {++reuse_slot_; phase_=Phase::ReuseLookup;}
      } else {
        const auto& e=reuse_->entries[reuse_slot_];
        length_=e.text_size; payload_=e.payload; resolved_=e.status; reuse_position_=0;
        if (length_>l_) {capacity_={length_,false}; finish(OutputStatus::TooLarge);}
        else phase_=Phase::ReuseCopy;
      }
      break;
    }
    case Phase::ReuseCopy: {
      if (!reuse_->current(reuse_slot_,reuse_revision_,true)) {finish(OutputStatus::Stale); break;}
      const auto n=std::min(length_-reuse_position_,b.max_work_items);
      if (n) std::memcpy(destination_+reuse_position_,reuse_->text.get()+reuse_slot_*reuse_->config.text_bytes+reuse_position_,n);
      reuse_position_+=n; out.cost.work_items=n ? n : 1; reuse_->stats.copied_bytes+=n;
      if (reuse_position_==length_) {
        reuse_->touch(reuse_->entries[reuse_slot_]); ++reuse_->stats.hits; finish(resolved_);
      }
      break;
    }
    case Phase::ReuseReserve: {
      // One bounded metadata unit, at most 64 slots. Byte copies are separate.
      reuse_slot_=reuse_->reserve();
      if (reuse_slot_==reuse_->config.entries) {finish(resolved_); break;}
      auto& e=reuse_->entries[reuse_slot_];
      e.key=expected_key_; e.ref=ref_; e.invocation=request_.invocation; e.status=resolved_;
      e.payload=payload_; e.form_size=request_.form.size(); e.text_size=length_; e.resolver_capacity=f_;
      reuse_revision_=e.revision; reuse_position_=0; phase_=Phase::ReuseStore;
      break;
    }
    case Phase::ReuseStore: {
      if (!reuse_->current(reuse_slot_,reuse_revision_,false)) {finish(OutputStatus::Stale); break;}
      const bool form=reuse_position_<request_.form.size();
      const auto pos=form ? reuse_position_ : reuse_position_-request_.form.size();
      const auto total=form ? request_.form.size() : length_;
      const auto n=std::min(total-pos,b.max_work_items);
      if (n) std::memcpy((form ? reuse_->forms.get()+reuse_slot_*reuse_->config.form_bytes :
                         reuse_->text.get()+reuse_slot_*reuse_->config.text_bytes)+pos,
                        (form ? request_.form.data() : destination_)+pos,n);
      reuse_position_+=n; out.cost.work_items=n ? n : 1; reuse_->stats.stored_bytes+=n;
      if (reuse_position_==request_.form.size()+length_) {
        auto& e=reuse_->entries[reuse_slot_]; e.valid=true; reuse_->touch(e);
        ++reuse_->stats.inserts; finish(resolved_);
      }
      break;
    }
    case Phase::LexemeStart: start_record(SharedRecordKind::Lexeme,ref_,Phase::LexemeRead); break;
    case Phase::RelationStart:
      if (relation_index_==lexeme_.relation_count) phase_=Phase::Decide;
      else start_record(SharedRecordKind::Relation,lexeme_.relation_start+relation_index_,Phase::RelationRead);
      break;
    case Phase::SelectorStart: start_record(SharedRecordKind::Selector,relation_.selector,Phase::SelectorRead); break;
    case Phase::LexemeRead: case Phase::RelationRead: case Phase::SelectorRead: {
      auto s=resource_->advance_read(&record_cursor_,b); out.cost=s.cost; add(costs_.metadata,s.cost);
      if (s.status==SharedReadStatus::InProgress) {
#if HANIME_TEXT_CHAIN
        lower_pending=true;
#endif
        break;
      }
      reading_record_=false;
      if (s.status!=SharedReadStatus::Ready) {finish(read_status(s.status)); break;}
      if (!resource_->result_is_current(s)) {finish(OutputStatus::Stale); break;}
      if (phase_==Phase::LexemeRead) {
        lexeme_=s.lexeme;
        if (!(lexeme_.identity.stable_key==expected_key_)) {finish(OutputStatus::Stale); break;}
        if (request_.form.empty()) {
          payload_=lexeme_.default_payload; resolved_=OutputStatus::Default;
#if HANIME_TEXT_CHAIN
          // Keep the current record owned until the private transfer succeeds;
          // cancellation or an intervening metadata begin must invalidate it.
          reading_record_=true; phase_=Phase::DefaultTextStart;
#else
          phase_=Phase::TextStart;
#endif
        }
        else phase_=Phase::RelationStart;
      } else if (phase_==Phase::RelationRead) {relation_=s.relation; phase_=Phase::SelectorStart;}
      else {name_id_=s.selector.name; phase_=Phase::NameStart;}
      break;
    }
#if HANIME_TEXT_CHAIN
    case Phase::DefaultTextStart: {
      const auto s=resource_->begin_default_payload_read(activation_,record_cursor_,ref_,payload_,
          destination_,l_,&text_cursor_);
      if (s!=SharedReadStatus::InProgress) finish(read_status(s));
      else {reading_record_=false; reading_text_=true; phase_=Phase::TextRead;}
      break;
    }
#endif
    case Phase::NameStart: case Phase::TextStart: {
      const bool name=phase_==Phase::NameStart;
      auto s=resource_->begin_payload_read(activation_,name ? name_id_ : payload_,
          name ? names_ : destination_,name ? f_ : l_,&text_cursor_);
      if (s!=SharedReadStatus::InProgress) finish(read_status(s));
      else {reading_text_=true; phase_=name ? Phase::NameRead : Phase::TextRead;}
      break;
    }
    case Phase::NameRead: case Phase::TextRead: {
      const bool name=phase_==Phase::NameRead;
      auto s=resource_->advance_payload_read(&text_cursor_,b); out.cost=s.cost;
      add(name ? costs_.names : costs_.payload,s.cost);
      (name ? costs_.name_payload_bytes : costs_.text_payload_bytes)+=s.copied_bytes;
      if (s.status==SharedReadStatus::InProgress) {
#if HANIME_TEXT_CHAIN
        lower_pending=true;
#endif
        break;
      }
      reading_text_=false;
      if (s.status==SharedReadStatus::CapacityExceeded) {
        capacity_={s.required_bytes,name};
      }
      if (s.status!=SharedReadStatus::Ready) {finish(read_status(s.status)); break;}
      if (!resource_->text_is_current(s)) {finish(OutputStatus::Stale); break;}
      if (name) {name_length_=s.text.size(); compare_pos_=0; equal_=name_length_==request_.form.size(); phase_=Phase::Compare;}
      else {
        length_=s.text.size();
        if (reuse_ && length_<=reuse_->config.text_bytes && request_.form.size()<=reuse_->config.form_bytes)
          phase_=Phase::ReuseReserve;
        else finish(resolved_);
      }
      break;
    }
    case Phase::Compare:
      if (equal_ && compare_pos_<name_length_) {
        equal_=names_[compare_pos_]==request_.form[compare_pos_]; ++compare_pos_; ++costs_.compared_bytes;
      } else {
        if (equal_) {++matches_; chosen_=relation_;}
        if (matches_==2) finish(OutputStatus::MultipleRelations);
        else {++relation_index_; phase_=Phase::RelationStart;}
      }
      break;
    case Phase::Decide:
      if (!matches_) {payload_=lexeme_.default_payload; resolved_=OutputStatus::FallbackDefault; phase_=Phase::TextStart;}
      else if (chosen_.admission==OutputAdmission::Blocked) finish(OutputStatus::Blocked);
      else if (!(chosen_.availability & static_cast<std::uint8_t>(request_.invocation))) finish(OutputStatus::InvocationDenied);
      else if (chosen_.condition_kind!=ConditionKind::None) finish(OutputStatus::ConditionUndetermined);
      else {payload_=chosen_.target_payload; resolved_=OutputStatus::Exact; phase_=Phase::TextStart;}
      break;
    case Phase::Done: break;
  }
  add(accumulated,out.cost);
#if HANIME_TEXT_CHAIN
  if (request_.form.empty()) {
    // Only completed default-path phases may chain. A true Pending always
    // returns to the caller, even when some budget remains. Work charges bound
    // every local transition; lower calls keep their actual three-axis cost.
    if (status_!=OutputStatus::Pending || lower_pending ||
        accumulated.work_items==budget.max_work_items) break;
    b={budget.max_read_calls-accumulated.read_calls,
       budget.max_read_bytes-accumulated.read_bytes,
       budget.max_work_items-accumulated.work_items};
    continue;
  }
#endif
  // Use remaining work for RAM cache work after a completed payload advance.
  // A lookup miss may also create the normal record ticket (no I/O), but must
  // yield before advancing it. Never perform two lower-level advances here.
  const bool more_ram=phase_>=Phase::ReuseLookup || (reuse_step && phase_==Phase::LexemeStart);
  if (!more_ram || accumulated.work_items==budget.max_work_items) break;
  b.max_work_items=budget.max_work_items-accumulated.work_items;
  }
  out.cost=accumulated;
  out.status=status_; out.payload=payload_;
  if (output_has_text(status_)) out.text={destination_,length_};
  return out;
}
} // namespace hanime
