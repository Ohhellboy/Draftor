#pragma once
#include <array>
#include "ime_composition_font.hpp"
#include "draftor/vertical_raster.hpp"
#include "hanime_extension/navigation.hpp"

namespace draftor::vertical {
enum class AbStatus : std::uint8_t { Ready, Stale, InvalidInput, CapacityExceeded };
enum class TextFlow : std::uint8_t { HorizontalSmall, VerticalBody, CompositionLatin };
enum class AbRole : std::uint8_t { Composition, CurrentCandidate, Preview, Peek, Selected };
enum class EdgeSide : std::uint8_t { None, StartRight, EndLeft };
struct AbRun {
  std::string_view text;
  Rect box{};
  TextFlow flow = TextFlow::HorizontalSmall;
  AbRole role = AbRole::Composition;
  unsigned number = 0;
  bool focused = false;
  // Borrowed full identity; only CurrentCandidate can be selected. Never retain
  // it beyond this call or select by string/slot without frame authorization.
  const void* identity = nullptr;
  s3app::CompositionTypography typography=s3app::CompositionTypography::Normal;
};
// Fixed presentation descriptors: no text/selection copy and no query state.
// Text and opaque identities are borrowed from the adapter for this one call.
// Consume and paint synchronously before any input/window/navigation mutation.
struct AbCandidateItem {
  std::string_view text;
  const void* identity = nullptr;
};
struct AbCandidateRow {
  std::array<AbCandidateItem, 6> items{};
  std::size_t index = 0, count = 0;
};
// The input owner treats a raw boundary as a right endpoint (Upstream).
// Candidate ranges instead use a downstream start and never draw a raw caret.
struct AbCompositionMark {
  enum class Affinity : std::uint8_t { Upstream, Downstream };
  std::uint16_t begin=0,end=0;
  bool candidate=false;
  Affinity affinity=Affinity::Upstream;
};
struct AbCandidateView {
  AbStatus status = AbStatus::Ready;
  std::array<AbCandidateRow, 3> rows{};
  std::size_t count = 0, row = 0, col = 0;
  bool expanded = false;
  AbCompositionMark mark{};
};
struct AbPort {
  void (*run)(void*, const AbRun&) = nullptr;
  void (*line)(void*, Rect, bool red) = nullptr;
  void* context = nullptr;
};
struct AbGeometry {
  std::uint16_t composition_width=s3app::ImeCompositionFontMetrics::minimum_width;
  std::uint8_t candidate_columns=1;
  bool selected_visible=false;
  Rect area{0, 0, 284, 306};
  std::uint16_t small_advance = 7, small_height = 14;
  std::uint16_t body_cell = 28, latin_advance = 14, gap = 2;
  // Kept for source compatibility; persistent geometry never depends on focus.
  std::uint32_t active_body_column = 0;
};
inline constexpr unsigned reserved_ime_capacity = 3;
inline constexpr Rect editor_area{8,22,284,356};
struct AbResult {
  AbStatus status = AbStatus::Ready;
  Rect body{}, selected{}, composition{}, candidates{}, separator{};
  std::size_t runs = 0, candidate_content_rows = 0;
};
// Pure reservation: no raw/candidate/nav/Document inputs, no query or allocation.
AbResult reservedAbRegions(AbGeometry = {}) noexcept;
// Consume snapshot and nav.view(handle, composition) in one serialized UI call.
// Cursor is the input owner's raw byte cursor, not Snapshot::focus.
// All callbacks are synchronous. No query, allocation, page copy or ownership.
AbResult layoutAb(const hanime_extension::Snapshot&, const hanime_extension::NavigationView&,
                  std::size_t raw_cursor, const AbCandidateView&, EdgeSide visible_edge,
                  AbGeometry, const AbPort&) noexcept;
// Resource preparation owns raw bytes but has no Composition or candidate authority.
// Same spatial contract and synchronous callback lifetime as layoutAb.
AbResult layoutPreparingAb(std::string_view raw, std::size_t cursor, AbGeometry,
                           const AbPort&) noexcept;
// Composition uses the shared fixed A font; small B labels and B glyphs retain
// their existing size/rotation. All text is borrowed for this synchronous call.
PaintResult paintAbRun(const AbRun&, const RasterPort&) noexcept;

}  // namespace draftor::vertical
