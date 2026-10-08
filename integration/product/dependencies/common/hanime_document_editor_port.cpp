#include "hanime_document_editor_port.hpp"
#include "operation_profile.hpp"

namespace hanime::documents::integration {

HanimeDocumentEditorPort::HanimeDocumentEditorPort(
    editor::Document& document,
    editor::body_edit::BodyEditor& body_editor,
    editor::presentation::PresentationPort& presentation,
    const EditorPortHooks hooks) noexcept
    : document_(document),
      body_editor_(body_editor),
      presentation_(presentation),
      hooks_(hooks) {}

Affinity HanimeDocumentEditorPort::encodeAffinity(
    const hanime::renderer::CaretAffinity affinity) noexcept {
  return affinity == hanime::renderer::CaretAffinity::Upstream
      ? Affinity::Before
      : Affinity::After;
}

hanime::renderer::CaretAffinity HanimeDocumentEditorPort::decodeAffinity(
    const Affinity affinity) noexcept {
  return affinity == Affinity::Before
      ? hanime::renderer::CaretAffinity::Upstream
      : hanime::renderer::CaretAffinity::Downstream;
}

Status HanimeDocumentEditorPort::capture(EditorView* const out) const {
  if (!out) return Status::Conflict;
  editor::presentation::ViewportState viewport;
  if (!presentation_.captureViewport(viewport)) return Status::InvalidCursor;

  out->text = document_.text();
  out->cursor_byte = document_.cursorByte();
  out->viewport.byte_offset = viewport.byte_offset;
  out->viewport.affinity = encodeAffinity(viewport.affinity);
  out->viewport.layout_revision = viewport.layout_revision;
  out->revision = document_.revision();
  out->body_revision = document_.textRevision();
  return Status::Ok;
}

std::uint64_t HanimeDocumentEditorPort::revision() const noexcept {
  return document_.revision();
}

std::uint64_t HanimeDocumentEditorPort::bodyRevision() const noexcept {
  return document_.textRevision();
}

bool HanimeDocumentEditorPort::compositionActive() const noexcept {
  return hooks_.composition_active &&
      hooks_.composition_active(hooks_.context);
}

bool HanimeDocumentEditorPort::inputQueuePending() const noexcept {
  return hooks_.input_queue_pending &&
      hooks_.input_queue_pending(hooks_.context);
}

Status HanimeDocumentEditorPort::install(Snapshot* const prepared) {
  s3profile::Scope timing(s3profile::Install);
  if (!prepared) return Status::Conflict;
  if (prepared->text.size() > document_.maxBytes()) return Status::TooLarge;
  if (!isValidUtf8(prepared->text)) return Status::InvalidUtf8;
  if (!isUtf8Boundary(prepared->text, prepared->cursor_byte) ||
      !isUtf8Boundary(prepared->text, prepared->viewport.byte_offset)) {
    return Status::InvalidCursor;
  }

  const std::size_t viewport_byte = prepared->viewport.byte_offset;
  const auto affinity = decodeAffinity(prepared->viewport.affinity);
  if (!document_.replaceAllOwned(
          &prepared->text, prepared->cursor_byte, false)) {
    return Status::InstallFailed;
  }

  if (hooks_.after_install) hooks_.after_install(hooks_.context);

  // The body swap/reset is shared; only its byte anchor is presentation-owned.
  // Same-byte movement restores editor affinity as well as the visible caret.
  (void)body_editor_.moveToByte(document_.cursorByte(), false, affinity);
  presentation_.restoreViewport({viewport_byte, affinity, prepared->viewport.layout_revision});
  return Status::Ok;
}

void HanimeDocumentEditorPort::onDocumentActivated(const DocumentId& id) noexcept {
  if (id == active_id_) return;
  active_id_ = id;
  if (hooks_.after_identity_change) hooks_.after_identity_change(hooks_.context);
}

void HanimeDocumentEditorPort::onBodyCommitted(
    const std::uint64_t committed_body_revision,
    const SaveLane) {
  if (committed_body_revision == document_.textRevision()) {
    document_.markSaved();
  }
}

}  // namespace hanime::documents::integration
