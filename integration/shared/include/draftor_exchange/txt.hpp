#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace draftor::exchange {
enum class Flow { NeedInput, NeedOutput, Done, Error };
enum class TextError { None, InvalidConfig, InvalidArgument, InvalidUtf8,
                       TruncatedUtf8, Nul, InputLimit, BodyLimit, AfterFinish };
struct TextFailure {
  TextError code = TextError::None;
  std::size_t offset = 0; // source byte offset, never normalized-output offset
};
struct Step {
  std::size_t consumed = 0, produced = 0;
  Flow flow = Flow::NeedInput;
  TextFailure failure{};
};
struct Output { char* data; std::size_t size; };
struct Limits {
  std::size_t body = 0, wire = 0;
  static bool checked(std::size_t body_limit, Limits& out) noexcept;
};

namespace detail {
// No allocation. At most one UTF-8 scalar (4 bytes) awaits output.
class TextStream {
 public:
  Step feed(std::string_view input, Output output) noexcept;
  Step finish(Output output) noexcept;
  std::size_t sourceBytes() const noexcept { return source_bytes_; }
  std::size_t outputBytes() const noexcept { return output_bytes_; }
  TextFailure failure() const noexcept { return failure_; }
 protected:
  TextStream(std::size_t body_limit, bool import_wire) noexcept;
 private:
  Step process(std::string_view, Output, bool finishing) noexcept;
  void fail(TextError code, std::size_t offset) noexcept;
  bool scalarComplete() noexcept;
  Limits limits_{};
  std::size_t source_bytes_ = 0, output_bytes_ = 0, admitted_bytes_ = 0;
  std::size_t scalar_offset_ = 0;
  std::array<char, 4> scalar_{}, pending_{};
  std::uint32_t codepoint_ = 0;
  std::uint8_t have_ = 0, width_ = 0, pending_size_ = 0, pending_at_ = 0;
  bool import_wire_, skip_lf_ = false, finishing_ = false, done_ = false;
  TextFailure failure_{};
};
} // namespace detail

// Deliberately distinct types: a Document is never fed through ImportWire.
class ImportWire final : public detail::TextStream {
 public: explicit ImportWire(std::size_t body_limit) noexcept
      : TextStream(body_limit, true) {}
};
class ExportBody final : public detail::TextStream {
 public: explicit ExportBody(std::size_t body_limit) noexcept
      : TextStream(body_limit, false) {}
};
} // namespace draftor::exchange

