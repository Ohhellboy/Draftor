#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
#include "display_schedule.hpp"
#include "ime_presentation.hpp"
#include "presentation_frame.hpp"
void pump(Rig& r){unsigned n=0;do{r.step();CHECK(++n<2000000);}while(r.ime.hasWork()||r.app.commandBarrier());}
void hidden(uint8_t key,uint8_t mods,const char* name){
 Rig r;r.begin(false,"ui",false);DisplaySchedule d;d.prepare(32);
 std::array<uint8_t,32> pixels{};unsigned transfers=0;
 auto present=[&](bool ok){
  const auto old=r.ime.window()->onScreenGeneration();
  const bool success=d.present(pixels.data(),pixels.size(),r.now,[&]{
   CHECK(r.ime.window()->onScreenGeneration()==old);++transfers;return ok;
  });
  if(success){publishVisibleCandidate(&r.ime,&r.app);r.app.dirtyDisplay=false;d.completed(r.now);}
  return success;
 };
 CHECK(present(true));const auto screen=r.ime.window()->onScreenGeneration();CHECK(screen);
 // Two distinct events arrive within the same drawing interval.
 r.ime.interrupt(); /* explicit fixture reset; plain Esc remains active CN */r.tap(key,mods);pump(r);
 CHECK(r.ime.chinese()&&!r.ime.active()&&!candidateSurfaceVisible(&r.ime,&r.app));
 CHECK(r.ime.view()->generation==0&&r.ime.window()->onScreenGeneration()==screen);
 CHECK(!candidatePublicationPending(&r.ime,&r.app));
 r.now+=50;CHECK(d.due(r.now,true,false,candidatePublicationPending(&r.ime,&r.app)));
 pixels[0]=1;CHECK(present(true));CHECK(r.ime.window()->onScreenGeneration()==screen);
 unsigned due=0;for(unsigned n=0;n<20;++n){if(d.due(++r.now,false,false,candidatePublicationPending(&r.ime,&r.app)))++due;}
 CHECK(!due);CHECK(publishVisibleCandidate(&r.ime,&r.app)==0&&r.ime.window()->onScreenGeneration()==screen);
 r.tap(0x29);pump(r);CHECK(candidateSurfaceVisible(&r.ime,&r.app));
 // Empty candidates are visible and must not be filtered by count>0.
 CHECK(r.ime.view()->count==0&&candidatePublicationPending(&r.ime,&r.app));
 CHECK(d.due(++r.now,false,true,candidatePublicationPending(&r.ime,&r.app)));
 pixels[0]=2;CHECK(!present(false));CHECK(r.ime.window()->onScreenGeneration()==screen&&candidatePublicationPending(&r.ime,&r.app));
 CHECK(present(true));CHECK(r.ime.window()->onScreenGeneration()==0&&!candidatePublicationPending(&r.ime,&r.app));
 r.type("ui");pump(r);CHECK(candidatePublicationPending(&r.ime,&r.app));
 pixels[0]=3;CHECK(present(true));const auto first=r.ime.window()->onScreenGeneration();
 r.ime.window()->request(0,0,false);pump(r);
 CHECK(r.ime.view()->generation!=first&&candidatePublicationPending(&r.ime,&r.app));
 const auto sent=transfers;CHECK(present(true));CHECK(transfers==sent&&d.reused==1&&!candidatePublicationPending(&r.ime,&r.app));
 std::cout<<"PASS hidden="<<name<<" idle_loops=20 draw_due="<<due<<" visible_clear=1 failed_no_publish=1 exact_pixels_reuse=1 return_to_body=1\n";
}
void findSurface(){
 Rig r;r.begin(false,"ui",false);const auto old=r.frame;r.ime.interrupt();r.tap(9,1);pump(r);
 CHECK(r.app.findOpen()&&candidateSurfaceVisible(&r.ime,&r.app));
 CHECK(prepareImePresentation(&r.ime,r.app));
 const s3app::find_ui::PaintPort port{[](void*,int,int,bool){},nullptr};
 const auto empty=s3app::find_ui::paint(r.app,&r.ime,port);
 CHECK(!empty.candidates&&publishVisibleCandidate(&r.ime,&r.app)==0);
 CHECK(r.ime.window()->onScreenGeneration()==old); // failed/untransferred pixels do not publish
 auto done=publishActivePresentation(r.app,&r.ime,r.app.presentationFrame(),{},empty);
 CHECK(done.accepted&&done.candidates==0&&!candidatePublicationPending(&r.ime,&r.app));
 for(unsigned i=0;i<20;++i)CHECK(!candidatePublicationPending(&r.ime,&r.app));
 r.type("ui");pump(r);CHECK(candidatePublicationPending(&r.ime,&r.app));
 CHECK(prepareImePresentation(&r.ime,r.app));const auto visible=s3app::find_ui::paint(r.app,&r.ime,port);
 CHECK(visible.candidates&&publishVisibleCandidate(&r.ime,&r.app)==0);
 // A stale empty receipt cannot retire the new generation.
 CHECK(s3app::find_ui::publish(r.app,&r.ime,empty)==0&&candidatePublicationPending(&r.ime,&r.app));
 done=publishActivePresentation(r.app,&r.ime,r.app.presentationFrame(),{},visible);r.frame=done.candidates;
 CHECK(r.frame&&!candidatePublicationPending(&r.ime,&r.app));r.tap(44);pump(r);
 CHECK(r.app.document.text().empty()&&!r.app.find.view().query.empty()&&r.app.findOpen());
 std::cout<<"PASS Find owns visible candidates, idle clear acknowledged once, legacy/stale receipts rejected, query-only commit\n";
}
int main(){try{
 CHECK(!candidateSurfaceVisible(nullptr,nullptr));
 hidden(59,0,"files");findSurface();hidden(60,0,"stats");
 return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
