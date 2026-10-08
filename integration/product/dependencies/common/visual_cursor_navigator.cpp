#include "visual_cursor_navigator.hpp"
#include "body_editor.hpp"
#include "operation_profile.hpp"

#include <algorithm>

namespace editor::visual_nav {

VisualCursorNavigator::VisualCursorNavigator(
    const Document& document,
    const hanime::renderer::LayoutEngine& layout) noexcept
    : document_(document), layout_(layout) {
  rememberAnchor({0, 0, true});
}

bool VisualCursorNavigator::placeCaretSlot(std::uint16_t slot) noexcept {
  const auto view=sync();
  if(!view.visible_lines||slot>=view.visible_lines)return false;
  const auto first=static_cast<std::uint16_t>(view.visual_line>slot?view.visual_line-slot:0);
  if(first==first_visible_line_)return true;
  const auto anchor=anchorForLine(first);
  if(!anchor.valid)return false;
  first_visible_line_=first;first_visible_anchor_=anchor;rememberAnchor(anchor);
  return true;
}

void VisualCursorNavigator::recordTraversal(const std::size_t bytes) noexcept {
  last_traversal_bytes_ += bytes;
  maximum_traversal_bytes_ = std::max(maximum_traversal_bytes_, bytes);
}

void VisualCursorNavigator::rememberAnchor(
    const hanime::renderer::VisualLineAnchor anchor) noexcept {
  if (!anchor.valid) return;
  for (auto& cached : anchor_history_) {
    if (cached.valid && cached.line == anchor.line) {
      cached = anchor;
      return;
    }
  }
  anchor_history_[anchor_history_next_] = anchor;
  anchor_history_next_ =
      (anchor_history_next_ + 1U) % anchor_history_.size();
}

hanime::renderer::VisualLineAnchor VisualCursorNavigator::anchorForLine(
    const std::uint16_t visual_line) noexcept {
  hanime::renderer::VisualLineAnchor best{};
  for (const auto& cached : anchor_history_) {
    if (!cached.valid || cached.line > visual_line) continue;
    if (!best.valid || cached.line > best.line) best = cached;
  }
  if (first_visible_anchor_.valid &&
      first_visible_anchor_.line <= visual_line &&
      (!best.valid || first_visible_anchor_.line > best.line)) {
    best = first_visible_anchor_;
  }
  if (!best.valid) {
    best = {0, 0, true};
    ++full_scan_fallback_count_;
  }

  while (best.valid && best.line < visual_line) {
    const auto line = layout_.layoutVisualLine(
        document_.text(), best, best.line, nullptr, 0);
    recordTraversal(line.bytes_visited);
    if (!line.next.valid || line.next.line <= best.line) return {};
    best = line.next;
    rememberAnchor(best);
  }
  return best.line == visual_line ? best
                                  : hanime::renderer::VisualLineAnchor{};
}

void VisualCursorNavigator::prepareAnchorsForTextEdit() noexcept {
  if (!initialized_ || document_.textRevision() == last_text_revision_) return;
  const auto edit = document_.lastTextEdit();
  const bool adjacent_incremental =
      edit.incremental && edit.text_revision == last_text_revision_ + 1U;
  if (!adjacent_incremental) {
    first_visible_anchor_ = {};
    anchor_history_ = {};
    anchor_history_next_ = 0;
    rememberAnchor({0, 0, true});
    cached_caret_ = {};
    return;
  }

  // Anchors at or before the edit remain canonical. Any later anchor may
  // change byte identity or wrap-line number, so discard it and rebuild only
  // forward from the current viewport boundary.
  for (auto& anchor : anchor_history_) {
    if (anchor.valid && anchor.byte_offset > edit.begin) anchor = {};
  }
  if (first_visible_anchor_.valid &&
      first_visible_anchor_.byte_offset > edit.begin) {
    first_visible_anchor_ = {};
  }
  cached_caret_ = {};
}

hanime::renderer::VisualCaretPosition
VisualCursorNavigator::caretAtAffinity(
    const hanime::renderer::CaretAffinity affinity) noexcept {
  return caretAtByte(document_.cursorByte(), affinity);
}

hanime::renderer::VisualCaretPosition VisualCursorNavigator::caretAtByte(
    const std::size_t cursor,
    const hanime::renderer::CaretAffinity affinity) noexcept {
  hanime::renderer::VisualLineAnchor anchor = first_visible_anchor_;
  if (!anchor.valid || cursor < anchor.byte_offset) {
    anchor = {0, 0, true};
    ++full_scan_fallback_count_;
  }
  std::size_t visited = 0;
  const auto caret = layout_.visualCaretAtFromAnchor(
      document_.text(), anchor, cursor, affinity, &visited);
  recordTraversal(visited);
  return caret;
}

hanime::renderer::VisualCaretPosition
VisualCursorNavigator::currentCaret() noexcept {
  if (cached_caret_.valid && initialized_ &&
      document_.cursorByte() == last_cursor_byte_ &&
      document_.revision() == last_document_revision_ &&
      document_.textRevision() == last_text_revision_ &&
      cached_caret_.affinity == affinity_) {
    return cached_caret_;
  }
  cached_caret_ = caretAtAffinity(affinity_);
  return cached_caret_;
}

void VisualCursorNavigator::observeExternalChange(
    const hanime::renderer::VisualCaretPosition& caret) noexcept {
  const bool external_change = !initialized_ ||
      document_.cursorByte() != last_cursor_byte_ ||
      document_.text().size() != last_text_size_ ||
      document_.revision() != last_document_revision_ ||
      document_.textRevision() != last_text_revision_;
  if (external_change && caret.valid) {
    preferred_x_ = caret.x;
    preferred_valid_ = true;
  }
}

void VisualCursorNavigator::ensureVisible(
    const std::uint16_t visual_line) noexcept {
  const std::uint16_t visible = layout_.visibleTextLineCount();
  const std::uint16_t previous = first_visible_line_;
  if (visible == 0) {
    first_visible_line_ = visual_line;
  } else {
    if (visual_line < first_visible_line_) first_visible_line_ = visual_line;
    const std::uint32_t bottom =
        static_cast<std::uint32_t>(first_visible_line_) + visible;
    if (visual_line >= bottom) {
      first_visible_line_ =
          static_cast<std::uint16_t>(visual_line - visible + 1U);
    }
  }

  if (first_visible_line_ != previous && first_visible_anchor_.valid) {
    rememberAnchor(first_visible_anchor_);
  }
  if (!first_visible_anchor_.valid ||
      first_visible_anchor_.line != first_visible_line_) {
    first_visible_anchor_ = anchorForLine(first_visible_line_);
    rememberAnchor(first_visible_anchor_);
  }
}

void VisualCursorNavigator::rememberState() noexcept {
  last_cursor_byte_ = document_.cursorByte();
  last_text_size_ = document_.text().size();
  last_document_revision_ = document_.revision();
  last_text_revision_ = document_.textRevision();
  initialized_ = true;
}

VisualNavigationView VisualCursorNavigator::sync() noexcept {
  s3profile::Scope timing(s3profile::Layout);
  last_traversal_bytes_ = 0;
  const bool text_changed =
      !initialized_ || document_.textRevision() != last_text_revision_;
  if (text_changed) {
    prepareAnchorsForTextEdit();
    if (!restored_affinity_pending_) {
      affinity_ = hanime::renderer::CaretAffinity::Downstream;
    }
  }
  if (restored_affinity_pending_) affinity_ = restored_affinity_;
  restored_affinity_pending_ = false;
  const auto caret = currentCaret();
  observeExternalChange(caret);
  if (caret.valid) ensureVisible(caret.line);
  cached_caret_ = caret;
  rememberState();
  return {document_.cursorByte(),
          static_cast<std::uint16_t>(caret.valid ? caret.line : 0U),
          static_cast<std::uint16_t>(caret.valid ? caret.x : 0U),
          static_cast<std::uint16_t>(preferred_valid_ ? preferred_x_ : 0U),
          first_visible_line_,
          layout_.visibleTextLineCount(),
          caret.affinity};
}

VisualNavigationView VisualCursorNavigator::rebindViewportGeometry(
    const bool clamp_blank_tail) noexcept {
  last_traversal_bytes_ = 0;

  // Horizontal wrapping is unchanged, so cached anchors remain canonical.
  // The caret cache is discarded only to make the new visible-height decision
  // explicit; affinity and preferred_x_ are deliberately untouched.
  cached_caret_ = {};
  const auto caret = currentCaret();
  observeExternalChange(caret);
  if (caret.valid) ensureVisible(caret.line);

  const std::uint16_t visible = layout_.visibleTextLineCount();
  if (clamp_blank_tail && visible != 0) {
    const std::uint16_t total = layout_.visualLineCount(document_.text());
    const std::uint16_t maximum_first = total > visible
        ? static_cast<std::uint16_t>(total - visible)
        : 0;
    if (first_visible_line_ > maximum_first) {
      if (first_visible_anchor_.valid) rememberAnchor(first_visible_anchor_);
      first_visible_line_ = maximum_first;
      first_visible_anchor_ = anchorForLine(first_visible_line_);
      rememberAnchor(first_visible_anchor_);
    }
  }

  cached_caret_ = caret;
  rememberState();
  return {document_.cursorByte(),
          static_cast<std::uint16_t>(caret.valid ? caret.line : 0U),
          static_cast<std::uint16_t>(caret.valid ? caret.x : 0U),
          static_cast<std::uint16_t>(preferred_valid_ ? preferred_x_ : 0U),
          first_visible_line_, visible, caret.affinity};
}

bool VisualCursorNavigator::restoreViewportAnchor(
    const std::size_t first_visible_byte,
    const hanime::renderer::CaretAffinity caret_affinity) noexcept {
  s3profile::Scope timing(s3profile::Layout);
  hanime::renderer::VisualLineAnchor viewport_anchor{};
  const auto viewport_caret = layout_.visualCaretAtFromAnchor(
      document_.text(), {0, 0, true}, first_visible_byte,
      hanime::renderer::CaretAffinity::Downstream, nullptr, &viewport_anchor);
  // Normal saved positions put the cursor inside this viewport. Reuse its
  // canonical anchor. Upstream at the first byte needs the preceding flow.
  auto cursor_start = viewport_anchor;
  if (!cursor_start.valid || document_.cursorByte() < cursor_start.byte_offset ||
      (document_.cursorByte() == cursor_start.byte_offset &&
       caret_affinity == hanime::renderer::CaretAffinity::Upstream)) {
    cursor_start = {0, 0, true};
  }
  const auto cursor_caret = layout_.visualCaretAtFromAnchor(
      document_.text(), cursor_start, document_.cursorByte(), caret_affinity);
  if (!viewport_caret.valid || viewport_caret.byte_offset != first_visible_byte ||
      !cursor_caret.valid ||
      cursor_caret.byte_offset != document_.cursorByte()) {
    return false;
  }

  if (!viewport_anchor.valid) return false;

  anchor_history_ = {};
  anchor_history_next_ = 0;
  rememberAnchor({0, 0, true});
  first_visible_line_ = viewport_anchor.line;
  first_visible_anchor_ = viewport_anchor;
  rememberAnchor(first_visible_anchor_);

  affinity_ = cursor_caret.affinity;
  restored_affinity_ = affinity_;
  restored_affinity_pending_ = false;
  cached_caret_ = cursor_caret;
  preferred_x_ = cursor_caret.x;
  preferred_valid_ = true;
  last_traversal_bytes_ = 0;

  // Persisted state normally already satisfies this invariant. Corrupt or
  // stale position records are made safe by minimally scrolling to the caret.
  ensureVisible(cursor_caret.line);
  rememberState();
  return true;
}

body_edit::Outcome VisualCursorNavigator::moveHorizontal(
    const VisualHorizontalDirection direction,
    body_edit::BodyEditor& editor, const bool extend_selection) noexcept {
  last_traversal_bytes_ = 0;
  auto current = currentCaret();
  if (!current.valid) return {body_edit::ResultCode::InvalidPosition};
  observeExternalChange(current);
  const auto target = document_.cursorTarget(
      direction == VisualHorizontalDirection::Left ? CursorMove::Left : CursorMove::Right);
  if (target == document_.cursorByte()) {
    const auto outcome = editor.moveToByte(target, extend_selection, affinity_);
    rememberState();
    return outcome;
  }
  auto affinity = hanime::renderer::CaretAffinity::Downstream;
  hanime::renderer::VisualCaretPosition landed{};
  if (direction == VisualHorizontalDirection::Right) {
    const auto upstream = caretAtByte(target, hanime::renderer::CaretAffinity::Upstream);
    if (upstream.valid && upstream.affinity == hanime::renderer::CaretAffinity::Upstream) {
      affinity = upstream.affinity;
      landed = upstream;
    }
  }
  if (!landed.valid) landed = caretAtByte(target, affinity);
  if (!landed.valid) return {body_edit::ResultCode::InvalidPosition};
  const auto outcome = editor.moveToByte(target, extend_selection, affinity);
  if (outcome.code == body_edit::ResultCode::InvalidPosition ||
      outcome.code == body_edit::ResultCode::ResourceExhausted) return outcome;
  affinity_ = affinity;
  cached_caret_ = landed;
  preferred_x_ = landed.x;
  preferred_valid_ = true;
  ensureVisible(landed.line);
  rememberState();
  return outcome;
}

body_edit::Outcome VisualCursorNavigator::move(
    const VisualDirection direction,
    body_edit::BodyEditor& editor, const bool extend_selection) noexcept {
  last_traversal_bytes_ = 0;
  auto current = currentCaret();
  if (!current.valid) return {body_edit::ResultCode::InvalidPosition};

  observeExternalChange(current);
  if (!preferred_valid_) {
    preferred_x_ = current.x;
    preferred_valid_ = true;
  }

  std::uint16_t target_line = current.line;
  if (direction == VisualDirection::Up) {
    if (current.line == 0) {
      ensureVisible(current.line);
      rememberState();
      return editor.moveToByte(document_.cursorByte(), extend_selection, affinity_);
    }
    target_line = static_cast<std::uint16_t>(current.line - 1U);
  } else if (current.line == 0xFFFFU) {
    ensureVisible(current.line);
    rememberState();
    return editor.moveToByte(document_.cursorByte(), extend_selection, affinity_);
  } else {
    target_line = static_cast<std::uint16_t>(current.line + 1U);
  }

  auto target_anchor = first_visible_anchor_;
  if (!target_anchor.valid || target_line < target_anchor.line) {
    target_anchor = anchorForLine(target_line);
  }
  if (!target_anchor.valid) {
    ensureVisible(current.line);
    rememberState();
    return editor.moveToByte(document_.cursorByte(), extend_selection, affinity_);
  }

  std::size_t visited = 0;
  const auto target = layout_.visualTargetAtXFromAnchor(
      document_.text(), target_anchor, target_line, preferred_x_, &visited);
  recordTraversal(visited);
  if (!target.exists) {
    ensureVisible(current.line);
    rememberState();
    return editor.moveToByte(document_.cursorByte(), extend_selection, affinity_);
  }

  const auto outcome = editor.moveToByte(target.byte_offset, extend_selection, target.affinity);
  if (outcome.code == body_edit::ResultCode::InvalidPosition ||
      outcome.code == body_edit::ResultCode::ResourceExhausted) return outcome;

  affinity_ = target.affinity;
  cached_caret_ = {target.byte_offset, target.line, target.x,
                   true, target.affinity};
  ensureVisible(target.line);
  rememberState();
  return outcome;
}

}  // namespace editor::visual_nav
