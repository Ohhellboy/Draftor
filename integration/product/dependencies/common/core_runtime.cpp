#include "core_runtime.hpp"

namespace draftor {
CoreRuntime::CoreRuntime(hanime::documents::StoragePort& storage)
 : document(product::document()), editor(document, product::editor()),
   layout({284,306,28,14,14,0,nullptr,nullptr}), navigator(document,layout),
   horizontalPosition(document,navigator),
   ids({},0x456469746f72436fULL), catalog(storage,ids),
   persistence(storage,catalog,{},product::save()),
   port(document,editor,horizontalPosition,{nullptr,nullptr,&installed,this,&activated}),
   session(catalog,persistence,port)
#if DRAFTOR_ENABLE_FIND
   , findDocument(document,editor,horizontalPosition,{},0,0), findController(find,findDocument)
#endif
{}
void CoreRuntime::installed(void* context) noexcept {
  auto& self=*static_cast<CoreRuntime*>(context);
  ++self.focus_;
  self.editor.resetForDocument();
  self.repeat.cancel(hanime::body_extension::RepeatCancelReason::DocumentChanged);
  self.navigator.invalidatePreferredX();
#if DRAFTOR_ENABLE_FIND
  self.find=hanime::find::FindSession{};
  self.findController.invalidateQueuedInput();
  self.findDocument.clearMatchHighlight();
#endif
#if DRAFTOR_ENABLE_STATS
  self.stats.reset();
#endif
}
void CoreRuntime::activated(void* context) noexcept {
  static_cast<CoreRuntime*>(context)->editor.resetForDocument(true);
}
void CoreRuntime::serviceReaders() {
#if DRAFTOR_ENABLE_FIND
  if(!findDocument.bindingMatches(session.currentId(),session.sessionSerial(),catalog.boundSession().mount_epoch)) {
    find=hanime::find::FindSession{};
    findDocument.rebind(session.currentId(),session.sessionSerial(),catalog.boundSession().mount_epoch);
  }
  if(find.view().status==hanime::find::Status::Searching) {
    (void)find.step(findDocument,product::reader_work);
  }
#endif
#if DRAFTOR_ENABLE_STATS
  if(session.hasCurrent()) {
    hanime::text_stats::observe_document(stats,document,session.sessionSerial());
    hanime::text_stats::service_document(stats,document,session.sessionSerial(),product::reader_work);
  }
#endif
}
} // namespace draftor
