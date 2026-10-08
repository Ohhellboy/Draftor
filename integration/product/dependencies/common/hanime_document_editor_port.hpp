#pragma once

#include "document.hpp"
#include "src/hanime_document/editor_port.hpp"
#include "editor_presentation.hpp"

#include <cstdint>

namespace hanime::documents::integration {

struct EditorPortHooks {
  using Probe = bool (*)(void*) noexcept;
  using AfterInstall = void (*)(void*) noexcept;

  Probe composition_active = nullptr;
  Probe input_queue_pending = nullptr;
  AfterInstall after_install = nullptr;
  void* context = nullptr;
  AfterInstall after_identity_change = nullptr;
};

// Product bridge between the single editable Document and DocumentSystem.
// The host owns input serialization. after_install is invoked only after the
// body swap succeeds and is the integration point for selection, undo/redo,
// composition, candidate, and document-local cache reset.
class HanimeDocumentEditorPort final : public EditorPort {
 public:
  HanimeDocumentEditorPort(
      editor::Document& document,
      editor::body_edit::BodyEditor& body_editor,
      editor::presentation::PresentationPort& presentation,
      EditorPortHooks hooks = {}) noexcept;

  Status capture(EditorView* out) const override;
  std::uint64_t revision() const noexcept override;
  std::uint64_t bodyRevision() const noexcept override;
  bool compositionActive() const noexcept override;
  bool inputQueuePending() const noexcept override;
  Status install(Snapshot* prepared) override;
  void onDocumentActivated(const DocumentId& id) noexcept override;
  void onBodyCommitted(std::uint64_t body_revision, SaveLane lane) override;

 private:
  static Affinity encodeAffinity(
      hanime::renderer::CaretAffinity affinity) noexcept;
  static hanime::renderer::CaretAffinity decodeAffinity(
      Affinity affinity) noexcept;

  editor::Document& document_;
  editor::body_edit::BodyEditor& body_editor_;
  editor::presentation::PresentationPort& presentation_;
  EditorPortHooks hooks_{};
  DocumentId active_id_{};
};

}  // namespace hanime::documents::integration
