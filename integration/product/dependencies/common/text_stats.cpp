#include "hanime/text_stats.hpp"

namespace hanime { namespace text_stats {
namespace {

struct Range { std::uint32_t first, last; };
#include "unicode_tables.h"

template<std::size_t N>
bool contains(const Range (&ranges)[N], std::uint32_t cp) noexcept {
    std::size_t lo = 0, hi = N;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (cp < ranges[mid].first) hi = mid;
        else if (cp > ranges[mid].last) lo = mid + 1;
        else return true;
    }
    return false;
}

bool whitespace(std::uint32_t cp) noexcept {
    // Explicit rule table: Unicode White_Space characters, not C locale isspace.
    return (cp >= 0x09 && cp <= 0x0d) || cp == 0x20 || cp == 0x85 ||
           cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f ||
           cp == 0x3000;
}

bool boundary(std::string_view text, std::size_t offset) noexcept {
    return offset <= text.size() &&
           (offset == text.size() ||
            (static_cast<unsigned char>(text[offset]) & 0xc0U) != 0x80U);
}

bool valid_change(const Change& c, std::size_t old_size,
                  std::string_view new_text) noexcept {
    return c.incremental && c.begin <= c.old_end && c.begin <= c.new_end &&
           c.old_end <= old_size && c.new_end <= new_text.size() &&
           old_size - c.old_end == new_text.size() - c.new_end &&
           boundary(new_text, c.begin) && boundary(new_text, c.new_end);
}

} // namespace

void Counter::reset() noexcept { *this = Counter{}; }

void Counter::begin(Source source) noexcept {
    reset();
    report_.context_id = source.context_id;
    report_.text_revision = source.text_revision;
    report_.total_bytes = source.text.size();
    report_.state = source.text.empty() ? State::Ready : State::Counting;
}

State Counter::observe(Source source, const Change* change) noexcept {
    if (report_.state == State::Unbound || report_.context_id != source.context_id) {
        begin(source);
        return report_.state;
    }
    if (report_.text_revision == source.text_revision) {
        if (report_.total_bytes != source.text.size()) {
            report_.state = State::InvalidSource;
        }
        return report_.state;
    }
    const bool next = source.text_revision ==
        static_cast<std::uint32_t>(report_.text_revision + std::uint32_t{1});
    const bool usable = report_.state == State::Counting || report_.state == State::Ready;
    // A processed prefix is reusable only when it is provably unchanged. This
    // also preserves the open token/UTF-8 decoder during continuous appends.
    if (next && usable && change &&
        valid_change(*change, report_.total_bytes, source.text) &&
        change->begin >= report_.scanned_bytes) {
        report_.text_revision = source.text_revision;
        report_.total_bytes = source.text.size();
        report_.state = report_.scanned_bytes == report_.total_bytes
            ? State::Ready : State::Counting;
        if (report_.state == State::Ready && remaining_ != 0) {
            report_.state = State::InvalidUtf8;
            report_.error_byte = sequence_start_;
        }
    } else {
        begin(source);
    }
    return report_.state;
}

void Counter::separator() noexcept {
    token_ = Token::None;
    base_ = Base::None;
    pending_ = Pending::None;
}

void Counter::accept(std::uint32_t cp) noexcept {
    ++report_.counts.characters;
    if (!whitespace(cp)) ++report_.counts.characters_no_space;

    if (cp >= 0xff01 && cp <= 0xff5e) cp -= 0xfee0; // Full-width ASCII, counting only.
    if (cp >= 0x3000 && contains(kHan, cp)) {
        ++report_.counts.han_characters;
        separator();
        return;
    }
    const bool letter = (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
                        (cp >= 0x80 && contains(kLatin, cp));
    const bool digit = cp >= '0' && cp <= '9';
    if (letter || digit) {
        const bool join = token_ != Token::None &&
            (pending_ == Pending::None ||
             (pending_ == Pending::WordJoin && base_ == Base::Letter && letter) ||
             (pending_ == Pending::Decimal && base_ == Base::Digit && digit));
        if (!join) {
            token_ = letter ? Token::Latin : Token::Number;
            if (letter) ++report_.counts.latin_words;
            else ++report_.counts.number_tokens;
        } else if (letter && token_ == Token::Number) {
            --report_.counts.number_tokens;
            ++report_.counts.latin_words;
            token_ = Token::Latin;
        }
        base_ = letter ? Base::Letter : Base::Digit;
        pending_ = Pending::None;
        return;
    }
    // Marks neither start a word nor break its current base/connector state.
    if (cp >= 0x300 && contains(kMarks, cp)) return;
    if (token_ != Token::None && pending_ == Pending::None) {
        if (base_ == Base::Letter &&
            (cp == '\'' || cp == 0x2019 || cp == '-' || cp == 0x2010 || cp == 0x2011)) {
            pending_ = Pending::WordJoin;
            return;
        }
        if (base_ == Base::Digit && cp == '.') {
            pending_ = Pending::Decimal;
            return;
        }
    }
    separator();
}

bool Counter::feed(unsigned char byte, std::size_t offset) noexcept {
    if (remaining_ == 0) {
        sequence_start_ = offset;
        if (byte <= 0x7f) { accept(byte); return true; }
        if (byte >= 0xc2 && byte <= 0xdf) {
            scalar_ = byte & 0x1f; minimum_scalar_ = 0x80; remaining_ = 1;
        } else if (byte >= 0xe0 && byte <= 0xef) {
            scalar_ = byte & 0x0f; minimum_scalar_ = 0x800; remaining_ = 2;
        } else if (byte >= 0xf0 && byte <= 0xf4) {
            scalar_ = byte & 0x07; minimum_scalar_ = 0x10000; remaining_ = 3;
        } else return false;
        return true;
    }
    if ((byte & 0xc0U) != 0x80U) return false;
    scalar_ = (scalar_ << 6U) | (byte & 0x3fU);
    --remaining_;
    if (remaining_ == 0) {
        if (scalar_ < minimum_scalar_ || scalar_ > 0x10ffff ||
            (scalar_ >= 0xd800 && scalar_ <= 0xdfff)) return false;
        accept(scalar_);
    }
    return true;
}

std::size_t Counter::service(Source source, std::size_t byte_budget) noexcept {
    observe(source);
    if (report_.state != State::Counting || byte_budget == 0) return 0;
    std::size_t consumed = 0;
    while (report_.scanned_bytes < source.text.size() && consumed < byte_budget) {
        const auto at = report_.scanned_bytes++;
        ++consumed;
        if (!feed(static_cast<unsigned char>(source.text[at]), at)) {
            report_.state = State::InvalidUtf8;
            report_.error_byte = sequence_start_;
            return consumed;
        }
    }
    if (report_.scanned_bytes == source.text.size()) {
        if (remaining_ != 0) {
            report_.state = State::InvalidUtf8;
            report_.error_byte = sequence_start_;
        } else {
            report_.state = State::Ready;
        }
    }
    return consumed;
}

Report count_utf8(std::string_view text) noexcept {
    Counter counter;
    counter.service({text, 0, 0}, text.size());
    return counter.report();
}

}} // namespace hanime::text_stats
