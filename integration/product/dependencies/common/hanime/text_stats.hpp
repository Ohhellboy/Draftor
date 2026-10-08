#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hanime { namespace text_stats {

// Product counting rules, independent of firmware Integration version.
inline constexpr unsigned kRulesVersion = 1;
inline constexpr std::size_t kDefaultByteBudget = 256;

struct Counts {
    std::uint64_t han_characters = 0;
    std::uint64_t latin_words = 0;     // Alphanumeric tokens containing a Latin letter.
    std::uint64_t number_tokens = 0;   // Numeric tokens with no Latin letter.
    std::uint64_t characters = 0;      // Unicode scalars, including whitespace.
    std::uint64_t characters_no_space = 0;

    std::uint64_t mixed_words() const noexcept {
        return han_characters + latin_words + number_tokens;
    }
};

enum class State : std::uint8_t {
    Unbound,
    Counting,
    Ready,
    InvalidUtf8,
    InvalidSource,  // Length changed without a context/revision change.
};

struct Source {
    std::string_view text;
    // Host-owned document incarnation token, NOT a filename or cursor revision.
    // Change it on successful document installation/switch, including remounts.
    std::uint64_t context_id = 0;
    std::uint32_t text_revision = 0;
};

// Coordinates are UTF-8 bytes: old [begin, old_end) -> new [begin, new_end).
// Optional optimization hint; it must describe the one edit since last observe.
struct Change {
    std::size_t begin = 0;
    std::size_t old_end = 0;
    std::size_t new_end = 0;
    bool incremental = false;
};

struct Report {
    State state = State::Unbound;
    Counts counts{};
    std::uint64_t context_id = 0;
    std::uint32_t text_revision = 0;
    std::size_t scanned_bytes = 0;
    std::size_t total_bytes = 0;
    std::size_t error_byte = std::string_view::npos;

    bool complete() const noexcept { return state == State::Ready; }
};

// No allocation, text copy, file access, clock, threading, UI or hardware calls.
// The host serializes document mutation and these calls on one event loop.
// Source views are used only within a call; no text pointer is retained.
class Counter {
public:
    void reset() noexcept;
    State observe(Source source, const Change* change = nullptr) noexcept;
    // Consumes AT MOST byte_budget bytes; even budget=1 progresses through UTF-8.
    // Automatically observes an unexpected revision using safe full invalidation.
    std::size_t service(Source source,
                        std::size_t byte_budget = kDefaultByteBudget) noexcept;
    Report report() const noexcept { return report_; }

private:
    enum class Token : std::uint8_t { None, Number, Latin };
    enum class Base : std::uint8_t { None, Digit, Letter };
    enum class Pending : std::uint8_t { None, WordJoin, Decimal };

    void begin(Source source) noexcept;
    void accept(std::uint32_t scalar) noexcept;
    void separator() noexcept;
    bool feed(unsigned char byte, std::size_t offset) noexcept;

    Report report_{};
    Token token_ = Token::None;
    Base base_ = Base::None;
    Pending pending_ = Pending::None;
    std::uint32_t scalar_ = 0;
    std::uint32_t minimum_scalar_ = 0;
    std::size_t sequence_start_ = 0;
    std::uint8_t remaining_ = 0;
};

// Synchronous helper for tests, PC tools or small selections. Do not call this
// on the whole document for every input event; use Counter::service instead.
Report count_utf8(std::string_view text) noexcept;

}} // namespace hanime::text_stats
