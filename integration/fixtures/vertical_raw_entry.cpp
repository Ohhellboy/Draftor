#define main inherited_wubi_main
#include "wubi_product.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_ime.hpp"
struct R5Rig:Rig {
 s3app::ProductInput product;
 explicit R5Rig(unsigned id):Rig(REAL_RESOURCES,id,productionPins()){}
 void deliver(const s3hid::Event& e){if(!product.layout(app,e)&&!product.ime(app,&ime,e))app.key(e);}
 void key(uint8_t u,uint8_t mods=0){for(bool down:{true,false})deliver(freshEvent(u,down,mods));}
 void letters(std::string_view s){for(char c:s)key(uint8_t(c-'a'+4));}
 // Publish a real current Vertical presentation before the physical report.
 // Rig::settle alone exercises the generic Host frame completion helper.
 void settle(){
  Rig::settle();
  if(ime.raw().empty())return;
  for(unsigned n=0;n<4&&!prepareImePresentation(&ime,app);++n)Rig::settle();
  CHECK(prepareImePresentation(&ime,app));
  if(app.horizontalActive())return;
  struct Paint{unsigned raw=0,candidates=0;} painted;
  draftor::vertical::AbPort draw;draw.context=&painted;
  draw.line=[](void*,draftor::vertical::Rect,bool){};
  draw.run=[](void* p,const draftor::vertical::AbRun& run){
   auto& out=*static_cast<Paint*>(p);
   if(run.role==draftor::vertical::AbRole::Composition)++out.raw;
   if(run.role==draftor::vertical::AbRole::CurrentCandidate){CHECK(run.identity);++out.candidates;}
  };
  const auto receipt=verticalIme(&ime,app,now,0,draw);
  CHECK(receipt.layout.status==draftor::vertical::AbStatus::Ready);
  CHECK(receipt.candidate_frame&&painted.raw&&painted.candidates);
  frame=publishVerticalFrame(&ime,app,receipt);CHECK(frame);
  std::cout<<"vertical_published frame="<<frame<<" generation="<<receipt.generation<<" revision="<<receipt.revision<<std::endl;
 }
 void setup(){key(57);settle();key(44,1);settle();key(44,1);settle();CHECK(resources.rules()==R::DirectExact);}
 void trace(const char* stage){const auto s=ime.composition()->snapshot();std::cout<<stage<<" layout="<<(app.horizontalActive()?"H":"V")<<" raw="<<ime.raw()<<" rawEditing="<<ime.rawEditing()<<" cursor="<<ime.rawCursor()<<" revision="<<s.revision<<" query="<<ime.window()->querySerial()<<" body_bytes="<<app.document.text().size()<<std::endl;}
};

