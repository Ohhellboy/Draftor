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
  if(ime.raw().empty()||!ime.candidateAvailable())return;
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


#include "esc_cases.inc"
