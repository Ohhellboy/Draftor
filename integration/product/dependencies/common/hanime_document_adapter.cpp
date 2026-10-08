#include "hanime_document_adapter.hpp"

#include "utf8.hpp"

namespace hanime::find::integration {

HanimeDocumentAdapter::HanimeDocumentAdapter(
    const editor::Document& document,
    editor::body_edit::BodyEditor& body_editor,
    editor::presentation::PresentationPort& presentation,
    const hanime::documents::DocumentId& document_id,
    const std::uint64_t document_session,
    const std::uint64_t mount_epoch) noexcept
    : document_(document),
      body_editor_(body_editor),
      presentation_(presentation),
      document_id_(document_id),
      document_session_(document_session),
      mount_epoch_(mount_epoch) {
  presentation_.followCaret(body_editor_.caretAffinity(), false);
}

SourceStamp HanimeDocumentAdapter::sourceStamp() const noexcept {
  return {document_id_, document_session_, mount_epoch_,
          document_.textRevision()};
}

bool HanimeDocumentAdapter::validRange(const std::size_t begin,
                                       const std::size_t end) const noexcept {
  return begin <= end && end <= document_.text().size() &&
         editor::isUtf8Boundary(document_.text(), begin) &&
         editor::isUtf8Boundary(document_.text(), end);
}

void HanimeDocumentAdapter::noteHighlight(const bool visible,
                                          const std::size_t begin,
                                          const std::size_t end) noexcept {
  if (highlight_.visible == visible && highlight_.begin == begin &&
      highlight_.end == end) return;
  highlight_.visible = visible;
  highlight_.begin = begin;
  highlight_.end = end;
  ++highlight_.version;
}

bool HanimeDocumentAdapter::beginFindSession() noexcept {
  const auto cancelled = body_editor_.cancelSelection();
  if (cancelled.code == editor::body_edit::ResultCode::InvalidPosition ||
      cancelled.code == editor::body_edit::ResultCode::ResourceExhausted) {
    return false;
  }
  noteHighlight(false, 0U, 0U);
  presentation_.followCaret(body_editor_.caretAffinity(), false);
  return true;
}

bool HanimeDocumentAdapter::revealMatch(const std::size_t begin,
                                        const std::size_t end) noexcept {
  if (!validRange(begin, end) || begin == end) return false;
  const auto cancelled = body_editor_.cancelSelection();
  if (cancelled.code == editor::body_edit::ResultCode::InvalidPosition ||
      cancelled.code == editor::body_edit::ResultCode::ResourceExhausted) {
    return false;
  }
  const auto moved = body_editor_.moveToByte(begin);
  if (moved.code == editor::body_edit::ResultCode::InvalidPosition ||
      moved.code == editor::body_edit::ResultCode::ResourceExhausted) return false;
  presentation_.followCaret(body_editor_.caretAffinity());
  noteHighlight(true, begin, end);
  return true;
}

bool HanimeDocumentAdapter::placeCaretAtByte(
    const std::size_t byte_offset) noexcept {
  if (!validRange(byte_offset, byte_offset)) return false;
  const auto cancelled = body_editor_.cancelSelection();
  if (cancelled.code == editor::body_edit::ResultCode::InvalidPosition ||
      cancelled.code == editor::body_edit::ResultCode::ResourceExhausted) {
    return false;
  }
  const auto moved = body_editor_.moveToByte(byte_offset);
  if (moved.code == editor::body_edit::ResultCode::InvalidPosition ||
      moved.code == editor::body_edit::ResultCode::ResourceExhausted) return false;
  presentation_.followCaret(body_editor_.caretAffinity());
  noteHighlight(false, 0U, 0U);
  return document_.cursorByte() == byte_offset;
}

void HanimeDocumentAdapter::clearMatchHighlight() noexcept {
  noteHighlight(false, 0U, 0U);
}

bool HanimeDocumentAdapter::moveDocument(
    const editor::CursorMove direction) noexcept {
  clearMatchHighlight();
  const auto outcome = body_editor_.moveToByte(document_.cursorTarget(direction));
  if (outcome.code == editor::body_edit::ResultCode::InvalidPosition ||
      outcome.code == editor::body_edit::ResultCode::ResourceExhausted) {
    return false;
  }
  // The shared position is authoritative, including same-byte affinity changes.
  presentation_.followCaret(body_editor_.caretAffinity());
  const std::size_t expected = direction == editor::CursorMove::DocumentStart
      ? 0U : document_.text().size();
  return document_.cursorByte() == expected;
}

bool HanimeDocumentAdapter::moveDocumentStart() noexcept {
  return moveDocument(editor::CursorMove::DocumentStart);
}

bool HanimeDocumentAdapter::moveDocumentEnd() noexcept {
  return moveDocument(editor::CursorMove::DocumentEnd);
}

void HanimeDocumentAdapter::rebind(
    const hanime::documents::DocumentId& document_id,
    const std::uint64_t document_session,
    const std::uint64_t mount_epoch) noexcept {
  document_id_ = document_id;
  document_session_ = document_session;
  mount_epoch_ = mount_epoch;
  clearMatchHighlight();
  presentation_.followCaret(body_editor_.caretAffinity());
}

bool HanimeDocumentAdapter::bindingMatches(
    const hanime::documents::DocumentId& document_id,
    const std::uint64_t document_session,
    const std::uint64_t mount_epoch) const noexcept {
  return document_id_ == document_id &&
         document_session_ == document_session &&
         mount_epoch_ == mount_epoch;
}

}  // namespace hanime::find::integration
