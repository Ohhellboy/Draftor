#pragma once
#include "product_config.hpp"
#include "body_key_repeat.hpp"
#include "hanime_document_editor_port.hpp"
#include "horizontal_viewport_adapter.hpp"
#include "src/hanime_document/entropy_id_source.hpp"
#include "src/hanime_document/document_session.hpp"
#if DRAFTOR_ENABLE_FIND
#include "find_controller.hpp"
#include "hanime_document_adapter.hpp"
#endif
#if DRAFTOR_ENABLE_STATS
#include "document_stats_adapter.hpp"
#endif

namespace draftor {
// Core composition for host/Pico validation, not a Lite product shell.
// Drivers, menus, IME, USB, fonts and product deletion flow are external.
class CoreRuntime {
 public:
  explicit CoreRuntime(hanime::documents::StoragePort& storage);
  void serviceReaders();
  std::uint64_t focusEpoch() const noexcept { return focus_; }
  editor::Document document;
  editor::body_edit::BodyEditor editor;
  hanime::renderer::LayoutEngine layout;
  editor::visual_nav::VisualCursorNavigator navigator;
  editor::presentation::HorizontalViewportAdapter horizontalPosition;
  hanime::body_extension::BodyKeyRepeater repeat;
  hanime::documents::EntropyIdSource ids;
  hanime::documents::DocumentCatalog catalog;
  hanime::documents::PersistenceService persistence;
  hanime::documents::integration::HanimeDocumentEditorPort port;
  hanime::documents::DocumentSession session;
#if DRAFTOR_ENABLE_FIND
  hanime::find::integration::HanimeDocumentAdapter findDocument;
  hanime::find::FindSession find;
  hanime::find::input::FindController findController;
#endif
#if DRAFTOR_ENABLE_STATS
  hanime::text_stats::Counter stats;
#endif
 private:
  static void installed(void*) noexcept;
  static void activated(void*) noexcept;
  std::uint64_t focus_ = 1;
};
} // namespace draftor
