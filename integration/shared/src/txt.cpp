#include "draftor_exchange/txt.hpp"
#include <algorithm>
#include <limits>

namespace draftor::exchange {
bool Limits::checked(std::size_t b, Limits& out) noexcept {
  out = {};
  if (b > (std::numeric_limits<std::size_t>::max() - 3) / 2) return false;
  out = {b, 2 * b + 3};
  return true;
}
namespace detail {
TextStream::TextStream(std::size_t b, bool import_wire) noexcept
    : import_wire_(import_wire) {
  if (!Limits::checked(b, limits_)) fail(TextError::InvalidConfig, 0);
}
void TextStream::fail(TextError code, std::size_t offset) noexcept {
  if (failure_.code == TextError::None) failure_ = {code, offset};
}
bool TextStream::scalarComplete() noexcept {
  if (codepoint_ == 0) { fail(TextError::Nul, scalar_offset_); return false; }
  if (import_wire_ && scalar_offset_ == 0 && codepoint_ == 0xfeff) {
    have_ = width_ = 0;
    return true;
  }
  const bool omit = skip_lf_ && codepoint_ == '\n';
  skip_lf_ = codepoint_ == '\r';
  std::size_t length = omit ? 0 : (codepoint_ == '\r' ? 1 : width_);
  if (length > limits_.body - admitted_bytes_) {
    fail(TextError::BodyLimit, scalar_offset_);
    return false;
  }
  admitted_bytes_ += length;
  pending_size_ = static_cast<std::uint8_t>(length);
  pending_at_ = 0;
  if (codepoint_ == '\r' && length) pending_[0] = '\n';
  else for (std::size_t i = 0; i < length; ++i) pending_[i] = scalar_[i];
  have_ = width_ = 0;
  return true;
}
Step TextStream::feed(std::string_view input, Output output) noexcept {
  if (finishing_ && !input.empty()) fail(TextError::AfterFinish, source_bytes_);
  return process(input, output, false);
}
Step TextStream::finish(Output output) noexcept {
  return process({}, output, true);
}
Step TextStream::process(std::string_view input, Output output, bool finishing) noexcept {
  Step r;
  auto error = [&]() { r.flow = Flow::Error; r.failure = failure_; return r; };
  if (failure_.code != TextError::None) return error();
  if ((output.size && !output.data) || (input.size() && !input.data())) {
    fail(TextError::InvalidArgument, source_bytes_); return error();
  }
  finishing_ = finishing_ || finishing;
  if (done_) { r.flow = Flow::Done; return r; }
  while (true) {
    while (pending_at_ < pending_size_ && r.produced < output.size) {
      output.data[r.produced++] = pending_[pending_at_++];
      ++output_bytes_;
    }
    if (pending_at_ < pending_size_) { r.flow = Flow::NeedOutput; return r; }
    pending_at_ = pending_size_ = 0;
    if (r.consumed == input.size()) {
      if (!finishing_) { r.flow = Flow::NeedInput; return r; }
      if (have_) { fail(TextError::TruncatedUtf8, scalar_offset_); return error(); }
      done_ = true; r.flow = Flow::Done; return r;
    }
    const auto source_limit = import_wire_ ? limits_.wire : limits_.body;
    if (source_bytes_ == source_limit) {
      fail(TextError::InputLimit, source_bytes_); return error();
    }
    const auto ch = static_cast<std::uint8_t>(input[r.consumed]);
    const auto at = source_bytes_;
    ++r.consumed; ++source_bytes_;
    if (!have_) {
      scalar_offset_ = at;
      scalar_[0] = static_cast<char>(ch);
      have_ = 1;
      if (ch <= 0x7f) { width_ = 1; codepoint_ = ch; }
      else if (ch >= 0xc2 && ch <= 0xdf) { width_ = 2; codepoint_ = ch & 0x1f; }
      else if (ch >= 0xe0 && ch <= 0xef) { width_ = 3; codepoint_ = ch & 0x0f; }
      else if (ch >= 0xf0 && ch <= 0xf4) { width_ = 4; codepoint_ = ch & 7; }
      else { fail(TextError::InvalidUtf8, scalar_offset_); return error(); }
    } else {
      const auto lead = static_cast<std::uint8_t>(scalar_[0]);
      if (ch < 0x80 || ch > 0xbf ||
          (have_ == 1 && ((lead == 0xe0 && ch < 0xa0) ||
                         (lead == 0xed && ch > 0x9f) ||
                         (lead == 0xf0 && ch < 0x90) ||
                         (lead == 0xf4 && ch > 0x8f)))) {
        fail(TextError::InvalidUtf8, scalar_offset_); return error();
      }
      scalar_[have_++] = static_cast<char>(ch);
      codepoint_ = (codepoint_ << 6) | (ch & 0x3f);
    }
    if (have_ == width_ && !scalarComplete()) return error();
  }
}
} // namespace detail
} // namespace draftor::exchange

