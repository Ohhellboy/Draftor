#pragma once

#include "document.hpp"
#include "hanime/text_stats.hpp"

namespace hanime { namespace text_stats {

// Compiled against the actual Integration v0.10 editor::Document in host tests.
// For a later editor retaining these getters, this adapter needs no modification.
// context_id is a host-owned incarnation counter changed on document installation.
inline Source document_source(const editor::Document& document,
                              std::uint64_t context_id) noexcept {
    return {document.text(), context_id, document.textRevision()};
}

inline State observe_document(Counter& counter, const editor::Document& document,
                              std::uint64_t context_id) noexcept {
    const auto edit = document.lastTextEdit();
    const Change change{edit.begin, edit.old_end, edit.new_end,
                        edit.incremental && edit.text_revision == document.textRevision()};
    return counter.observe(document_source(document, context_id), &change);
}

inline std::size_t service_document(Counter& counter, const editor::Document& document,
                                    std::uint64_t context_id,
                                    std::size_t budget = kDefaultByteBudget) noexcept {
    observe_document(counter, document, context_id);
    return counter.service(document_source(document, context_id), budget);
}

}} // namespace hanime::text_stats
