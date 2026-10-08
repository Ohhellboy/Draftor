#include "draftor/vertical_layout.hpp"
#include "utf8.hpp"

#include <limits>

namespace draftor::vertical {
namespace {

bool printableAscii(const unsigned char byte) noexcept {
  return byte >= 0x20U && byte <= 0x7eU;
}

// The existing editor validates scalars, not grapheme clusters. Validate only
// this bounded slice, then decode it; never call a full-body validator per step.
bool decode(std::string_view text, std::size_t offset,
            std::uint32_t& cp, std::size_t& length) noexcept {
  const auto lead = static_cast<unsigned char>(text[offset]);
  length = lead < 0x80U ? 1U : lead < 0xe0U ? 2U : lead < 0xf0U ? 3U : 4U;
  if (length > text.size() - offset ||
      !editor::isValidUtf8(text.substr(offset, length))) {
    return false;
  }
  cp = length == 1U ? lead : lead & ((1U << (7U - length)) - 1U);
  for (std::size_t i = 1; i < length; ++i) {
    cp = (cp << 6U) | (static_cast<unsigned char>(text[offset + i]) & 0x3fU);
  }
  return true;
}

}  // namespace


bool Stamp::operator==(const Stamp& other) const noexcept {
  return document_id == other.document_id && session == other.session &&
         mount_epoch == other.mount_epoch && text_revision == other.text_revision &&
         geometry_revision == other.geometry_revision &&
         literal_revision == other.literal_revision;
}

bool validGeometry(const Config& c) noexcept {
  const auto max = std::numeric_limits<std::uint16_t>::max();
  return c.body.width > 0 && c.body.height > 0 && c.column_width_px > 0 &&
         c.column_width_px <= c.body.width && c.upright_advance_px > 0 &&
         c.upright_advance_px <= c.body.height && c.literal_advance_px > 0 &&
         static_cast<unsigned>(c.body.x) + c.body.width <= max &&
         static_cast<unsigned>(c.body.y) + c.body.height <= max;
}

Caret Event::before() const noexcept {
  return {{begin_byte, Affinity::Downstream}, column, y_px};
}

Caret Event::after() const noexcept {
  if (kind == EventKind::SoftBreak || kind == EventKind::Newline) {
    return {{end_byte, Affinity::Downstream}, column + 1U, 0};
  }
  return {{end_byte, Affinity::Upstream}, column,
          static_cast<std::uint16_t>(y_px + advance_px)};
}

bool Event::glyph() const noexcept {
  return kind == EventKind::Upright || kind == EventKind::RotateCW90;
}

Status Flow::start(const Input& input, const Config config) noexcept {
  *this = Flow{};
  config_ = config;
  stamp_ = input.stamp;
  text_size_ = input.text.size();
  literal_count_ = input.literal_count;
  if (!validGeometry(config)) return fail(Status::InvalidGeometry);
  if (input.automatic_literals && input.literal_count != 0) return fail(Status::InvalidLiteral);
  if (input.literal_count != 0 && input.literals == nullptr) {
    return fail(Status::InvalidLiteral);
  }
  return status_;
}

void Flow::resumeUnchangedPrefix(const std::size_t byte,
                                const std::uint32_t column, const bool long_word) noexcept {
  // Only Viewport may reuse this prefix, and only when no Literal lookahead
  // can reach across the edit. Public continuations use the intact Flow object.
  byte_ = byte;
  column_ = column;
  if(long_word){
    // This canonical soft-break checkpoint is inside an already overlong
    // automatic word. Its unchanged prefix proves per-glyph wrapping; scan
    // only the remaining suffix, never remeasure the whole document word.
    scanning_=true;scan_byte_=byte;long_literal_=range_ready_=true;
  }
}

Status Flow::fail(const Status status) noexcept {
  status_ = status;
  return status;
}

std::uint16_t Flow::literalAdvance(const std::uint32_t cp, const LiteralRules rules) const noexcept {
  // Upright exceptions use an upright glyph's height, including when measuring
  // the containing block. This prevents overlap without inserting whitespace.
  if (rules.upright_at_hash && (cp == '@' || cp == '#')) return config_.upright_advance_px;
  return config_.literal_advance
      ? config_.literal_advance(config_.metrics_context, cp)
      : config_.literal_advance_px;
}

Step Flow::step(const Input& input, Event* const output,
                const std::size_t capacity, const std::size_t work_budget) noexcept {
  Step result;
  if (input.stamp != stamp_ || input.text.size() != text_size_ ||
      input.literal_count != literal_count_) {
    result.status = fail(Status::Stale);
    return result;
  }
  if (status_ != Status::MoreWork) {
    result.status = status_;
    return result;
  }
  if ((capacity != 0 && output == nullptr) ||
      (literal_count_ != 0 && input.literals == nullptr)) {
    result.status = fail(Status::InvalidLiteral);
    return result;
  }

  while (result.work < work_budget) {
    if (result.written == capacity) {
      result.status = Status::OutputFull;
      return result;
    }
    ++result.work;

    if (checked_ranges_ < literal_count_) {
      const auto range = input.literals[checked_ranges_];
      if (range.begin >= range.end || range.end > text_size_ ||
          (checked_ranges_ != 0 && input.literals[checked_ranges_ - 1U].end > range.begin) ||
          !editor::isUtf8Boundary(input.text, range.begin) ||
          !editor::isUtf8Boundary(input.text, range.end)) {
        result.status = fail(Status::InvalidLiteral);
        return result;
      }
      ++checked_ranges_;
      continue;
    }

    if (scanning_) {
      const int member = scan_byte_ == text_size_ ? 0 :
          literalClass(static_cast<unsigned char>(input.text[scan_byte_]), input.rules);
      if (scan_byte_ != text_size_) ++result.bytes_read;
      if (member < 0) {
        result.status = fail(Status::NeedsLiteralPolicy);
        return result;
      }
      if (member == 0) {
        automatic_range_ = {byte_, scan_byte_};
        scanning_ = false;
      } else {
        ++scan_byte_;
      }
      continue;
    }
    const auto range = input.automatic_literals ? automatic_range_ :
        range_index_ < literal_count_ ? input.literals[range_index_] : ByteRange{};

    if (measuring_) {
      const auto cp = static_cast<unsigned char>(input.text[measure_byte_]);
      ++result.bytes_read;
      const auto advance = literalAdvance(cp, input.rules);
      if (!printableAscii(cp)) {
        result.status = fail(Status::InvalidLiteral);
        return result;
      }
      if (advance == 0 || advance > config_.body.height) {
        result.status = fail(Status::InvalidGeometry);
        return result;
      }
      measured_width_ += advance;
      ++measure_byte_;
      long_literal_ = measured_width_ > config_.body.height;
      if (long_literal_ || measure_byte_ == range.end) {
        measuring_ = false;
        range_ready_ = true;
      }
      continue;
    }

    if (byte_ == text_size_) {
      output[result.written++] = {byte_, byte_, column_, 0, y_px_, 0, EventKind::End};
      result.status = fail(Status::DocumentEnd);
      return result;
    }

    if (input.automatic_literals && byte_ >= automatic_range_.end) {
      const int member = literalClass(static_cast<unsigned char>(input.text[byte_]), input.rules);
      if (member < 0) {
        result.status = fail(Status::NeedsLiteralPolicy);
        return result;
      }
      if (member != 0) {
        scan_byte_ = byte_;
        scanning_ = true;
        continue;
      }
    }
    const bool in_literal = byte_ >= range.begin && byte_ < range.end;
    if (in_literal && !range_ready_) {
      measure_byte_ = byte_;
      measured_width_ = 0;
      measuring_ = true;
      continue;
    }

    std::uint32_t cp = 0;
    std::size_t length = 0;
    if (!decode(input.text, byte_, cp, length)) {
      result.bytes_read += input.text.size() - byte_ < 4U ? input.text.size() - byte_ : 4U;
      result.status = fail(Status::InvalidUtf8);
      return result;
    }
    result.bytes_read += length;
    if (in_literal && (length != 1U || !printableAscii(static_cast<unsigned char>(cp)))) {
      result.status = fail(Status::InvalidLiteral);
      return result;
    }

    if (cp == '\n') {
      if (column_ == std::numeric_limits<std::uint32_t>::max()) {
        result.status = fail(Status::CapacityExceeded);
        return result;
      }
      output[result.written++] = {byte_, byte_ + length, column_, cp, y_px_, 0, EventKind::Newline};
      byte_ += length;
      ++column_;
      y_px_ = 0;
      continue;
    }

    const bool font_space = cp == ' ' && input.rules.font_space_advance;
    const auto advance = in_literal || font_space
        ? literalAdvance(cp, input.rules) : config_.upright_advance_px;
    if (advance == 0 || advance > config_.body.height) {
      result.status = fail(Status::InvalidGeometry);
      return result;
    }
    const bool whole_literal_wrap = in_literal && !long_literal_ &&
        byte_ == range.begin &&
        y_px_ + measured_width_ > config_.body.height;
    if (y_px_ != 0 && (whole_literal_wrap || y_px_ + advance > config_.body.height)) {
      if (column_ == std::numeric_limits<std::uint32_t>::max()) {
        result.status = fail(Status::CapacityExceeded);
        return result;
      }
      // A soft break has TWO carets at the SAME byte. Do not insert a newline,
      // and do not eagerly wrap an exactly full last column at document end.
      output[result.written++] = {byte_, byte_, column_, 0, y_px_, 0, EventKind::SoftBreak};
      ++column_;
      y_px_ = 0;
      continue;
    }

    const bool upright_exception = input.rules.upright_at_hash && (cp == '@' || cp == '#');
    output[result.written++] = {byte_, byte_ + length, column_, cp, y_px_, advance,
                               in_literal && !upright_exception ? EventKind::RotateCW90 : EventKind::Upright};
    byte_ += length;
    y_px_ = static_cast<std::uint16_t>(y_px_ + advance);
    if (in_literal && byte_ == range.end) {
      ++range_index_;
      range_ready_ = false;
      long_literal_ = false;
    }
  }
  return result;
}

const char* statusName(const Status status) noexcept {
  switch (status) {
    case Status::MoreWork: return "MoreWork";
    case Status::OutputFull: return "OutputFull";
    case Status::DocumentEnd: return "DocumentEnd";
    case Status::Ready: return "Ready";
    case Status::Stale: return "Stale";
    case Status::InvalidGeometry: return "InvalidGeometry";
    case Status::InvalidUtf8: return "InvalidUtf8";
    case Status::InvalidLiteral: return "InvalidLiteral";
    case Status::InvalidPosition: return "InvalidPosition";
    case Status::CapacityExceeded: return "CapacityExceeded";
    case Status::Boundary: return "Boundary";
    case Status::Disabled: return "Disabled";
    case Status::StaleLiteral: return "StaleLiteral";
    case Status::NeedsLiteralPolicy: return "NeedsLiteralPolicy";
    case Status::NeedsLayout: return "NeedsLayout";
  }
  return "Unknown";
}

}  // namespace draftor::vertical
