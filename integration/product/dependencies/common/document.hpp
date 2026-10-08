#include "core_memory.hpp"
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace editor {

inline constexpr std::uint16_t kEditorCoreProtocolVersion = 1;

struct DocumentConfig {
    std::size_t max_bytes = 64U * 1024U;
    std::size_t viewport_rows = 12;
    std::size_t viewport_columns = 40;
};

enum class EditResult : std::uint8_t {
    Changed,
    NoChange,
    InvalidUtf8,
    CapacityExceeded,
    InvalidPosition,
    ResourceExhausted,
};

enum class CursorMove : std::uint8_t {
    Left,
    Right,
    Up,
    Down,
    LineStart,
    LineEnd,
    DocumentStart,
    DocumentEnd,
};

struct DocumentView {
    std::string_view text;
    std::size_t cursor_byte = 0;
    std::size_t cursor_line = 0;
    std::size_t cursor_column = 0;
    std::size_t viewport_top_line = 0;
    bool dirty = false;
};

// Read-only description of the most recent text mutation.  It is deliberately
// byte-oriented because Document already defines cursor/storage identity in
// UTF-8 byte offsets.  Rendering may use it to bound reflow; input, save and
// restore semantics never read it back.
struct DocumentTextEdit {
    std::uint32_t text_revision = 0;
    std::size_t begin = 0;
    std::size_t old_end = 0;
    std::size_t new_end = 0;
    bool incremental = false;
    bool line_break_changed = false;
};

class Document {
public:
    explicit Document(DocumentConfig config = {});

    EditResult insertUtf8(std::string_view text);
    EditResult insertLineBreak();
    EditResult eraseBackward();
    EditResult eraseForward();
    std::size_t cursorTarget(CursorMove direction) const noexcept;
    EditResult move(CursorMove direction);
    EditResult setCursorByte(std::size_t byte_offset);

    // Atomic range transaction used by BodyEditor. The prepare call validates
    // and reserves without changing text, cursor, or revision state.
    EditResult prepareReplaceRange(
        std::size_t begin,
        std::size_t end,
        std::string_view replacement,
        std::size_t cursor_after
    );
    EditResult replaceRange(
        std::size_t begin,
        std::size_t end,
        std::string_view replacement,
        std::size_t cursor_after
    );

    bool replaceAll(
        std::string_view text,
        std::size_t cursor_byte,
        bool dirty = false
    );
    // Installs an already-owned body by swapping buffers. On validation
    // failure neither this Document nor *owned_text changes. On success the
    // caller receives the previously installed body in *owned_text, avoiding
    // a third full-body allocation during cross-document restore.
    bool replaceAllOwned(
        draftor::memory::String* owned_text,
        std::size_t cursor_byte,
        bool dirty = false
    ) noexcept;
    void clear() noexcept;
    void markSaved() noexcept { dirty_ = false; }

    const draftor::memory::String& text() const noexcept { return text_; }
    std::size_t cursorByte() const noexcept { return cursor_byte_; }
    std::size_t maxBytes() const noexcept { return config_.max_bytes; }
    std::size_t viewportTopLine() const noexcept { return viewport_top_line_; }
    bool dirty() const noexcept { return dirty_; }
    std::uint32_t revision() const noexcept { return revision_; }
    std::uint32_t textRevision() const noexcept { return text_revision_; }
    // The transfer identity observes every wrap at the mutation owner.
    std::uint64_t textRevisionEpoch() const noexcept { return text_revision_epoch_; }
    DocumentTextEdit lastTextEdit() const noexcept { return last_text_edit_; }
    DocumentView view() const noexcept;

private:
    struct Position {
        std::size_t line = 0;
        std::size_t column = 0;
        std::size_t line_start = 0;
        std::size_t line_end = 0;
    };

    Position positionAt(std::size_t byte_offset) const noexcept;
    std::size_t byteAt(std::size_t line, std::size_t column) const noexcept;
    std::size_t lineCount() const noexcept;
    void moveCursorCacheTo(std::size_t byte_offset) noexcept;
    void ensureCursorVisible() noexcept;
    void finishReplaceAll(std::size_t cursor_byte, bool dirty) noexcept;

    DocumentConfig config_;
    draftor::memory::String text_;
    std::size_t cursor_byte_ = 0;
    std::size_t cursor_line_ = 0;
    std::size_t cursor_column_ = 0;
    std::size_t cursor_line_start_ = 0;
    std::size_t viewport_top_line_ = 0;
    std::size_t preferred_column_ = 0;
    bool dirty_ = false;
    std::uint32_t revision_ = 0;
    std::uint32_t text_revision_ = 0;
    std::uint64_t text_revision_epoch_ = 1;
    void advanceTextRevision() noexcept {
        if (++text_revision_ == 0 && text_revision_epoch_ != 0) ++text_revision_epoch_;
    }
    DocumentTextEdit last_text_edit_{};
};

}  // namespace editor
