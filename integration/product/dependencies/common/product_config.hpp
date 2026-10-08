#pragma once
#include "body_editor.hpp"
#include "src/hanime_document/persistence_service.hpp"

#ifndef DRAFTOR_MINIMAL
#define DRAFTOR_MINIMAL 0
#endif
#ifndef DRAFTOR_ENABLE_FIND
#define DRAFTOR_ENABLE_FIND (!DRAFTOR_MINIMAL)
#endif
#ifndef DRAFTOR_ENABLE_STATS
#define DRAFTOR_ENABLE_STATS (!DRAFTOR_MINIMAL)
#endif

namespace draftor::product {
inline constexpr bool minimal = DRAFTOR_MINIMAL;
inline constexpr std::size_t body_bytes = minimal ? 8192U : 65536U;
inline constexpr std::size_t reader_work = minimal ? 64U : 256U;
constexpr editor::DocumentConfig document() { return {body_bytes, 9, 40}; }
constexpr editor::body_edit::BodyEditorConfig editor() {
  editor::body_edit::BodyEditorConfig c;
  c.clipboard_bytes = minimal ? 512U : 4096U;
  c.history_bytes = minimal ? 0U : 8192U;
  c.history_records = minimal ? 0U : 32U;
  c.transaction_bytes = minimal ? 512U : 8192U;
  c.record_history = !minimal;
  c.cross_document_clipboard = !minimal;
  return c;
}
constexpr hanime::documents::SaveBufferConfig save() {
  return {body_bytes, body_bytes, nullptr, !minimal};
}
} // namespace draftor::product
