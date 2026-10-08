#include "utf8.hpp"

#include <cstdint>

namespace editor {
namespace {

std::size_t sequenceLength(const unsigned char lead) noexcept {
    if (lead <= 0x7FU) return 1;
    if (lead >= 0xC2U && lead <= 0xDFU) return 2;
    if (lead >= 0xE0U && lead <= 0xEFU) return 3;
    if (lead >= 0xF0U && lead <= 0xF4U) return 4;
    return 0;
}

bool continuation(const unsigned char value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

}  // namespace

bool isValidUtf8(const std::string_view text) noexcept {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = sequenceLength(lead);
        if (length == 0 || i + length > text.size()) return false;
        for (std::size_t j = 1; j < length; ++j) {
            if (!continuation(static_cast<unsigned char>(text[i + j]))) return false;
        }
        if (length == 3) {
            const auto second = static_cast<unsigned char>(text[i + 1]);
            if ((lead == 0xE0U && second < 0xA0U) ||
                (lead == 0xEDU && second > 0x9FU)) return false;
        }
        if (length == 4) {
            const auto second = static_cast<unsigned char>(text[i + 1]);
            if ((lead == 0xF0U && second < 0x90U) ||
                (lead == 0xF4U && second > 0x8FU)) return false;
        }
        i += length;
    }
    return true;
}

bool isUtf8Boundary(const std::string_view text, const std::size_t offset) noexcept {
    if (offset > text.size()) return false;
    return offset == 0 || offset == text.size() ||
           !continuation(static_cast<unsigned char>(text[offset]));
}

std::size_t nextUtf8Boundary(const std::string_view text, const std::size_t offset) noexcept {
    if (offset >= text.size()) return text.size();
    std::size_t next = offset + 1;
    while (next < text.size() &&
           continuation(static_cast<unsigned char>(text[next]))) ++next;
    return next;
}

std::size_t previousUtf8Boundary(
    const std::string_view text,
    const std::size_t offset
) noexcept {
    if (offset == 0) return 0;
    std::size_t previous = offset > text.size() ? text.size() : offset;
    --previous;
    while (previous > 0 &&
           continuation(static_cast<unsigned char>(text[previous]))) --previous;
    return previous;
}

std::size_t utf8CodePointCount(const std::string_view text) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); i = nextUtf8Boundary(text, i)) ++count;
    return count;
}

}  // namespace editor
