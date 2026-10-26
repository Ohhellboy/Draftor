#include "find_session.hpp"

#include <algorithm>
#include <cstring>

namespace hanime::find {

namespace {

bool continuation(const unsigned char byte) noexcept {
  return (byte & 0xC0U) == 0x80U;
}

}  // namespace

const char* statusName(const Status status) noexcept {
  switch (status) {
    case Status::Closed: return "CLOSED";
    case Status::Editing: return "EDITING";
    case Status::Searching: return "SEARCHING";
    case Status::Match: return "MATCH";
    case Status::NotFound: return "NOT_FOUND";
    case Status::EmptyQuery: return "EMPTY_QUERY";
    case Status::QueryTooLong: return "QUERY_TOO_LONG";
    case Status::CancelledOrStale: return "CANCELLED_OR_STALE";
    case Status::AdapterFailure: return "ADAPTER_FAILURE";
  }
  return "UNKNOWN";
}

FindSession::FindSession(FindConfig config) noexcept : config_(config) {
  if (config_.query_capacity_bytes == 0U ||
      config_.query_capacity_bytes > kHardMaximumQueryBytes) {
    config_.query_capacity_bytes = kHardMaximumQueryBytes;
  }
  if (config_.default_step_work == 0U) config_.default_step_work = 1U;
  case_sensitive_ = config_.default_case_sensitive;
}

bool FindSession::validUtf8(const std::string_view text) noexcept {
  std::size_t i = 0;
  while (i < text.size()) {
    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t length = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (lead <= 0x7FU) {
      length = 1U; value = lead;
    } else if ((lead & 0xE0U) == 0xC0U) {
      length = 2U; value = lead & 0x1FU; minimum = 0x80U;
    } else if ((lead & 0xF0U) == 0xE0U) {
      length = 3U; value = lead & 0x0FU; minimum = 0x800U;
    } else if ((lead & 0xF8U) == 0xF0U) {
      length = 4U; value = lead & 0x07U; minimum = 0x10000U;
    } else {
      return false;
    }
    if (i + length > text.size()) return false;
    for (std::size_t j = 1; j < length; ++j) {
      const auto byte = static_cast<unsigned char>(text[i + j]);
      if (!continuation(byte)) return false;
      value = (value << 6U) | (byte & 0x3FU);
    }
    if ((length > 1U && value < minimum) || value > 0x10FFFFU ||
        (value >= 0xD800U && value <= 0xDFFFU)) return false;
    i += length;
  }
  return true;
}

bool FindSession::isBoundary(const std::string_view text,
                             const std::size_t offset) noexcept {
  return offset <= text.size() &&
         (offset == text.size() ||
          !continuation(static_cast<unsigned char>(text[offset])));
}

std::size_t FindSession::nextBoundary(const std::string_view text,
                                      const std::size_t offset) noexcept {
  if (offset >= text.size()) return text.size();
  std::size_t next = offset + 1U;
  while (next < text.size() &&
         continuation(static_cast<unsigned char>(text[next]))) ++next;
  return next;
}

std::size_t FindSession::previousBoundary(const std::string_view text,
                                          const std::size_t offset) noexcept {
  if (offset == 0U) return 0U;
  std::size_t previous = std::min(offset, text.size()) - 1U;
  while (previous > 0U &&
         continuation(static_cast<unsigned char>(text[previous]))) --previous;
  return previous;
}

unsigned char FindSession::folded(const unsigned char byte,
                                  const bool case_sensitive) noexcept {
  if (!case_sensitive && byte >= 'A' && byte <= 'Z') {
    return static_cast<unsigned char>(byte - 'A' + 'a');
  }
  return byte;
}

bool FindSession::bindingCurrent(const DocumentPort& port) const noexcept {
  return open_ && port.sourceStamp() == opened_stamp_;
}

bool FindSession::open(DocumentPort& port,
                       const bool body_composition_active) noexcept {
  if (body_composition_active) return false;
  if (open_) return true;
  opened_stamp_ = port.sourceStamp();
  opened_cursor_ = port.cursorByte();
  if (!isBoundary(port.text(), opened_cursor_) || !port.beginFindSession()) {
    status_ = Status::AdapterFailure;
    return false;
  }
  ++session_id_;
  if (session_id_ == 0U) ++session_id_;
  query_cursor_ = query_size_;
  match_ = {};
  task_ = {}; count_ = {}; count_complete_=false; current_index_=total_matches_=0;
  query_anchor_=uint16_t(query_cursor_);capacity_exceeded_=false;
  wrapped_ = false;
  last_match_exists_ = false;
  open_ = true;
  open_port_ = &port;
  status_ = Status::Editing;
  ++highlight_version_;
  return true;
}

bool FindSession::close(DocumentPort& port,
                        const bool find_composition_active) noexcept {
  if (!open_ || find_composition_active) return false;
  task_ = {}; count_ = {};
  port.clearMatchHighlight();
  ++highlight_version_;
  bool placed = false;
  if (bindingCurrent(port)) {
    placed = port.placeCaretAtByte(match_.valid ? match_.end : opened_cursor_);
  }
  open_ = false;
  open_port_ = nullptr;
  match_ = {};
  status_ = placed ? Status::Closed : Status::AdapterFailure;
  return placed;
}

void FindSession::cancel() noexcept {
  task_ = {}; count_ = {}; count_complete_=false; current_index_=total_matches_=0;
  match_ = {};
  wrapped_ = false;
  if (open_port_ != nullptr) open_port_->clearMatchHighlight();
  if (open_) status_ = Status::CancelledOrStale;
  ++highlight_version_;
}

void FindSession::documentSwitched(DocumentPort& port) noexcept {
  task_ = {}; count_ = {}; count_complete_=false; current_index_=total_matches_=0;
  match_ = {};
  last_match_exists_ = false;
  wrapped_ = false;
  port.clearMatchHighlight();
  ++highlight_version_;
  if (open_) {
    open_port_ = &port;
    opened_stamp_ = port.sourceStamp();
    opened_cursor_ = port.cursorByte();
    ++session_id_;
    if (session_id_ == 0U) ++session_id_;
    status_ = Status::CancelledOrStale;
  }
}

void FindSession::invalidateQueryResult(const Status new_status) noexcept {
  task_ = {}; count_ = {}; count_complete_=false; current_index_=total_matches_=0;
  match_ = {};
  wrapped_ = false;
  if (open_port_ != nullptr) open_port_->clearMatchHighlight();
  status_ = new_status;
  ++highlight_version_;
}

void FindSession::queryChanged() noexcept {
  ++query_version_;
  if (query_version_ == 0U) ++query_version_;
  query_anchor_=uint16_t(query_cursor_);capacity_exceeded_=false;
  invalidateQueryResult(query_size_?Status::Editing:Status::EmptyQuery);
}

bool FindSession::canInsertQuery(const std::string_view utf8) const noexcept {
  return open_ && !utf8.empty() && validUtf8(utf8) &&
         (query_size_-(view().selectionEnd()-view().selectionBegin())) + utf8.size() <= config_.query_capacity_bytes &&
         isBoundary({query_.data(), query_size_}, query_cursor_);
}

QueryEditResult FindSession::setQuery(const std::string_view utf8) noexcept {
  if (!open_) return QueryEditResult::NotOpen;
  if (!validUtf8(utf8)) return QueryEditResult::InvalidUtf8;
  if (utf8.size() > config_.query_capacity_bytes) {
    capacity_exceeded_=true; // Atomic refusal: query, highlight and counts remain valid.
    return QueryEditResult::CapacityExceeded;
  }
  if (utf8.size() == query_size_ &&
      (utf8.empty() || std::memcmp(query_.data(), utf8.data(), utf8.size()) == 0)) {
    return QueryEditResult::NoChange;
  }
  if (!utf8.empty()) std::memcpy(query_.data(), utf8.data(), utf8.size());
  query_size_ = utf8.size();
  query_cursor_ = query_size_;
  queryChanged();
  return QueryEditResult::Applied;
}

QueryEditResult FindSession::insertQueryUtf8(
    const std::string_view utf8) noexcept {
  if (!open_) return QueryEditResult::NotOpen;
  if (utf8.empty()) return QueryEditResult::NoChange;
  if (!validUtf8(utf8)) return QueryEditResult::InvalidUtf8;
  if ((query_size_-(view().selectionEnd()-view().selectionBegin())) + utf8.size() > config_.query_capacity_bytes) {
    capacity_exceeded_=true; // Atomic refusal: query, highlight and counts remain valid.
    return QueryEditResult::CapacityExceeded;
  }
  if(query_anchor_!=query_cursor_)eraseQuerySelection();
  std::memmove(query_.data() + query_cursor_ + utf8.size(),
               query_.data() + query_cursor_, query_size_ - query_cursor_);
  std::memcpy(query_.data() + query_cursor_, utf8.data(), utf8.size());
  query_size_ += utf8.size();
  query_cursor_ += utf8.size();
  queryChanged();
  return QueryEditResult::Applied;
}

QueryEditResult FindSession::eraseQueryBackward() noexcept {
  if (!open_) return QueryEditResult::NotOpen;
  if(query_anchor_!=query_cursor_){eraseQuerySelection();queryChanged();return QueryEditResult::Applied;}
  if (query_cursor_ == 0U) return QueryEditResult::NoChange;
  const std::string_view query(query_.data(), query_size_);
  const std::size_t begin = previousBoundary(query, query_cursor_);
  std::memmove(query_.data() + begin, query_.data() + query_cursor_,
               query_size_ - query_cursor_);
  query_size_ -= query_cursor_ - begin;
  query_cursor_ = begin;
  queryChanged();
  return QueryEditResult::Applied;
}

QueryEditResult FindSession::eraseQueryForward() noexcept {
  if (!open_) return QueryEditResult::NotOpen;
  if(query_anchor_!=query_cursor_){eraseQuerySelection();queryChanged();return QueryEditResult::Applied;}
  if (query_cursor_ >= query_size_) return QueryEditResult::NoChange;
  const std::string_view query(query_.data(), query_size_);
  const std::size_t end = nextBoundary(query, query_cursor_);
  std::memmove(query_.data() + query_cursor_, query_.data() + end,
               query_size_ - end);
  query_size_ -= end - query_cursor_;
  queryChanged();
  return QueryEditResult::Applied;
}

void FindSession::eraseQuerySelection() noexcept {
  const auto q=view();const auto begin=q.selectionBegin(),end=q.selectionEnd();
  std::memmove(query_.data()+begin,query_.data()+end,query_size_-end);
  query_size_-=end-begin;query_cursor_=query_anchor_=begin;
}

bool FindSession::moveQueryCursor(std::size_t to,bool extend) noexcept {
  if(!open_)return false;
  const auto anchor=extend?query_anchor_:uint16_t(to);
  if(to==query_cursor_&&anchor==query_anchor_)return false;
  query_cursor_=to;query_anchor_=anchor;++selection_version_;return true;
}

bool FindSession::moveQueryCursorLeft(bool extend) noexcept {
  const auto q=view();
  return moveQueryCursor(!extend&&q.query_selected?q.selectionBegin():
    query_cursor_?previousBoundary(q.query,query_cursor_):0,extend);
}

bool FindSession::moveQueryCursorRight(bool extend) noexcept {
  const auto q=view();
  return moveQueryCursor(!extend&&q.query_selected?q.selectionEnd():
    query_cursor_<query_size_?nextBoundary(q.query,query_cursor_):query_size_,extend);
}

void FindSession::setCaseSensitive(const bool enabled) noexcept {
  if (case_sensitive_ == enabled) return;
  case_sensitive_ = enabled;
  if (open_) queryChanged();
}

void FindSession::start(const Direction direction) noexcept {
  if (!open_) return;
  task_ = {}; count_ = {}; capacity_exceeded_=false;
  if (open_port_ != nullptr) open_port_->clearMatchHighlight();
  direction_ = direction;
  wrapped_ = false;
  if (query_size_ == 0U) {
    invalidateQueryResult(Status::EmptyQuery);
    return;
  }

  const bool continuing = match_.valid &&
      match_.query_version == query_version_;
  if(!continuing)count_complete_=false;
  const std::string_view source_query(query_.data(), query_size_);
  (void)source_query;
  const std::size_t text_size = 0U;  // Bound when the first step sees the port.
  const std::size_t origin = continuing
      ? match_.begin
      : (last_match_exists_ ? last_match_begin_ : opened_cursor_);

  task_.stamp = opened_stamp_;
  task_.session_id = session_id_;
  task_.query_version = query_version_;
  task_.direction = direction;
  task_.first_lo = origin;
  task_.first_hi = text_size;
  // first_hi == 0 is a sentinel; step() derives all interval bounds after it
  // has revalidated and read the current full text.
  task_.compare_offset = continuing ? 1U : 0U;
  task_.active = true;
  status_ = Status::Searching;
  match_ = {};
  ++highlight_version_;
}

bool FindSession::initializeSegment(const std::string_view text) noexcept {
  // start() stores origin in first_lo and continuing in compare_offset.
  const std::size_t origin = std::min(task_.first_lo, text.size());
  const bool continuing = task_.compare_offset == 1U;
  task_.compare_offset = 0U;
  if (task_.direction == Direction::Forward) {
    const std::size_t anchor = continuing ? nextBoundary(text, origin) : origin;
    task_.first_lo = anchor;
    task_.first_hi = text.size();
    task_.second_lo = 0U;
    task_.second_hi = anchor;
    task_.position = anchor;
  } else {
    const std::size_t anchor = origin;
    task_.first_lo = 0U;
    task_.first_hi = anchor;
    task_.second_lo = anchor;
    task_.second_hi = text.size();
    task_.position = anchor == 0U ? 0U : previousBoundary(text, anchor);
  }
  task_.candidate_active = false;
  // Change sentinel to a real, distinguishable initialized state. Empty first
  // segments are handled by advanceOrWrap().
  return advanceOrWrap(text);
}

bool FindSession::advanceCandidate(const std::string_view text) noexcept {
  const std::size_t lo = task_.second_segment ? task_.second_lo : task_.first_lo;
  const std::size_t hi = task_.second_segment ? task_.second_hi : task_.first_hi;
  if (task_.direction == Direction::Forward) {
    if (task_.position >= hi) return false;
    task_.position = nextBoundary(text, task_.position);
    return task_.position < hi;
  }
  if (hi <= lo || task_.position <= lo) return false;
  task_.position = previousBoundary(text, task_.position);
  return task_.position >= lo && task_.position < hi;
}

bool FindSession::advanceOrWrap(const std::string_view text) noexcept {
  const std::size_t lo = task_.second_segment ? task_.second_lo : task_.first_lo;
  const std::size_t hi = task_.second_segment ? task_.second_hi : task_.first_hi;
  const bool valid = hi > lo && task_.position >= lo && task_.position < hi;
  if (valid) return true;
  if (task_.second_segment) return false;
  task_.second_segment = true;
  wrapped_ = true;
  if (task_.second_hi <= task_.second_lo) return false;
  task_.position = task_.direction == Direction::Forward
      ? task_.second_lo : previousBoundary(text, task_.second_hi);
  return task_.position >= task_.second_lo &&
         task_.position < task_.second_hi;
}

void FindSession::finishNotFound(DocumentPort& port) noexcept {
  task_ = {}; count_ = {}; count_complete_=true; current_index_=total_matches_=0;
  match_ = {};
  port.clearMatchHighlight();
  status_ = Status::NotFound;
  ++highlight_version_;
}

StepResult FindSession::step(DocumentPort& port,
                             std::size_t work_budget) noexcept {
  last_step_work_ = 0U;
  if (work_budget == 0U) work_budget = config_.default_step_work;
  if (!task_.active) return count_.active?countStep(port,work_budget):StepResult::Idle;
  if (!bindingCurrent(port) || task_.stamp != port.sourceStamp() ||
      task_.session_id != session_id_ ||
      task_.query_version != query_version_) {
    task_ = {};
    match_ = {};
    port.clearMatchHighlight();
    status_ = Status::CancelledOrStale;
    ++highlight_version_;
    return StepResult::Stale;
  }
  const std::string_view text = port.text();
  // Product Document accepts only valid UTF-8. Revalidating the complete
  // 64 KiB body on every slice would defeat the bounded-work contract; the
  // full identity + text revision above is the cross-slice validity gate.

  // An uninitialized task has first_hi==0 and first_lo carrying the origin.
  if (task_.first_hi == 0U && task_.second_hi == 0U) {
    if (!initializeSegment(text)) {
      finishNotFound(port);
      return StepResult::NotFound;
    }
  }

  while (last_step_work_ < work_budget) {
    if (!advanceOrWrap(text)) {
      finishNotFound(port);
      maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
      return StepResult::NotFound;
    }

    if (!task_.candidate_active) {
      task_.candidate_active = true;
      task_.compare_offset = 0U;
    }

    bool mismatch = false;
    while (task_.compare_offset < query_size_ &&
           last_step_work_ < work_budget) {
      const std::size_t at = task_.position + task_.compare_offset;
      ++last_step_work_;
      if (at >= text.size() ||
          folded(static_cast<unsigned char>(text[at]), case_sensitive_) !=
              folded(static_cast<unsigned char>(query_[task_.compare_offset]),
                     case_sensitive_)) {
        mismatch = true;
        break;
      }
      ++task_.compare_offset;
    }

    if (!mismatch && task_.compare_offset == query_size_) {
      const std::size_t end = task_.position + query_size_;
      if (end <= text.size() && isBoundary(text, end)) {
        match_ = {task_.position, end, query_version_, true};
        last_match_begin_ = task_.position;
        last_match_exists_ = true;
        task_ = {};
        status_ = Status::Match;
        if (!port.revealMatch(match_.begin, match_.end)) {
          match_ = {};
          status_ = Status::AdapterFailure;
          ++highlight_version_;
          maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
          return StepResult::AdapterFailure;
        }
        // A full count is reusable for navigation within this exact query.
        if(count_complete_&&total_matches_&&current_index_){
          current_index_=direction_==Direction::Forward?(current_index_%total_matches_)+1:
            (current_index_==1?total_matches_:current_index_-1);
        }else{count_={};count_.active=true;count_complete_=false;current_index_=total_matches_=0;}
        ++highlight_version_;
        maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
        return StepResult::Found;
      }
      mismatch = true;
    }

    if (mismatch) {
      task_.candidate_active = false;
      task_.compare_offset = 0U;
      if (!advanceCandidate(text)) {
        // Mark the current segment exhausted; advanceOrWrap() handles the
        // single permitted wrap on the next loop iteration.
        if (task_.direction == Direction::Forward) {
          task_.position = task_.second_segment
              ? task_.second_hi : task_.first_hi;
        } else {
          if (task_.second_segment) {
            finishNotFound(port);
            maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
            return StepResult::NotFound;
          }
          task_.second_segment = true;
          wrapped_ = true;
          if (task_.second_hi <= task_.second_lo) {
            finishNotFound(port);
            maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
            return StepResult::NotFound;
          }
          task_.position = previousBoundary(text, task_.second_hi);
        }
      }
    }
  }
  maximum_step_work_ = std::max(maximum_step_work_, last_step_work_);
  return StepResult::MoreWork;
}

StepResult FindSession::countStep(DocumentPort& port,std::size_t budget) noexcept {
  if(!bindingCurrent(port)){cancel();return StepResult::Stale;}
  const auto text=port.text();
  while(count_.position<text.size()&&last_step_work_<budget){
    const auto at=count_.position+count_.compare;
    ++last_step_work_;
    const bool equal=at<text.size()&&folded(static_cast<unsigned char>(text[at]),case_sensitive_)==
      folded(static_cast<unsigned char>(query_[count_.compare]),case_sensitive_);
    if(equal&&++count_.compare<query_size_)continue;
    if(equal&&isBoundary(text,count_.position+query_size_)){
      ++count_.total;if(match_.valid&&count_.position==match_.begin)count_.index=count_.total;
    }
    count_.compare=0;count_.position=nextBoundary(text,count_.position);
  }
  maximum_step_work_=std::max(maximum_step_work_,last_step_work_);
  if(count_.position<text.size())return StepResult::MoreWork;
  total_matches_=count_.total;current_index_=count_.index;count_complete_=true;count_={};
  return StepResult::Idle;
}

ViewModel FindSession::view() const noexcept {
  return {status_, direction_, {query_.data(), query_size_}, query_cursor_,
          match_, query_version_, highlight_version_, case_sensitive_, wrapped_,
          open_, current_index_, total_matches_, selection_version_, count_complete_, query_anchor_!=query_cursor_, capacity_exceeded_, query_anchor_};
}

MemoryBudget FindSession::memoryBudget(
    const std::size_t ime_staging_bytes) const noexcept {
  return {sizeof(*this), query_.size(), ime_staging_bytes, 0U};
}

}  // namespace hanime::find
