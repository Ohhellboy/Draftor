#pragma once

#include "types.hpp"

#include <cstdint>

namespace hanime::documents {

struct EditorView {
  std::string_view text;
  std::size_t cursor_byte = 0;
  VisualAnchor viewport{};
  std::uint64_t revision = 0;
  std::uint64_t body_revision = 0;
};

// The host owns the only editable body. capture() must return one consistent
// body/cursor/visual-anchor revision. install() is transactional: on failure it
// leaves both the old document and prepared snapshot unchanged; on success it
// consumes/swaps the prepared body, clears selection, undo/redo and
// document-specific caches while preserving clipboard by host policy.
class EditorPort {
 public:
  virtual ~EditorPort() = default;
  // The view remains valid while the synchronous document operation owns the
  // editor input gate. This avoids a second 64 KiB body copy during saves.
  virtual Status capture(EditorView* out) const = 0;
  virtual std::uint64_t revision() const noexcept = 0;
  // Changes only when the body text changes.  Navigation-only revisions must
  // not invalidate the persisted-body credential used by position records.
  virtual std::uint64_t bodyRevision() const noexcept = 0;
  virtual bool compositionActive() const noexcept = 0;
  virtual bool inputQueuePending() const noexcept = 0;
  virtual Status install(Snapshot* prepared) = 0;
  virtual void onDocumentActivated(const DocumentId&) noexcept {}
  virtual void onBodyCommitted(std::uint64_t body_revision, SaveLane lane) = 0;
};

struct SaveGates {
  bool storage_granted = true;
  bool composition_active = false;
  bool input_queue_pending = false;
};

}  // namespace hanime::documents
