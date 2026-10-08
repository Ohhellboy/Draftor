#include "layout.hpp"

#include <algorithm>
#include <limits>

namespace hanime::renderer {
namespace {

struct Decoded {
  std::uint32_t codepoint;
  std::size_t bytes;
};

Decoded decode(std::string_view text, std::size_t offset) noexcept {
  const auto first = static_cast<unsigned char>(text[offset]);
  if (first < 0x80) return {first, 1};
  std::size_t length = 0;
  std::uint32_t value = 0;
  if ((first & 0xE0) == 0xC0) { length = 2; value = first & 0x1F; }
  else if ((first & 0xF0) == 0xE0) { length = 3; value = first & 0x0F; }
  else if ((first & 0xF8) == 0xF0) { length = 4; value = first & 0x07; }
  else return {0xFFFD, 1};
  if (offset + length > text.size()) return {0xFFFD, 1};
  for (std::size_t i = 1; i < length; ++i) {
    const auto next = static_cast<unsigned char>(text[offset + i]);
    if ((next & 0xC0) != 0x80) return {0xFFFD, 1};
    value = (value << 6) | (next & 0x3F);
  }
  const bool overlong = (length == 2 && value < 0x80) ||
                        (length == 3 && value < 0x800) ||
                        (length == 4 && value < 0x10000);
  if (overlong || value > 0x10FFFF ||
      (value >= 0xD800 && value <= 0xDFFF)) return {0xFFFD, 1};
  return {value, length};
}

bool isAscii(std::uint32_t codepoint) noexcept { return codepoint <= 0x7F; }

std::uint16_t advanceFor(const LayoutConfig& config,
                         std::uint32_t codepoint) noexcept {
  if (config.codepoint_advance != nullptr) {
    return config.codepoint_advance(config.codepoint_advance_context, codepoint);
  }
  return isAscii(codepoint) ? config.ascii_advance_px : config.cjk_advance_px;
}

bool isAsciiWordByte(unsigned char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '\'';
}

bool startsAsciiWord(std::string_view text, std::size_t offset) noexcept {
  if (offset >= text.size()) return false;
  const auto current = static_cast<unsigned char>(text[offset]);
  if (current >= 0x80U || !isAsciiWordByte(current)) return false;
  if (offset == 0) return true;
  const auto prev = static_cast<unsigned char>(text[offset - 1U]);
  return prev >= 0x80U || !isAsciiWordByte(prev);
}

bool isOpeningPunctuation(std::uint32_t cp) noexcept {
  switch (cp) {
    case '(':
    case '[':
    case '{':
    case '<':
    case 0xFF08:  // （
    case 0x3010:  // 【
    case 0x300A:  // 《
    case 0x201C:  // “
    case 0x2018:  // ‘
      return true;
    default:
      return false;
  }
}

bool isClosingPunctuation(std::uint32_t cp) noexcept {
  switch (cp) {
    case ',':
    case '.':
    case '!':
    case '?':
    case ';':
    case ':':
    case ')':
    case ']':
    case '}':
    case '>':
    case 0xFF0C:  // ，
    case 0x3002:  // 。
    case 0xFF01:  // ！
    case 0xFF1F:  // ？
    case 0xFF1B:  // ；
    case 0xFF1A:  // ：
    case 0x3001:  // 、
    case 0xFF09:  // ）
    case 0x3011:  // 】
    case 0x300B:  // 》
    case 0x201D:  // ”
    case 0x2019:  // ’
      return true;
    default:
      return false;
  }
}

std::uint32_t measureClosingRun(const LayoutConfig& config,
                                std::string_view text,
                                std::size_t offset) noexcept {
  std::uint32_t width = 0;
  while (offset < text.size()) {
    const auto item = decode(text, offset);
    if (!isClosingPunctuation(item.codepoint)) break;
    width += advanceFor(config, item.codepoint);
    offset += item.bytes;
  }
  return width;
}

std::uint32_t measureAsciiWordToken(const LayoutConfig& config,
                                    std::string_view text,
                                    std::size_t offset) noexcept {
  std::uint32_t width = 0;
  while (offset < text.size()) {
    const auto b = static_cast<unsigned char>(text[offset]);
    if (b >= 0x80U || !isAsciiWordByte(b)) break;
    width += advanceFor(config, b);
    ++offset;
  }
  return width + measureClosingRun(config, text, offset);
}

std::uint32_t measureFollowingUnit(const LayoutConfig& config,
                                   std::string_view text,
                                   std::size_t offset) noexcept {
  if (offset >= text.size()) return 0;
  const auto b = static_cast<unsigned char>(text[offset]);
  if (b < 0x80U && isAsciiWordByte(b)) {
    return measureAsciiWordToken(config, text, offset);
  }
  const auto item = decode(text, offset);
  if (item.codepoint == '\n') return 0;
  return static_cast<std::uint32_t>(advanceFor(config, item.codepoint)) +
         measureClosingRun(config, text, offset + item.bytes);
}

void emit(LayoutResult& result, Placement placement, Placement* output,
          std::size_t capacity) noexcept {
  if (result.placements_written < capacity && output != nullptr) {
    output[result.placements_written] = placement;
    ++result.placements_written;
  }
  ++result.placements_required;
}

struct FlowState {
  std::size_t offset = 0;
  std::uint16_t line = 0;
  std::uint16_t x = 0;
};

void nextVisualLine(FlowState& state) noexcept {
  state.x = 0;
  if (state.line != std::numeric_limits<std::uint16_t>::max()) ++state.line;
}

void applyPendingSoftWrap(const LayoutConfig& config, std::string_view utf8,
                          FlowState& state) noexcept {
  if (state.offset >= utf8.size() || state.x == 0) return;
  const auto item = decode(utf8, state.offset);
  if (item.codepoint == '\n') return;

  const std::uint32_t current_advance = advanceFor(config, item.codepoint);
  std::uint32_t preferred = current_advance;

  if (startsAsciiWord(utf8, state.offset)) {
    preferred = measureAsciiWordToken(config, utf8, state.offset);
  } else if (isOpeningPunctuation(item.codepoint)) {
    const std::size_t next = state.offset + item.bytes;
    const std::uint32_t following = measureFollowingUnit(config, utf8, next);
    if (following != 0) {
      const std::uint32_t together = current_advance + following;
      if (together <= config.width_px) {
        preferred = together;
      } else if (next < utf8.size()) {
        // If the following unit is itself overlong, keep the opening mark with
        // at least the following code point and let that unit hard-wrap later.
        const auto next_item = decode(utf8, next);
        const std::uint32_t minimal = current_advance + advanceFor(config, next_item.codepoint);
        if (minimal <= config.width_px) preferred = minimal;
      }
    }
  } else {
    // Closing punctuation look-ahead only matters near the right edge. Avoid
    // decoding the following code point for the vast majority of ordinary CJK
    // cells, preserving the R02 long-document cost profile.
    const std::uint32_t used_after_current =
        static_cast<std::uint32_t>(state.x) + current_advance;
    const std::uint32_t guard =
        2U * static_cast<std::uint32_t>(
            std::max(config.cjk_advance_px, config.ascii_advance_px));
    if (used_after_current <= config.width_px &&
        config.width_px - used_after_current < guard) {
      preferred += measureClosingRun(config, utf8, state.offset + item.bytes);
    }
  }

  if (preferred <= config.width_px &&
      static_cast<std::uint32_t>(state.x) + preferred > config.width_px) {
    nextVisualLine(state);
    return;
  }

  // Overlong word/unit fallback: character-level wrapping always makes
  // progress and preserves the old behavior for pathological tokens.
  if (static_cast<std::uint32_t>(state.x) + current_advance > config.width_px) {
    nextVisualLine(state);
  }
}

void advanceFlowAfterPending(const LayoutConfig& config, std::string_view utf8,
                             FlowState& state) noexcept {
  if (state.offset >= utf8.size()) return;
  const auto item = decode(utf8, state.offset);
  if (item.codepoint == '\n') {
    nextVisualLine(state);
    state.offset += item.bytes;
    return;
  }
  state.x = static_cast<std::uint16_t>(state.x + advanceFor(config, item.codepoint));
  state.offset += item.bytes;
}

void advanceFlow(const LayoutConfig& config, std::string_view utf8,
                 FlowState& state) noexcept {
  if (state.offset >= utf8.size()) return;
  applyPendingSoftWrap(config, utf8, state);
  advanceFlowAfterPending(config, utf8, state);
}

bool isByteBoundary(std::string_view utf8, std::size_t byte) noexcept {
  if (byte > utf8.size()) return false;
  if (byte == 0 || byte == utf8.size()) return true;
  const auto value = static_cast<unsigned char>(utf8[byte]);
  return (value & 0xC0U) != 0x80U;
}

}  // namespace

std::uint16_t LayoutEngine::visibleTextLineCount() const noexcept {
  const auto body_height = config_.height_px > config_.candidate_bar_height_px
                               ? config_.height_px - config_.candidate_bar_height_px
                               : 0;
  return config_.line_height_px ? body_height / config_.line_height_px : 0;
}

VisualLineAnchor LayoutEngine::visualLineAnchorAt(
    const std::string_view utf8,
    const std::uint16_t visual_line) const noexcept {
  return visualLineAnchorFrom(utf8, {0, 0, true}, visual_line);
}

VisualLineAnchor LayoutEngine::visualLineAnchorFrom(
    const std::string_view utf8,
    const VisualLineAnchor anchor,
    const std::uint16_t visual_line,
    std::size_t* const bytes_visited) const noexcept {
  if (bytes_visited != nullptr) *bytes_visited = 0;
  if (!anchor.valid || anchor.byte_offset > utf8.size() ||
      !isByteBoundary(utf8, anchor.byte_offset) ||
      visual_line < anchor.line) {
    return {};
  }
  FlowState state{anchor.byte_offset, anchor.line, 0};
  while (true) {
    applyPendingSoftWrap(config_, utf8, state);
    if (state.line == visual_line && state.x == 0) {
      return {state.offset, state.line, true};
    }
    if (state.line > visual_line || state.offset >= utf8.size()) break;
    const std::size_t before = state.offset;
    advanceFlowAfterPending(config_, utf8, state);
    if (bytes_visited != nullptr) *bytes_visited += state.offset - before;
  }
  return {};
}

VisualLineLayoutResult LayoutEngine::layoutVisualLine(
    const std::string_view utf8,
    const VisualLineAnchor anchor,
    const std::uint16_t first_visible_line,
    Placement* output,
    const std::size_t capacity) const noexcept {
  VisualLineLayoutResult result;
  if (!anchor.valid || anchor.byte_offset > utf8.size() ||
      !isByteBoundary(utf8, anchor.byte_offset)) {
    return result;
  }

  FlowState state{anchor.byte_offset, anchor.line, 0};
  while (true) {
    const std::uint16_t x_before_wrap = state.x;
    applyPendingSoftWrap(config_, utf8, state);
    if (state.line != anchor.line) {
      result.next = {state.offset, state.line, true};
      result.end_byte = state.offset;
      result.end_x = x_before_wrap;
      result.break_kind = VisualLineBreak::SoftWrap;
      result.output_truncated =
          result.placements_written < result.placements_required;
      return result;
    }

    if (state.offset >= utf8.size()) {
      result.document_end = true;
      result.end_byte = state.offset;
      result.end_x = state.x;
      result.break_kind = VisualLineBreak::DocumentEnd;
      result.output_truncated =
          result.placements_written < result.placements_required;
      return result;
    }

    const auto item = decode(utf8, state.offset);
    if (item.codepoint == '\n') {
      const std::size_t before = state.offset;
      const std::uint16_t line_end_x = state.x;
      advanceFlowAfterPending(config_, utf8, state);
      result.bytes_visited += state.offset - before;
      result.next = {state.offset, state.line, true};
      result.end_byte = before;
      result.end_x = line_end_x;
      result.break_kind = VisualLineBreak::HardNewline;
      result.output_truncated =
          result.placements_written < result.placements_required;
      return result;
    }

    const auto advance = advanceFor(config_, item.codepoint);
    Placement placement{item.codepoint, state.offset, state.x,
                        static_cast<std::uint16_t>(
                            (anchor.line - first_visible_line) *
                            config_.line_height_px),
                        advance, anchor.line, false};
    if (result.placements_written < capacity && output != nullptr) {
      output[result.placements_written++] = placement;
    }
    ++result.placements_required;
    const std::size_t before = state.offset;
    advanceFlowAfterPending(config_, utf8, state);
    result.bytes_visited += state.offset - before;
    result.end_x = state.x;
  }
}

VisualCaretPosition LayoutEngine::visualCaretAt(
    std::string_view utf8, std::size_t cursor_byte,
    const CaretAffinity affinity) const noexcept {
  return visualCaretAtFromAnchor(
      utf8, {0, 0, true}, cursor_byte, affinity);
}

VisualCaretPosition LayoutEngine::visualCaretAtFromAnchor(
    const std::string_view utf8,
    const VisualLineAnchor anchor,
    const std::size_t cursor_byte,
    const CaretAffinity affinity,
    std::size_t* const bytes_visited,
    VisualLineAnchor* const caret_line_anchor) const noexcept {
  VisualCaretPosition result;
  if (caret_line_anchor) *caret_line_anchor = {};
  VisualLineAnchor line_anchor = anchor;
  if (bytes_visited != nullptr) *bytes_visited = 0;
  if (!anchor.valid || anchor.byte_offset > utf8.size() ||
      cursor_byte < anchor.byte_offset ||
      !isByteBoundary(utf8, anchor.byte_offset) ||
      !isByteBoundary(utf8, cursor_byte)) {
    return result;
  }

  FlowState state{anchor.byte_offset, anchor.line, 0};
  while (true) {
    const FlowState before_wrap = state;
    applyPendingSoftWrap(config_, utf8, state);
    const bool soft_wrap = state.line != before_wrap.line &&
                           state.offset == before_wrap.offset;
    if (affinity == CaretAffinity::Upstream && soft_wrap &&
        before_wrap.offset == cursor_byte) {
      result.byte_offset = cursor_byte;
      result.line = before_wrap.line;
      result.x = before_wrap.x;
      result.valid = true;
      result.affinity = CaretAffinity::Upstream;
      if (caret_line_anchor) *caret_line_anchor = line_anchor;
      return result;
    }
    if (state.line != line_anchor.line) line_anchor = {state.offset, state.line, true};
    if (state.offset == cursor_byte) {
      result.byte_offset = cursor_byte;
      result.line = state.line;
      result.x = state.x;
      result.valid = true;
      result.affinity = CaretAffinity::Downstream;
      if (caret_line_anchor) *caret_line_anchor = line_anchor;
      return result;
    }
    if (state.offset >= utf8.size()) return result;
    const std::size_t before = state.offset;
    advanceFlowAfterPending(config_, utf8, state);
    if (bytes_visited != nullptr) *bytes_visited += state.offset - before;
  }
}

VisualLineTarget LayoutEngine::visualTargetAtX(
    std::string_view utf8, std::uint16_t visual_line,
    std::uint16_t preferred_x) const noexcept {
  return visualTargetAtXFromAnchor(
      utf8, {0, 0, true}, visual_line, preferred_x);
}

VisualLineTarget LayoutEngine::visualTargetAtXFromAnchor(
    const std::string_view utf8,
    const VisualLineAnchor anchor,
    const std::uint16_t visual_line,
    const std::uint16_t preferred_x,
    std::size_t* const bytes_visited) const noexcept {
  VisualLineTarget result;
  if (bytes_visited != nullptr) *bytes_visited = 0;
  if (!anchor.valid || anchor.byte_offset > utf8.size() ||
      visual_line < anchor.line ||
      !isByteBoundary(utf8, anchor.byte_offset)) {
    return result;
  }
  bool saw_target_line = false;
  bool have_best = false;
  std::size_t best_byte = 0;
  std::uint16_t best_x = 0;
  CaretAffinity best_affinity = CaretAffinity::Downstream;

  auto consider = [&](const std::size_t byte, const std::uint16_t line,
                      const std::uint16_t x,
                      const CaretAffinity affinity) noexcept -> bool {
    if (line != visual_line) return false;
    saw_target_line = true;
    if (!have_best || x <= preferred_x) {
      if (!have_best || x >= best_x) {
        best_byte = byte;
        best_x = x;
        best_affinity = affinity;
        have_best = true;
      }
    }
    if (x != preferred_x) return false;
    result = {byte, visual_line, x, true, false, affinity};
    return true;
  };

  FlowState state{anchor.byte_offset, anchor.line, 0};
  while (true) {
    const FlowState before_wrap = state;
    applyPendingSoftWrap(config_, utf8, state);
    const bool soft_wrap = state.line != before_wrap.line &&
                           state.offset == before_wrap.offset;
    if (soft_wrap && consider(before_wrap.offset, before_wrap.line,
                              before_wrap.x, CaretAffinity::Upstream)) {
      return result;
    }
    if (consider(state.offset, state.line, state.x,
                 CaretAffinity::Downstream)) {
      return result;
    }
    if (state.line > visual_line) {
      break;
    }

    if (state.offset >= utf8.size()) break;
    const std::size_t before = state.offset;
    advanceFlowAfterPending(config_, utf8, state);
    if (bytes_visited != nullptr) *bytes_visited += state.offset - before;
  }

  if (!saw_target_line || !have_best) return result;
  result.byte_offset = best_byte;
  result.line = visual_line;
  result.x = best_x;
  result.exists = true;
  result.clamped = best_x < preferred_x;
  result.affinity = best_affinity;
  return result;
}

std::uint16_t LayoutEngine::visualLineCount(std::string_view utf8) const noexcept {
  FlowState state;
  while (state.offset < utf8.size()) advanceFlow(config_, utf8, state);
  return static_cast<std::uint16_t>(state.line + 1U);
}

LayoutResult LayoutEngine::layoutText(std::string_view utf8,
                                      std::size_t cursor_byte,
                                      std::uint16_t first_visible_line,
                                      Placement* output,
                                      std::size_t capacity) const noexcept {
  cursor_byte = std::min(cursor_byte, utf8.size());
  const auto cursor = visualCaretAt(utf8, cursor_byte);
  return layoutTextWithKnownCaret(utf8, cursor, first_visible_line, output, capacity);
}

LayoutResult LayoutEngine::layoutTextWithKnownCaret(
    std::string_view utf8, const VisualCaretPosition& cursor,
    std::uint16_t first_visible_line, Placement* output,
    std::size_t capacity) const noexcept {
  LayoutResult result;
  result.visible_lines = visibleTextLineCount();

  if (cursor.valid && cursor.byte_offset <= utf8.size() &&
      isByteBoundary(utf8, cursor.byte_offset)) {
    result.cursor_visible = cursor.line >= first_visible_line &&
                            cursor.line < first_visible_line + result.visible_lines;
    result.cursor_x = cursor.x;
    if (result.cursor_visible) {
      result.cursor_y = static_cast<std::uint16_t>(
          (cursor.line - first_visible_line) * config_.line_height_px);
    }
  }

  FlowState state;
  while (state.offset < utf8.size()) {
    applyPendingSoftWrap(config_, utf8, state);
    const auto item = decode(utf8, state.offset);
    if (item.codepoint == '\n') {
      advanceFlowAfterPending(config_, utf8, state);
      continue;
    }

    const auto advance = advanceFor(config_, item.codepoint);
    if (state.line >= first_visible_line &&
        state.line < first_visible_line + result.visible_lines) {
      emit(result,
           {item.codepoint, state.offset, state.x,
            static_cast<std::uint16_t>(
                (state.line - first_visible_line) * config_.line_height_px),
            advance, state.line, false},
           output, capacity);
    }
    advanceFlowAfterPending(config_, utf8, state);
  }

  result.total_lines = static_cast<std::uint16_t>(state.line + 1U);
  result.output_truncated = result.placements_written < result.placements_required;
  return result;
}

LayoutResult LayoutEngine::layoutCandidates(const std::string_view* candidates,
                                            std::size_t count,
                                            std::size_t selected,
                                            Placement* output,
                                            std::size_t capacity) const noexcept {
  LayoutResult result;
  result.visible_lines = config_.candidate_bar_height_px / config_.line_height_px;
  const auto y = static_cast<std::uint16_t>(
      config_.height_px - config_.candidate_bar_height_px);
  std::uint16_t x = 0;
  for (std::size_t candidate = 0; candidate < count && candidate < 9; ++candidate) {
    const char label[3] = {static_cast<char>('1' + candidate), '.', '\0'};
    for (std::size_t offset = 0; offset < 2;) {
      const auto item = decode(std::string_view(label, 2), offset);
      const auto advance = config_.ascii_advance_px;
      if (x + advance > config_.width_px) {
        result.output_truncated = true;
        return result;
      }
      emit(result,
           {item.codepoint, offset, x, y, advance, 0,
            candidate == selected, true},
           output, capacity);
      x = static_cast<std::uint16_t>(x + advance);
      offset += item.bytes;
    }
    for (std::size_t offset = 0; offset < candidates[candidate].size();) {
      const auto item = decode(candidates[candidate], offset);
      const auto advance = advanceFor(config_, item.codepoint);
      if (x + advance > config_.width_px) {
        result.output_truncated = true;
        return result;
      }
      emit(result,
           {item.codepoint, offset, x, y, advance, 0,
            candidate == selected, false},
           output, capacity);
      x = static_cast<std::uint16_t>(x + advance);
      offset += item.bytes;
    }
    if (x + config_.ascii_advance_px <= config_.width_px) {
      x = static_cast<std::uint16_t>(x + config_.ascii_advance_px);
    }
  }
  result.total_lines = 1;
  result.output_truncated = result.output_truncated ||
                            result.placements_written < result.placements_required;
  return result;
}

LayoutResult LayoutEngine::layoutCandidatePeek(const std::string_view candidate,
                                               const std::uint16_t start_x,
                                               Placement* output,
                                               const std::size_t capacity) const noexcept {
  LayoutResult result;
  result.visible_lines = config_.candidate_bar_height_px / config_.line_height_px;
  result.total_lines = 1;
  const auto y = static_cast<std::uint16_t>(
      config_.height_px - config_.candidate_bar_height_px);

  std::uint16_t x = start_x;
  std::size_t emitted_glyphs = 0;
  for (std::size_t offset = 0; offset < candidate.size();) {
    if (emitted_glyphs >= 1) {
      result.output_truncated = true;
      return result;
    }
    const auto item = decode(candidate, offset);
    const auto advance = advanceFor(config_, item.codepoint);
    if (x + advance > config_.width_px) {
      result.output_truncated = true;
      return result;
    }
    emit(result,
         {item.codepoint, offset, x, y, advance, 0, false},
         output, capacity);
    x = static_cast<std::uint16_t>(x + advance);
    offset += item.bytes;
    ++emitted_glyphs;
  }

  result.output_truncated = result.placements_written < result.placements_required;
  return result;
}

}  // namespace hanime::renderer