// Physical-key contract tests. No writes to internal IME/presentation state.
void initial(R5Rig& r,unsigned scheme,bool vertical=true){
 r.key(57);r.settle();for(unsigned n=0;n<scheme;++n){r.key(44,1);r.settle();}
 CHECK(r.resources.rules()==(scheme==0?R::R1Xiaohe:scheme==1?R::FullPinyin:R::DirectExact));
 if(vertical)r.key(61);r.letters(scheme==2?"aaar":"ni");r.settle();
 CHECK(r.ime.candidateAvailable()&&!r.ime.rawEditing());r.trace("initial");
}
void upEntry(unsigned scheme){
 R5Rig r(740+scheme);initial(r,scheme);const auto owner=r.ime.resourceOwner();
 const auto stamp=r.ime.composition()->raw_input().stamp;const auto query=r.ime.window()->querySerial();
 const auto frame=r.ime.window()->onScreenGeneration();const auto body=r.app.document.textRevision();
 auto* composition=r.ime.composition();auto* window=r.ime.window();
 r.key(82);r.settle();r.trace("physical_up");
 CHECK(r.ime.rawEditing());CHECK(r.app.document.text().empty()&&r.app.document.textRevision()==body);
 CHECK(r.ime.resourceOwner()==owner&&r.ime.composition()==composition&&r.ime.window()==window);
 CHECK(composition->input_is_current(stamp)&&window->querySerial()==query&&window->onScreenGeneration()==frame);
 if(scheme==2)CHECK(r.ime.rawCursor()==3);
}
void leftCandidate(unsigned scheme){
 R5Rig r(750+scheme);initial(r,scheme);const auto row=r.ime.view()->row,col=r.ime.view()->col;
 const auto query=r.ime.window()->querySerial();r.key(80);r.settle();r.trace("physical_left");
 CHECK(!r.ime.rawEditing()&&r.ime.window()->browsingExpanded());
 CHECK(r.ime.view()->row==row&&r.ime.view()->col==col&&r.ime.window()->querySerial()==query);
 CHECK(r.app.document.text().empty());
 r.key(80);r.settle();CHECK(!r.ime.rawEditing());
 CHECK(r.ime.view()->row==row+1||r.ime.boundaryVisible(r.now));
}
void test(unsigned id){
 if(id==1||id==6||id==7){upEntry(id==1?2:id==6?1:0);return;}
 if(id==3){leftCandidate(2);return;}
 if(id==8){leftCandidate(0);leftCandidate(1);return;}
 if(id==12){upEntry(0);upEntry(1);upEntry(2);return;}
 if(id==9){for(unsigned scheme=0;scheme<3;++scheme){R5Rig r(760+scheme);initial(r,scheme,false);
  r.key(80);r.settle();CHECK(r.ime.rawEditing());const auto cursor=r.ime.rawCursor();
  r.key(79);r.key(80);r.settle();CHECK(r.ime.rawCursor()==cursor&&r.app.document.text().empty());}return;}
 R5Rig r(770+id);initial(r,2);
 if(id==5||id==13){
  r.key(id==5?81:80);if(id==5)r.settle();r.key(82);r.settle();
  CHECK(!r.ime.rawEditing()&&r.app.document.text().empty());
  if(id==5)CHECK(r.ime.view()->col==0);return;
 }
 if(id==14){r.key(81);r.settle();r.key(42);r.settle();CHECK(!r.ime.rawEditing());
  r.key(82);r.settle();CHECK(r.ime.rawEditing()&&r.ime.raw()=="aaa");return;}
 if(id==15){auto old=r.freshEvent(82,true);r.key(42);r.settle();
  r.deliver(old);r.deliver(r.freshEvent(82,false));r.settle();CHECK(!r.ime.rawEditing());
  r.key(82);r.settle();CHECK(r.ime.rawEditing());return;}
 if(id==16){auto e=r.freshEvent(82,true);r.deliver(e);CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3);
  r.key(61);e.ms=++r.now;r.deliver(e);r.deliver(r.freshEvent(82,false));r.settle();
  CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3&&r.app.document.text().empty());return;}
 r.key(82);r.settle();CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3);
 if(id==2){const auto revision=r.ime.composition()->snapshot().revision;const auto query=r.ime.window()->querySerial();
  r.key(42);r.settle();r.trace("backspace");CHECK(r.ime.raw()=="aar"&&r.ime.rawCursor()==2);
  CHECK(r.ime.composition()->snapshot().revision==revision+1&&r.ime.window()->querySerial()==query+1);
 }else if(id==4){r.key(82);r.settle();CHECK(r.ime.rawCursor()==2);r.key(81);r.settle();CHECK(r.ime.rawCursor()==3);
 }else if(id==10){const auto stamp=r.ime.composition()->raw_input().stamp;const auto query=r.ime.window()->querySerial();
  for(char c:{'x','z'}){r.letters(std::string(1,c));r.settle();CHECK(r.ime.raw()=="aaar"&&r.ime.rawCursor()==3);
   CHECK(r.ime.composition()->input_is_current(stamp)&&r.ime.window()->querySerial()==query);}
 }else if(id==11){
  const auto item=r.ime.view()->rows[0].items[0];const std::string expected(item.text.data(),item.bytes);
  const auto revision=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
  auto e=r.freshEvent(44,true);r.deliver(e);r.settle();e.ms=++r.now;r.deliver(e);r.deliver(r.freshEvent(44,false));r.settle();
  CHECK(std::string_view(r.app.document.text())==expected&&r.app.document.textRevision()==revision+1&&r.app.editor.undoCount()==undo+1);
  CHECK(r.ime.raw().empty());return;
 }else CHECK(false);
 CHECK(r.app.document.text().empty());
}
int main(int argc,char** argv){try{CHECK(argc==2);test(unsigned(std::stoul(argv[1])));
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS T"<<argv[1]<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
