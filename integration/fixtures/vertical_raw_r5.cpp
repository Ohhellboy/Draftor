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
void entry(R5Rig& r,bool vertical){
 r.setup();if(vertical)r.key(61);r.letters("aaar");r.settle();
 CHECK(!r.ime.rawEditing()&&r.ime.rawCursor()==4);r.key(vertical?82:80);r.settle();
 CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3&&r.ime.raw()=="aaar");
 CHECK(r.app.document.text().empty());r.trace("entry");
}
void eraseRestore(R5Rig& r,bool restore){
 const auto rev=r.ime.composition()->snapshot().revision;
 const auto query=r.ime.window()->querySerial();
 auto* composition=r.ime.composition();auto* owner=r.ime.window();
 r.key(42);r.settle();CHECK(r.ime.raw()=="aar"&&r.ime.rawCursor()==2&&r.ime.rawEditing());
 CHECK(composition->snapshot().revision==rev+1&&owner->querySerial()==query+1);
 CHECK(r.ime.candidateAvailable()&&r.app.document.text().empty());r.trace("backspace");
 if(restore){r.letters("a");r.settle();CHECK(r.ime.raw()=="aaar"&&r.ime.rawEditing()&&r.ime.rawCursor()==3);
  CHECK(composition->snapshot().revision==rev+2&&owner->querySerial()==query+2);
  CHECK(r.ime.candidateAvailable()&&r.ime.composition()==composition&&r.ime.window()==owner);
  CHECK(r.app.document.text().empty());r.trace("restore");}
}
void reject(R5Rig& r,char c){
 const auto stamp=r.ime.composition()->raw_input().stamp;const auto query=r.ime.window()->querySerial();
 const auto frame=r.frame;const auto cursor=r.ime.rawCursor();
 const auto text=r.ime.view()->rows[0].items[0].text;const auto rev=r.app.document.textRevision();
 r.letters(std::string(1,c));r.settle();
 CHECK(r.ime.raw()=="aaar"&&r.ime.rawEditing()&&r.ime.rawCursor()==cursor);
 CHECK(r.ime.composition()->input_is_current(stamp)&&r.ime.window()->querySerial()==query);
 CHECK(r.frame==frame&&r.ime.view()->rows[0].items[0].text==text);
 CHECK(r.app.document.text().empty()&&r.app.document.textRevision()==rev);r.trace("rejected");
}
void selection(R5Rig& r,uint8_t key){
 const auto item=r.ime.view()->rows[0].items[key==44?0:1];
 const std::string expected(item.text.data(),item.bytes);
 const auto rev=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
 auto e=r.freshEvent(key,true);r.deliver(e);r.settle();e.ms=++r.now;r.deliver(e);
 r.deliver(r.freshEvent(key,false));r.settle();
 CHECK(std::string_view(r.app.document.text())==expected);
 CHECK(r.app.document.textRevision()==rev+1&&r.app.editor.undoCount()==undo+1&&r.ime.raw().empty());
 r.trace("selection_once");
}
void equivalent(){
 for(unsigned scheme=0;scheme<3;++scheme){
  size_t cursor[2]{};std::string erased[2];
  for(unsigned vertical=0;vertical<2;++vertical){R5Rig r(630+scheme*2+vertical);
   r.key(57);r.settle();for(unsigned n=0;n<scheme;++n){r.key(44,1);r.settle();}
   if(vertical)r.key(61);r.letters(scheme==2?"aaar":"ni");r.settle();r.key(vertical?82:80);r.settle();
   CHECK(r.ime.rawEditing());cursor[vertical]=r.ime.rawCursor();
   r.key(vertical?81:79);r.key(vertical?82:80);r.settle();CHECK(r.ime.rawCursor()==cursor[vertical]);
   r.key(42);r.settle();erased[vertical]=std::string(r.ime.raw());CHECK(r.app.document.text().empty());
   r.trace("three_scheme_equivalent");
  }
  CHECK(cursor[0]==cursor[1]&&erased[0]==erased[1]);
 }
}
void scenario(unsigned id){
 if(id==6){equivalent();return;}
 R5Rig r(610+id);entry(r,id!=5);
 if(id==1)eraseRestore(r,false);
 else if(id==2)eraseRestore(r,true);
 else if(id==3)reject(r,'x');
 else if(id==4)reject(r,'z');
 else if(id==5){eraseRestore(r,true);reject(r,'x');reject(r,'z');r.key(80);r.key(79);CHECK(r.ime.rawCursor()==3);
  r.key(41);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw().empty());r.letters("aaar");r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw()=="aaar");selection(r,44);}
 else if(id==7||id==8)selection(r,id==7?44:31);
 else if(id==9){auto stale=r.freshEvent(31,true);auto staleEsc=r.freshEvent(41,true);
  eraseRestore(r,true);const auto stamp=r.ime.composition()->raw_input().stamp;
  r.deliver(stale);r.deliver(r.freshEvent(31,false));r.deliver(staleEsc);r.deliver(r.freshEvent(41,false));r.settle();
  CHECK(r.ime.rawEditing()&&r.ime.raw()=="aaar"&&r.ime.composition()->input_is_current(stamp)&&r.app.document.text().empty());
  r.key(41);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw().empty());r.letters("aaar");r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw()=="aaar");r.key(82);r.settle();CHECK(r.ime.rawEditing()&&r.ime.rawCursor()==3);}
 else if(id==10){const auto owner=r.ime.resourceOwner();auto stale=r.freshEvent(44,true);auto oldLetter=r.freshEvent(4,true);
  r.key(44,1);r.settle();CHECK(r.resources.rules()==R::R1Xiaohe&&r.ime.resourceOwner()!=owner);
  CHECK(r.ime.raw().empty()&&!r.ime.rawEditing());r.deliver(stale);r.deliver(r.freshEvent(44,false));
  r.deliver(oldLetter);r.deliver(r.freshEvent(4,false));r.settle();CHECK(r.ime.raw().empty()&&r.app.document.text().empty());}
 else CHECK(false);
}
int main(int argc,char** argv){try{
 CHECK(argc==2);const std::string mode=argv[1];
 if(mode.rfind("R5T",0)==0){scenario(unsigned(std::stoul(mode.substr(3))));CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS "<<mode<<std::endl;return 0;}
 R5Rig r(601);r.setup();
 if(mode!="H-left"&&mode!="bridge")r.key(61);
 r.letters("aaar");r.settle();r.trace("before_navigation");
 if(mode=="H-left")r.key(80);
 else if(mode=="V-up")r.key(82);
 else if(mode=="V-left")r.key(80);
 else if(mode=="bridge"){r.key(80);r.settle();r.trace("H_raw_enter");r.key(61);r.settle();r.trace("V_raw_before_up");r.key(82);r.settle();r.trace("V_raw_after_up");r.key(81);}
 else CHECK(false);
 r.settle();r.trace("after_navigation");r.key(42);r.settle();r.trace("after_backspace");
 CHECK(r.ime.raw()=="aar");CHECK(r.ime.rawEditing());CHECK(r.app.document.text().empty());return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
