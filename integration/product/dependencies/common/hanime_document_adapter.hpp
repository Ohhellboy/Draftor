#pragma once

#include "body_editor.hpp"
#include "find_types.hpp"
#include "editor_presentation.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hanime::find::integration {

struct MatchHighlight {
  std::size_t begin = 0;
  std::size_t end = 0;
  std::uint32_t version = 0;
  bool visible = false;
};

// Shared Find binding: borrows the sole Document and BodyEditor.
// Logical positioning goes through BodyEditor; the active presentation follows.
// No visual geometry, text copy, selection, or history is owned here.
class HanimeDocumentAdapter final : public DocumentPort {
 public:
  HanimeDocumentAdapter(const editor::Document& document,
                        editor::body_edit::BodyEditor& body_editor,
                        editor::presentation::PresentationPort& presentation,
                        const hanime::documents::DocumentId& document_id,
                        std::uint64_t document_session,
                        std::uint64_t mount_epoch) noexcept;

  SourceStamp sourceStamp() const noexcept override;
  std::string_view text() const noexcept override { return document_.text(); }
  std::size_t cursorByte() const noexcept override {
    return document_.cursorByte();
  }
  bool beginFindSession() noexcept override;
  bool revealMatch(std::size_t begin, std::size_t end) noexcept override;
  bool placeCaretAtByte(std::size_t byte_offset) noexcept override;
  void clearMatchHighlight() noexcept override;

  bool moveDocumentStart() noexcept;
  bool moveDocumentEnd() noexcept;
  void rebind(const hanime::documents::DocumentId& document_id,
              std::uint64_t document_session,
              std::uint64_t mount_epoch) noexcept;
  bool bindingMatches(const hanime::documents::DocumentId& document_id,
                      std::uint64_t document_session,
                      std::uint64_t mount_epoch) const noexcept;

  MatchHighlight matchHighlight() const noexcept { return highlight_; }

 private:
  bool moveDocument(editor::CursorMove direction) noexcept;
  bool validRange(std::size_t begin, std::size_t end) const noexcept;
  void noteHighlight(bool visible, std::size_t begin, std::size_t end) noexcept;

  const editor::Document& document_;
  editor::body_edit::BodyEditor& body_editor_;
  editor::presentation::PresentationPort& presentation_;
  hanime::documents::DocumentId document_id_{};
  std::uint64_t document_session_ = 0;
  std::uint64_t mount_epoch_ = 0;
  MatchHighlight highlight_{};

};

}  // namespace hanime::find::integration
