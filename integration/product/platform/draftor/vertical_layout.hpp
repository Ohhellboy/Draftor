#pragma once

#include "layout.hpp"
#include "src/hanime_document/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace draftor::vertical {

using Affinity = hanime::renderer::CaretAffinity;

enum class Status : std::uint8_t {
  MoreWork, OutputFull, DocumentEnd, Ready, Stale, InvalidGeometry,
  InvalidUtf8, InvalidLiteral, InvalidPosition, CapacityExceeded, Boundary,
  Disabled, StaleLiteral, NeedsLiteralPolicy, NeedsLayout
};

struct Rect {
  std::uint16_t x = 0, y = 0, width = 0, height = 0;
};

struct Config {
  Rect body{0, 0, 284, 306};
  std::uint16_t column_width_px = 28;
  std::uint16_t column_gap_px = 6;
  std::uint16_t upright_advance_px = 28;
  std::uint16_t literal_advance_px = 14;
  // Used only for Literal's horizontal advance, before clockwise rotation.
  // Callback/context must remain valid and immutable until geometry changes.
  hanime::renderer::CodepointAdvance literal_advance = nullptr;
  const void* metrics_context = nullptr;
  // A/B can anchor beside the active column; ordinary readers keep their viewport.
  bool focus_at_left = false;
};

struct Stamp {
  hanime::documents::DocumentId document_id{};
  std::uint64_t session = 0;
  std::uint64_t mount_epoch = 0;
  std::uint32_t text_revision = 0;
  std::uint64_t geometry_revision = 0;
  std::uint64_t literal_revision = 0;
  bool operator==(const Stamp& other) const noexcept;
  bool operator!=(const Stamp& other) const noexcept { return !(*this == other); }
};

struct ByteRange {
  std::size_t begin = 0, end = 0;  // Half-open UTF-8 byte offsets.
};

// Ambiguous ASCII grouping is an explicit adapter policy, not a font rule.
enum class JoinRule : std::uint8_t { Unresolved, Separate, Join };
struct LiteralRules {
  JoinRule punctuation = JoinRule::Unresolved;
  JoinRule spaces = JoinRule::Unresolved;
  // Opt-in display policy. Group membership and glyph direction are distinct:
  // upright @/# still belong to their entire word for measurement/wrapping.
  bool font_space_advance = false;
  bool upright_at_hash = false;
};
enum class BodyPolicy : std::uint8_t { LegacyUnresolved, LatinWordsUprightAtHash };
constexpr LiteralRules bodyPolicyRules(BodyPolicy policy) noexcept {
  return policy == BodyPolicy::LatinWordsUprightAtHash
      ? LiteralRules{JoinRule::Join, JoinRule::Separate, true, true}
      : LiteralRules{};
}
// Returns 1 for a member, 0 for a separator, -1 for an unresolved rule.
// Shared input classification is also needed by horizontal-only consumers.
inline int literalClass(const unsigned char byte, const LiteralRules rules) noexcept {
  if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
      (byte >= '0' && byte <= '9')) return 1;
  if (byte < 0x20U || byte > 0x7eU) return 0;
  const auto rule = byte == ' ' ? rules.spaces : rules.punctuation;
  if (rule == JoinRule::Unresolved) return -1;
  return rule == JoinRule::Join ? 1 : 0;
}

struct Input {
  std::string_view text;
  const ByteRange* literals = nullptr;
  std::size_t literal_count = 0;
  Stamp stamp{};
  bool automatic_literals = false;
  LiteralRules rules{};
  // All views are borrowed for ONE call. The scanner retains no text pointer.
  // Explicit ranges are sorted/disjoint/nonempty printable ASCII. Automatic
  // mode scans the borrowed text using rules instead, with no range array.
  // Neither mode changes input text or implements Unicode grapheme rules.
};

struct Position {
  std::size_t byte = 0;
  Affinity affinity = Affinity::Downstream;
};

struct Caret {
  Position position{};
  std::uint32_t column = 0;  // Logical visual-column index, increasing leftward.
  std::uint16_t y_px = 0;    // Within the column, including its bottom boundary.
};

enum class EventKind : std::uint8_t { Upright, RotateCW90, SoftBreak, Newline, End };

struct Event {
  std::size_t begin_byte = 0, end_byte = 0;
  std::uint32_t column = 0, codepoint = 0;
  std::uint16_t y_px = 0, advance_px = 0;
  EventKind kind = EventKind::End;
  Caret before() const noexcept;
  Caret after() const noexcept;
  bool glyph() const noexcept;
};

struct Step {
  Status status = Status::MoreWork;
  std::size_t written = 0, work = 0, bytes_read = 0;
};

