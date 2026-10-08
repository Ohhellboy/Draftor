#include "draftor/vertical_layout.hpp"
#include "utf8.hpp"
#include "body_caret.hpp"

#include <algorithm>
#include <limits>

namespace draftor::vertical {
namespace {

bool before(const Caret& a, const Caret& b) noexcept {
  return a.column < b.column || (a.column == b.column && a.y_px < b.y_px);
}

unsigned distance(const std::uint16_t a, const std::uint16_t b) noexcept {
  return a > b ? a - b : b - a;
}

bool sameIdentityAndGeometry(const Stamp& a, const Stamp& b) noexcept {
  return a.document_id == b.document_id && a.session == b.session &&
         a.mount_epoch == b.mount_epoch && a.geometry_revision == b.geometry_revision &&
         a.literal_revision == b.literal_revision;
}

}  // namespace

void Viewport::invalidate() noexcept {
  status_ = Status::Stale;
  count_ = 0;
  anchors_.fill({});
  next_anchor_ = 0;
}

Status Viewport::status(const Stamp& current) const noexcept {
  return current == stamp_ ? status_ : Status::Stale;
}

Event Viewport::eventAt(const std::size_t index) const noexcept {
  return index < count_ ? events_[(head_ + index) % event_capacity] : Event{};
}

void Viewport::remember(const Anchor anchor) noexcept {
  for (auto& existing : anchors_) {
    if (existing.valid && existing.column == anchor.column) {
      existing = anchor;
      return;
    }
  }
  anchors_[next_anchor_] = anchor;
  next_anchor_ = (next_anchor_ + 1U) % anchor_capacity;
}

Status Viewport::start(const Input& input, const Config config,
                       const Position focus, const bool unchanged_prefix,
                       const std::size_t edit_begin) noexcept {
  // A width-only change does not change vertical flow. Reuse the bounded
  // events and scanner continuation, including an in-progress long Latin word.
  const bool same_text=input.stamp.text_revision==stamp_.text_revision&&
      input.stamp.document_id==stamp_.document_id&&input.stamp.session==stamp_.session&&
      input.stamp.mount_epoch==stamp_.mount_epoch&&input.stamp.literal_revision==stamp_.literal_revision;
  const bool same_wrap=config.body.height==config_.body.height&&
      config.column_width_px==config_.column_width_px&&config.column_gap_px==config_.column_gap_px&&
      config.upright_advance_px==config_.upright_advance_px&&config.literal_advance_px==config_.literal_advance_px&&
      config.literal_advance==config_.literal_advance&&config.metrics_context==config_.metrics_context;
  if(same_text&&same_wrap&&status_==Status::Ready&&validGeometry(config)){
    const auto found=locateUnchecked(focus);
    const auto visible=(unsigned(config.body.width)+config.column_gap_px)/(unsigned(config.column_width_px)+config.column_gap_px);
    auto first=first_column_;
    if(found.status==Status::Ready){
      if(found.caret.column<first)first=found.caret.column;
      if(found.caret.column-first>=visible)first=found.caret.column+1-visible;
      if(count_&&eventAt(0).column<=first){
        config_=config;stamp_=input.stamp;focus_=focus;first_column_=first;visible_columns_=visible;
        flow_.config_=config;flow_.stamp_=input.stamp;focus_found_=true;
        status_=document_end_||eventAt(count_-1).column>=first+visible?Status::Ready:Status::MoreWork;
        return status_;
      }
    }
  }
  Anchor resume{};
  const bool same_flow = same_text&&same_wrap;
  const bool prefix_reusable = unchanged_prefix && input.literal_count == 0 &&
      sameIdentityAndGeometry(input.stamp, stamp_);
  if (same_flow || prefix_reusable) {
    // Retain a preceding column: an upstream caret at a soft break belongs to
    // that column, and Right needs its geometry. Never trust an edited suffix.
    const auto wanted_column = first_column_ == 0 ? 0 : first_column_ - 1U;
    for (const auto anchor : anchors_) {
      const bool inside_word=input.automatic_literals&&anchor.byte&&anchor.byte<input.text.size()&&
          literalClass(static_cast<unsigned char>(input.text[anchor.byte-1]),input.rules)==1&&
          literalClass(static_cast<unsigned char>(input.text[anchor.byte]),input.rules)==1;
      if ((!inside_word||anchor.long_word) && anchor.valid && anchor.column <= wanted_column && anchor.byte < focus.byte &&
          (same_flow || anchor.byte < edit_begin) &&
          (!resume.valid || anchor.column > resume.column)) {
        resume = anchor;
      }
    }
  }
  if (!same_flow) {
    anchors_.fill({});
    next_anchor_ = 0;
  }
  config_ = config;
  stamp_ = input.stamp;
  focus_ = focus;
  head_ = 0;
  count_ = 0;
  resumed_at_byte_ = 0;
  focus_found_ = false;
  document_end_ = false;
  status_ = flow_.start(input, config);
  if (status_ != Status::MoreWork) return status_;
  if (!editor::isUtf8Boundary(input.text, focus.byte)) {
    status_ = Status::InvalidPosition;
    return status_;
  }
  visible_columns_ = static_cast<std::uint16_t>(
      (static_cast<unsigned>(config.body.width) + config.column_gap_px) /
      (static_cast<unsigned>(config.column_width_px) + config.column_gap_px));
  // Reusing a checkpoint within a Literal would require retaining its measured
  // block state. This first version deliberately rescans such inputs instead.
  if (resume.valid && input.literal_count == 0) {
    flow_.resumeUnchangedPrefix(resume.byte, resume.column, resume.long_word);
    resumed_at_byte_ = resume.byte;
    remember(resume);
  } else {
    remember({0, 0, true});
  }
  return status_;
}

Status Viewport::renewNavigation(const Input& input, const Position origin,
                                 const Direction direction) noexcept {
  if (input.stamp != stamp_) return Status::Stale;
  const auto mapped = locate(input.stamp, origin);
  if (mapped.status != Status::Ready) return mapped.status;
  const bool earlier = direction == Direction::Right || direction == Direction::Up;
  const auto neighbor = earlier && mapped.caret.column ? mapped.caret.column - 1U : mapped.caret.column;
  const auto wanted = std::min(first_column_, neighbor);
  Anchor resume{};
  for (const auto anchor : anchors_) {
    const bool inside_word = input.automatic_literals && anchor.byte && anchor.byte < input.text.size() &&
        literalClass(static_cast<unsigned char>(input.text[anchor.byte-1]),input.rules)==1 &&
        literalClass(static_cast<unsigned char>(input.text[anchor.byte]),input.rules)==1;
    // One predecessor preserves both affinities at a soft wrap.
    if (anchor.valid && anchor.column <= (wanted ? wanted-1U : 0U) &&
        (!inside_word || anchor.long_word) && (!resume.valid || anchor.column > resume.column)) resume=anchor;
  }
  status_=flow_.start(input,config_);
  if (status_!=Status::MoreWork) return status_;
  head_=count_=0;document_end_=false;focus_=origin;focus_found_=true;resumed_at_byte_=0;
  if (resume.valid && !input.literal_count) {
    flow_.resumeUnchangedPrefix(resume.byte,resume.column,resume.long_word);
    resumed_at_byte_=resume.byte;
  }
  // first_column_ is unchanged until App applies the canonical target.
  return status_;
}

Status Viewport::scrollTo(const Position anchor) noexcept {
  if(status_!=Status::Ready) return status_;
  const auto found=locateUnchecked(anchor);
  if(found.status!=Status::Ready) return found.status;
  first_column_=found.caret.column;
  return Status::Ready;
}

void Viewport::dropFirstColumn() noexcept {
  if (count_ == 0) return;
  const auto column = eventAt(0).column;
  while (count_ != 0 && eventAt(0).column == column) {
    head_ = (head_ + 1U) % event_capacity;
    --count_;
  }
}

bool Viewport::push(const Event event) noexcept {
  // Discard only whole columns older than the window we might still need.
  // A single overfull column or an overfull visible window is an explicit error.
  while (count_ != 0 && eventAt(0).column + visible_columns_ + 2U < event.column &&
         (!focus_found_ || eventAt(0).column < first_column_)) {
    dropFirstColumn();
  }
  while (count_ == event_capacity) {
    if (eventAt(0).column == event.column ||
        (focus_found_ && eventAt(0).column >= first_column_)) {
      return false;
    }
    dropFirstColumn();
  }
  events_[(head_ + count_) % event_capacity] = event;
  ++count_;
  return true;
}

void Viewport::ensureFocusVisible(const Caret& caret) noexcept {
  if (config_.focus_at_left) {
    first_column_ = caret.column >= visible_columns_ ? caret.column + 1U - visible_columns_ : 0;
    return;
  }
  if (caret.column < first_column_) first_column_ = caret.column;
  if (caret.column >= first_column_ + visible_columns_) {
    first_column_ = caret.column + 1U - visible_columns_;
  }
}

Step Viewport::service(const Input& input, const std::size_t work_budget) noexcept {
  Step result;
  if (input.stamp != stamp_) {
    status_ = Status::Stale;
  }
  if (status_ != Status::MoreWork) {
    result.status = status_;
    return result;
  }
  while (result.work < work_budget) {
    Event event;
    const auto part = flow_.step(input, &event, 1, work_budget - result.work);
    result.work += part.work;
    result.bytes_read += part.bytes_read;
    total_work_ += part.work;
    if (part.status != Status::MoreWork && part.status != Status::OutputFull &&
        part.status != Status::DocumentEnd) {
      status_ = part.status;
      break;
    }
    if (part.written == 0) break;
    if (!push(event)) {
      status_ = Status::CapacityExceeded;
      break;
    }
    ++result.written;
    if (event.kind == EventKind::SoftBreak || event.kind == EventKind::Newline) {
      const bool long_word=input.automatic_literals&&flow_.long_literal_&&
          event.kind==EventKind::SoftBreak;
      remember({event.end_byte, event.column + 1U, true, long_word});
    }
    if (!focus_found_ && (event.end_byte > focus_.byte || event.kind == EventKind::End)) {
      const auto mapped = locateUnchecked(focus_);
      if (mapped.status != Status::Ready) {
        status_ = Status::InvalidPosition;
        break;
      }
      focus_found_ = true;
      ensureFocusVisible(mapped.caret);
      if (count_ != 0 && eventAt(0).column > first_column_) {
        status_ = Status::CapacityExceeded;
        break;
      }
    }
    if (event.kind == EventKind::End) {
      document_end_ = true;
      // Width growth reveals more text without restoring a pre-avoidance
      // viewport. The real caret column remains visible even at document end.
      status_ = focus_found_ ? Status::Ready : Status::InvalidPosition;
      break;
    }
    // Complete one extra left column for the next geometric navigation action.
    if (focus_found_ && (event.kind == EventKind::SoftBreak || event.kind == EventKind::Newline) &&
        event.column >= first_column_ + visible_columns_) {
      status_ = Status::Ready;
      break;
    }
  }
  result.status = status_;
  return result;
}

Mapping Viewport::locateUnchecked(const Position position) const noexcept {
  Mapping first, last;
  for (std::size_t i = 0; i < count_; ++i) {
    const auto event = eventAt(i);
    for (const auto candidate : {event.before(), event.after()}) {
      if (candidate.position.byte != position.byte) continue;
      if (first.status != Status::Ready || before(candidate, first.caret)) {
        first = {Status::Ready, candidate};
      }
      if (last.status != Status::Ready || before(last.caret, candidate)) {
        last = {Status::Ready, candidate};
      }
    }
  }
  auto found = position.affinity == Affinity::Upstream ? first : last;
  if (found.status == Status::Ready) {
    found.caret.position.affinity = first.caret.column != last.caret.column &&
        found.caret.column == first.caret.column ? Affinity::Upstream : Affinity::Downstream;
  }
  return found;
}

Mapping Viewport::locate(const Stamp& current, const Position position) const noexcept {
  if (status(current) != Status::Ready) return {status(current), {}};
  return locateUnchecked(position);
}

Mapping Viewport::hitTest(const Stamp& current, const std::uint32_t column,
                          const std::uint16_t preferred_y_px) const noexcept {
  if (status(current) != Status::Ready) return {status(current), {}};
  Mapping best;
  unsigned best_distance = std::numeric_limits<unsigned>::max();
  for (std::size_t i = 0; i < count_; ++i) {
    const auto event = eventAt(i);
    for (auto candidate : {event.before(), event.after()}) {
      if (candidate.column != column) continue;
      const auto delta = distance(candidate.y_px, preferred_y_px);
      if (delta < best_distance || (delta == best_distance && candidate.y_px < best.caret.y_px)) {
        best_distance = delta;
        best = {Status::Ready, candidate};
      }
    }
  }
  if (best.status == Status::Ready) {
    const auto upstream = locateUnchecked({best.caret.position.byte, Affinity::Upstream});
    best.caret.position.affinity = upstream.caret.column == column &&
        locateUnchecked({best.caret.position.byte, Affinity::Downstream}).caret.column != column
        ? Affinity::Upstream : Affinity::Downstream;
  }
  return best;
}

Mapping Viewport::move(const Stamp& current, const Position from,
                       const Direction direction, const std::uint16_t preferred_y_px) const noexcept {
  const auto origin = locate(current, from);
  if (origin.status != Status::Ready) return origin;
  if (direction == Direction::Left || direction == Direction::Right) {
    if (direction == Direction::Right && origin.caret.column == 0) return {Status::Boundary, origin.caret};
    const auto column = direction == Direction::Left ? origin.caret.column + 1U : origin.caret.column - 1U;
    auto target = hitTest(current, column, preferred_y_px);
    if (document_end_ && column > eventAt(count_ - 1U).column) return {Status::Boundary,origin.caret};
    // A break.after() may expose only y=0 in an otherwise absent column.
    // Nearest-y selection is valid only after canonical Flow completed it.
    bool complete=false;
    for (std::size_t i=0;i<count_;++i) {
      const auto e=eventAt(i);
      if(e.column==column&&(e.kind==EventKind::SoftBreak||e.kind==EventKind::Newline||e.kind==EventKind::End))complete=true;
    }
    if (target.status!=Status::Ready || !complete) target.status=Status::NeedsLayout;
    return target;
  }
  Mapping best;
  for (std::size_t i = 0; i < count_; ++i) {
    const auto event = eventAt(i);
    for (auto candidate : {event.before(), event.after()}) {
      const bool eligible = direction == Direction::Up ? before(candidate, origin.caret) : before(origin.caret, candidate);
      const bool nearer = best.status != Status::Ready ||
          (direction == Direction::Up ? before(best.caret, candidate) : before(candidate, best.caret));
      if (eligible && nearer) best = {Status::Ready, candidate};
    }
  }
  if (best.status != Status::Ready) {
    const bool boundary = direction == Direction::Up ? origin.caret.position.byte == 0 : document_end_;
    return {boundary ? Status::Boundary : Status::NeedsLayout, origin.caret};
  }
  const auto upstream = locateUnchecked({best.caret.position.byte, Affinity::Upstream});
  const auto downstream = locateUnchecked({best.caret.position.byte, Affinity::Downstream});
  best.caret.position.affinity = upstream.caret.column != downstream.caret.column &&
      best.caret.column == upstream.caret.column ? Affinity::Upstream : Affinity::Downstream;
  return best;
}

bool Viewport::visible(const std::uint32_t column) const noexcept {
  return column >= first_column_ && column - first_column_ < visible_columns_;
}

Rect Viewport::glyphRect(const Event& event) const noexcept {
  if (!visible(event.column)) return {};
  const auto x = config_.body.x + config_.body.width - config_.column_width_px -
      (event.column - first_column_) * (config_.column_width_px + config_.column_gap_px);
  return {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(config_.body.y + event.y_px),
          config_.column_width_px, event.advance_px};
}

Rect Viewport::caretRect(const Caret& caret) const noexcept {
  Event event;
  event.column = caret.column;
  event.y_px = std::min(caret.y_px, static_cast<std::uint16_t>(config_.body.height - 1U));
  event.advance_px = draftor::BODY_CARET_STROKE_PX;
  return glyphRect(event);
}

Rectangles Viewport::rectangles(const Input& input, const ByteRange range,
                                Rect* const output, const std::size_t capacity) const noexcept {
  if (status(input.stamp) != Status::Ready) return {status(input.stamp), 0, 0};
  if (range.begin > range.end || !editor::isUtf8Boundary(input.text, range.begin) ||
      !editor::isUtf8Boundary(input.text, range.end)) return {Status::InvalidPosition, 0, 0};
  Rectangles result;
  for (std::size_t i = 0; i < count_; ++i) {
    const auto event = eventAt(i);
    if (visible(event.column) && event.begin_byte < range.end && event.end_byte > range.begin &&
        (event.glyph() || event.kind == EventKind::Newline)) ++result.required;
  }
  if (result.required > capacity || (result.required != 0 && output == nullptr)) {
    result.status = Status::CapacityExceeded;
    return result;  // No partial rectangle buffer is published as complete.
  }
  for (std::size_t i = 0; i < count_; ++i) {
    auto event = eventAt(i);
    if (!visible(event.column) || event.begin_byte >= range.end || event.end_byte <= range.begin ||
        (!event.glyph() && event.kind != EventKind::Newline)) continue;
    if (event.kind == EventKind::Newline) {
      event.y_px = std::min(event.y_px, static_cast<std::uint16_t>(config_.body.height - 1U));
      event.advance_px = static_cast<std::uint16_t>(config_.body.height - event.y_px);
    }
    output[result.written++] = glyphRect(event);
  }
  return result;
}

}  // namespace draftor::vertical
