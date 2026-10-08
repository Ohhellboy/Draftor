#include "core_memory.hpp"
#include "document.hpp"

#include "utf8.hpp"

#include <algorithm>
#include <new>

namespace editor {

Document::Document(DocumentConfig config) : config_(config) {
    if (config_.viewport_rows == 0) config_.viewport_rows = 1;
    if (config_.viewport_columns == 0) config_.viewport_columns = 1;
    text_.reserve(std::min<std::size_t>(config_.max_bytes, 4096));
}

EditResult Document::insertUtf8(const std::string_view value) {
    if (value.empty()) return EditResult::NoChange;
    if (!isValidUtf8(value)) return EditResult::InvalidUtf8;
    if (value.size() > config_.max_bytes - text_.size()) {
        return EditResult::CapacityExceeded;
    }
    const std::size_t begin = cursor_byte_;
    text_.insert(cursor_byte_, value.data(), value.size());
    cursor_byte_ += value.size();
    for (std::size_t offset = 0; offset < value.size();) {
        if (value[offset] == '\n') {
            ++cursor_line_;
            cursor_column_ = 0;
            cursor_line_start_ = begin + offset + 1U;
            ++offset;
        } else {
            ++cursor_column_;
            offset = nextUtf8Boundary(value, offset);
        }
    }
    dirty_ = true;
    ++revision_;
    advanceTextRevision();
    last_text_edit_ = {text_revision_, begin, begin, cursor_byte_, true,
                       value.find('\n') != std::string_view::npos};
    preferred_column_ = cursor_column_;
    ensureCursorVisible();
    return EditResult::Changed;
}

EditResult Document::insertLineBreak() { return insertUtf8("\n"); }

EditResult Document::eraseBackward() {
    if (cursor_byte_ == 0) return EditResult::NoChange;
    const std::size_t begin = previousUtf8Boundary(text_, cursor_byte_);
    const std::size_t old_end = cursor_byte_;
    const bool removed_line_break = text_[begin] == '\n';
    moveCursorCacheTo(begin);
    text_.erase(begin, old_end - begin);
    cursor_byte_ = begin;
    dirty_ = true;
    ++revision_;
    advanceTextRevision();
    last_text_edit_ = {text_revision_, begin, old_end, begin, true,
                       removed_line_break};
    preferred_column_ = cursor_column_;
    ensureCursorVisible();
    return EditResult::Changed;
}

EditResult Document::eraseForward() {
    if (cursor_byte_ == text_.size()) return EditResult::NoChange;
    const std::size_t end = nextUtf8Boundary(text_, cursor_byte_);
    const bool removed_line_break = text_[cursor_byte_] == '\n';
    text_.erase(cursor_byte_, end - cursor_byte_);
    dirty_ = true;
    ++revision_;
    advanceTextRevision();
    last_text_edit_ = {text_revision_, cursor_byte_, end, cursor_byte_, true,
                       removed_line_break};
    preferred_column_ = cursor_column_;
    ensureCursorVisible();
    return EditResult::Changed;
}

std::size_t Document::cursorTarget(const CursorMove direction) const noexcept {
    const std::size_t delimiter = text_.find('\n', cursor_byte_);
    const Position position{cursor_line_, cursor_column_, cursor_line_start_,
                            delimiter == draftor::memory::String::npos
                                ? text_.size() : delimiter};
    std::size_t target = cursor_byte_;
    switch (direction) {
        case CursorMove::Left:
            target = previousUtf8Boundary(text_, cursor_byte_);
            break;
        case CursorMove::Right:
            target = nextUtf8Boundary(text_, cursor_byte_);
            break;
        case CursorMove::Up:
            if (position.line > 0) {
                const std::size_t previous_end = position.line_start - 1U;
                const std::size_t previous_delimiter = previous_end == 0
                    ? draftor::memory::String::npos
                    : text_.rfind('\n', previous_end - 1U);
                target = previous_delimiter == draftor::memory::String::npos
                    ? 0 : previous_delimiter + 1U;
                std::size_t column = 0;
                while (target < previous_end && column < preferred_column_) {
                    target = nextUtf8Boundary(text_, target);
                    ++column;
                }
            }
            break;
        case CursorMove::Down:
            if (position.line_end < text_.size()) {
                target = position.line_end + 1U;
                const std::size_t next_delimiter = text_.find('\n', target);
                const std::size_t next_end = next_delimiter == draftor::memory::String::npos
                    ? text_.size() : next_delimiter;
                std::size_t column = 0;
                while (target < next_end && column < preferred_column_) {
                    target = nextUtf8Boundary(text_, target);
                    ++column;
                }
            }
            break;
        case CursorMove::LineStart:
            target = position.line_start;
            break;
        case CursorMove::LineEnd:
            target = position.line_end;
            break;
        case CursorMove::DocumentStart:
            target = 0;
            break;
        case CursorMove::DocumentEnd:
            target = text_.size();
            break;
    }
    return target;
}

EditResult Document::move(const CursorMove direction) {
    const std::size_t old = cursor_byte_;
    const std::size_t target = cursorTarget(direction);
    if (old == target) return EditResult::NoChange;
    moveCursorCacheTo(target);
    cursor_byte_ = target;
    if (direction == CursorMove::Left || direction == CursorMove::Right ||
        direction == CursorMove::LineStart || direction == CursorMove::LineEnd ||
        direction == CursorMove::DocumentStart ||
        direction == CursorMove::DocumentEnd) {
        preferred_column_ = cursor_column_;
    }
    ensureCursorVisible();
    ++revision_;
    return EditResult::Changed;
}

EditResult Document::setCursorByte(const std::size_t byte_offset) {
    if (!isUtf8Boundary(text_, byte_offset)) return EditResult::InvalidPosition;
    if (cursor_byte_ == byte_offset) return EditResult::NoChange;
    moveCursorCacheTo(byte_offset);
    cursor_byte_ = byte_offset;
    ++revision_;
    preferred_column_ = cursor_column_;
    ensureCursorVisible();
    return EditResult::Changed;
}

EditResult Document::prepareReplaceRange(
    const std::size_t begin,
    const std::size_t end,
    const std::string_view replacement,
    const std::size_t cursor_after
) {
    if (begin > end || end > text_.size() ||
        !isUtf8Boundary(text_, begin) || !isUtf8Boundary(text_, end)) {
        return EditResult::InvalidPosition;
    }
    if (!isValidUtf8(replacement)) return EditResult::InvalidUtf8;
    const std::size_t removed = end - begin;
    if (replacement.size() > config_.max_bytes - (text_.size() - removed)) {
        return EditResult::CapacityExceeded;
    }
    const std::size_t new_size = text_.size() - removed + replacement.size();
    if (cursor_after > new_size) return EditResult::InvalidPosition;

    bool cursor_boundary = false;
    if (cursor_after <= begin) {
        cursor_boundary = isUtf8Boundary(text_, cursor_after);
    } else if (cursor_after <= begin + replacement.size()) {
        cursor_boundary = isUtf8Boundary(replacement, cursor_after - begin);
    } else {
        const std::size_t old_cursor =
            cursor_after - replacement.size() + removed;
        cursor_boundary = isUtf8Boundary(text_, old_cursor);
    }
    if (!cursor_boundary) return EditResult::InvalidPosition;

    if (new_size > text_.capacity()) {
#if defined(__cpp_exceptions)
        try {
            text_.reserve(new_size);
        } catch (const std::bad_alloc&) {
            return EditResult::ResourceExhausted;
        } catch (...) {
            return EditResult::ResourceExhausted;
        }
#else
        text_.reserve(new_size);
#endif
        if (text_.capacity() < new_size) return EditResult::ResourceExhausted;
    }

    const std::string_view removed_text(text_.data() + begin, removed);
    const bool equal = removed_text.size() == replacement.size() &&
        std::equal(removed_text.begin(), removed_text.end(), replacement.begin());
    return equal ? EditResult::NoChange : EditResult::Changed;
}

EditResult Document::replaceRange(
    const std::size_t begin,
    const std::size_t end,
    const std::string_view replacement,
    const std::size_t cursor_after
) {
    const EditResult prepared =
        prepareReplaceRange(begin, end, replacement, cursor_after);
    if (prepared != EditResult::Changed) {
        if (prepared == EditResult::NoChange) {
            return setCursorByte(cursor_after);
        }
        return prepared;
    }

    const bool line_break_changed =
        text_.find('\n', begin) < end ||
        replacement.find('\n') != std::string_view::npos;
    const char* const replacement_data =
        replacement.empty() ? "" : replacement.data();
    text_.replace(begin, end - begin, replacement_data, replacement.size());
    cursor_byte_ = cursor_after;
    const Position position = positionAt(cursor_byte_);
    cursor_line_ = position.line;
    cursor_column_ = position.column;
    cursor_line_start_ = position.line_start;
    preferred_column_ = cursor_column_;
    dirty_ = true;
    ++revision_;
    advanceTextRevision();
    last_text_edit_ = {text_revision_, begin, end,
                       begin + replacement.size(), true,
                       line_break_changed};
    ensureCursorVisible();
    return EditResult::Changed;
}

bool Document::replaceAll(
    const std::string_view value,
    const std::size_t cursor_byte,
    const bool dirty
) {
    if (value.size() > config_.max_bytes || !isValidUtf8(value) ||
        !isUtf8Boundary(value, cursor_byte)) return false;
    text_.assign(value.data(), value.size());
    finishReplaceAll(cursor_byte, dirty);
    return true;
}

bool Document::replaceAllOwned(
    draftor::memory::String* const owned_text,
    const std::size_t cursor_byte,
    const bool dirty
) noexcept {
    if (!owned_text || owned_text->size() > config_.max_bytes ||
        !isValidUtf8(*owned_text) ||
        !isUtf8Boundary(*owned_text, cursor_byte)) {
        return false;
    }
    text_.swap(*owned_text);
    finishReplaceAll(cursor_byte, dirty);
    return true;
}

void Document::finishReplaceAll(
    const std::size_t cursor_byte,
    const bool dirty
) noexcept {
    cursor_byte_ = cursor_byte;
    const Position position = positionAt(cursor_byte_);
    cursor_line_ = position.line;
    cursor_column_ = position.column;
    cursor_line_start_ = position.line_start;
    dirty_ = dirty;
    preferred_column_ = cursor_column_;
    viewport_top_line_ = 0;
    ++revision_;
    advanceTextRevision();
    last_text_edit_ = {text_revision_, 0, 0, text_.size(), false};
    ensureCursorVisible();
}

void Document::clear() noexcept {
    const bool changed = !text_.empty() || cursor_byte_ != 0;
    text_.clear();
    cursor_byte_ = 0;
    cursor_line_ = 0;
    cursor_column_ = 0;
    cursor_line_start_ = 0;
    viewport_top_line_ = 0;
    preferred_column_ = 0;
    dirty_ = false;
    if (changed) {
        ++revision_;
        advanceTextRevision();
        last_text_edit_ = {text_revision_, 0, 0, 0, false};
    }
}

DocumentView Document::view() const noexcept {
    return {text_, cursor_byte_, cursor_line_, cursor_column_,
            viewport_top_line_, dirty_};
}

void Document::moveCursorCacheTo(const std::size_t byte_offset) noexcept {
    if (byte_offset == cursor_byte_) return;
    if (byte_offset > cursor_byte_) {
        for (std::size_t offset = cursor_byte_; offset < byte_offset;) {
            if (text_[offset] == '\n') {
                ++cursor_line_;
                cursor_column_ = 0;
                cursor_line_start_ = offset + 1U;
                ++offset;
            } else {
                ++cursor_column_;
                offset = nextUtf8Boundary(text_, offset);
            }
        }
        return;
    }

    std::size_t removed_newlines = 0;
    for (std::size_t offset = byte_offset; offset < cursor_byte_;) {
        if (text_[offset] == '\n') {
            ++removed_newlines;
            ++offset;
        } else {
            offset = nextUtf8Boundary(text_, offset);
        }
    }
    cursor_line_ = removed_newlines > cursor_line_
        ? 0 : cursor_line_ - removed_newlines;
    const std::size_t delimiter = byte_offset == 0
        ? draftor::memory::String::npos : text_.rfind('\n', byte_offset - 1U);
    cursor_line_start_ = delimiter == draftor::memory::String::npos ? 0 : delimiter + 1U;
    cursor_column_ = utf8CodePointCount(std::string_view(text_).substr(
        cursor_line_start_, byte_offset - cursor_line_start_));
}

Document::Position Document::positionAt(const std::size_t byte_offset) const noexcept {
    Position result;
    for (std::size_t i = 0; i < byte_offset;) {
        if (text_[i] == '\n') {
            ++result.line;
            result.column = 0;
            result.line_start = i + 1;
            ++i;
        } else {
            ++result.column;
            i = nextUtf8Boundary(text_, i);
        }
    }
    const std::size_t newline = text_.find('\n', byte_offset);
    result.line_end = newline == draftor::memory::String::npos ? text_.size() : newline;
    return result;
}

std::size_t Document::byteAt(std::size_t line, const std::size_t column) const noexcept {
    std::size_t offset = 0;
    while (line > 0 && offset < text_.size()) {
        const std::size_t newline = text_.find('\n', offset);
        if (newline == draftor::memory::String::npos) return text_.size();
        offset = newline + 1;
        --line;
    }
    std::size_t current_column = 0;
    while (offset < text_.size() && text_[offset] != '\n' && current_column < column) {
        offset = nextUtf8Boundary(text_, offset);
        ++current_column;
    }
    return offset;
}

std::size_t Document::lineCount() const noexcept {
    return 1 + static_cast<std::size_t>(std::count(text_.begin(), text_.end(), '\n'));
}

void Document::ensureCursorVisible() noexcept {
    if (cursor_line_ < viewport_top_line_) viewport_top_line_ = cursor_line_;
    if (cursor_line_ >= viewport_top_line_ + config_.viewport_rows) {
        viewport_top_line_ = cursor_line_ - config_.viewport_rows + 1;
    }
}

}  // namespace editor