// Incremental, allocation-free geometry. One work unit is one range descriptor,
// one scanned/measured ASCII byte, or one event/decoded scalar (at most four bytes).
// OutputFull and MoreWork preserve the exact continuation in this object.
// DocumentEnd is the only result claiming the entire input was processed.
class Flow {
 public:
  Status start(const Input& input, Config config) noexcept;
  Step step(const Input& input, Event* output, std::size_t capacity,
            std::size_t work_budget) noexcept;
  std::size_t nextByte() const noexcept { return byte_; }
  std::uint32_t column() const noexcept { return column_; }

 private:
  friend class Viewport;
  void resumeUnchangedPrefix(std::size_t byte, std::uint32_t column, bool long_word=false) noexcept;
  Status fail(Status status) noexcept;
  std::uint16_t literalAdvance(std::uint32_t cp, LiteralRules rules) const noexcept;
  Config config_{};
  Stamp stamp_{};
  std::size_t text_size_ = 0, literal_count_ = 0;
  std::size_t byte_ = 0, checked_ranges_ = 0, range_index_ = 0;
  std::size_t measure_byte_ = 0;
  std::uint32_t column_ = 0, measured_width_ = 0;
  std::uint16_t y_px_ = 0;
  ByteRange automatic_range_{};
  std::size_t scan_byte_ = 0;
  bool scanning_ = false;
  bool measuring_ = false, range_ready_ = false, long_literal_ = false;
  Status status_ = Status::MoreWork;
};

#ifndef DRAFTOR_VERTICAL_EVENT_CAPACITY
#define DRAFTOR_VERTICAL_EVENT_CAPACITY 256
#endif

enum class Direction : std::uint8_t { Up, Down, Left, Right };

struct Mapping {
  Status status = Status::InvalidPosition;
  Caret caret{};
};

struct Rectangles {
  Status status = Status::Ready;
  std::size_t required = 0, written = 0;
};

// Owns a bounded rolling window and 32 prefix checkpoints, never a body copy.
// start/service invalidate the previous frame. Queries accept a current stamp;
// stale frames cannot be used for movement or painting. Event values are copied.
class Viewport {
 public:
  static constexpr std::size_t event_capacity = DRAFTOR_VERTICAL_EVENT_CAPACITY;
  static constexpr std::size_t anchor_capacity = 32;
  Status start(const Input& input, Config config, Position focus,
               bool unchanged_prefix = false, std::size_t edit_begin = 0) noexcept;
  Step service(const Input& input, std::size_t work_budget) noexcept;
  // Rebuild a missing neighbor with the SAME scanner/window. No cursor move.
  Status renewNavigation(const Input&, Position, Direction) noexcept;
  void invalidate() noexcept;
  Status status(const Stamp& current) const noexcept;
  Mapping locate(const Stamp& current, Position position) const noexcept;
  Mapping hitTest(const Stamp& current, std::uint32_t column,
                  std::uint16_t preferred_y_px) const noexcept;
  Mapping move(const Stamp& current, Position from, Direction direction,
               std::uint16_t preferred_y_px) const noexcept;
  Rectangles rectangles(const Input& input, ByteRange range, Rect* output,
                        std::size_t capacity) const noexcept;
  void firstColumn(std::uint32_t column) noexcept {first_column_=column;}
  Status scrollTo(Position anchor) noexcept;
  Rect glyphRect(const Event& event) const noexcept;
  Rect caretRect(const Caret& caret) const noexcept;
  bool visible(std::uint32_t column) const noexcept;
  std::uint32_t firstColumn() const noexcept { return first_column_; }
  std::uint16_t visibleColumns() const noexcept { return visible_columns_; }
  std::size_t eventCount() const noexcept { return count_; }
  Event eventAt(std::size_t index) const noexcept;
  // Cumulative scanner work (unsigned wrap), independent of viewport restarts.
  std::size_t totalWork() const noexcept { return total_work_; }
  std::size_t resumedAtByte() const noexcept { return resumed_at_byte_; }

 private:
  struct Anchor { std::size_t byte = 0; std::uint32_t column = 0; bool valid = false, long_word = false; };
  Mapping locateUnchecked(Position position) const noexcept;
  void remember(Anchor anchor) noexcept;
  bool push(Event event) noexcept;
  void dropFirstColumn() noexcept;
  void ensureFocusVisible(const Caret& caret) noexcept;
  Flow flow_{};
  Config config_{};
  Stamp stamp_{};
  Position focus_{};
  std::array<Event, event_capacity> events_{};
  std::array<Anchor, anchor_capacity> anchors_{};
  std::size_t head_ = 0, count_ = 0, next_anchor_ = 0;
  std::size_t total_work_ = 0, resumed_at_byte_ = 0;
  std::uint32_t first_column_ = 0;
  std::uint16_t visible_columns_ = 0;
  bool focus_found_ = false, document_end_ = false;
  Status status_ = Status::Stale;
};

const char* statusName(Status status) noexcept;
bool validGeometry(const Config& config) noexcept;

}  // namespace draftor::vertical
