// Explicit build verification only. No startup call, command, mode, or toggle.
// --undefined retains this probe and its real consumers despite section GC.
#if defined(PHASE2B_S3_LINK_COVERAGE)
#include "vertical_view.hpp"
extern "C" __attribute__((noinline,used)) void phase2b_vertical_link_probe(
    s3app::App& app,ime_device::DeviceIme& ime,const s3host::FontPort& font,
    const s3app::VerticalPaintPort& port) {
  s3app::VerticalPresentation vertical(app.sharedState(),font);
  vertical.followCaret(app.editor.caretAffinity());
  editor::presentation::ByteTarget target;
  vertical.spatialTarget(editor::presentation::SpatialDirection::Left,false,target);
  editor::presentation::ViewportState anchor;
  if(vertical.captureViewport(anchor))vertical.restoreViewport(anchor);
  const auto painted=s3app::paintVerticalView(app,vertical,&ime,font,0,port);
  if(painted.status==draftor::vertical::Status::Ready)
    (void)ime_device::publishVerticalFrame(&ime,app,painted.ime);
  if(ime.window())(void)ime_device::chooseVertical(*ime.window(),
      {draftor::vertical::AbRole::Preview,0,painted.ime});
  if(ime.window())(void)ime_device::verticalCandidateKey(*ime.window(),'=');
  (void)ime_device::dispatchVerticalNumber(ime,app,{},{});
}
#endif
