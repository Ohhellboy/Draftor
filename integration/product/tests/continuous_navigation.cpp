#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
void ordered(bool py){
 Rig serial,burst;serial.begin(py,py?"yi":"ui",false);burst.begin(py,py?"yi":"ui",false);
 for(uint8_t key:{0x51,0x51,0x51,0x52,0x4f,0x50,0x52}){serial.tap(key);serial.settle();burst.tap(key);}
 burst.settle();CHECK(digest(*burst.ime.view())==digest(*serial.ime.view()));CHECK(!burst.ime.queuedArrows());
 const auto row=burst.ime.view()->row;
 s3hid::Event held{1,++burst.now,0x51,0,true,0,burst.frame,0};held.ime_context=burst.ime.ingressContext();held.resource_owner=burst.ime.resourceOwner();CHECK(burst.ime.event(held));
 for(unsigned i=0;i<10;++i){held.ms=++burst.now;CHECK(burst.ime.event(held));}
 held.down=false;held.ms=++burst.now;burst.ime.event(held);burst.settle();CHECK(burst.ime.view()->row==row+1);
 std::cout<<"PASS ordered initial entry/burst/reversal/columns py="<<py<<" held duplicates stay one action\n";
}
void selectionAndCancel(){
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();
 const auto& v=*r.ime.view();const auto item=v.rows[v.row-v.rows[0].index].items[0];
 r.tap(0x51);r.tap(0x51);r.tap(0x51);CHECK(r.ime.queuedArrows());
 r.tap(30);r.settle();CHECK(r.app.document.text()==std::string_view(item.text.data(),item.bytes));CHECK(!r.ime.queuedArrows());
 r.type("ui");r.settle();r.tap(0x51);r.tap(0x51);r.tap(0x51);r.tap(0x2a);r.settle();
 CHECK(r.ime.raw()=="u"&&!r.ime.queuedArrows());
 r.tap(0x29);r.type("ui");r.settle();r.tap(0x51);r.tap(0x51);r.tap(0x2c,1);r.settle();
 CHECK(r.ime.raw().empty()&&!r.ime.queuedArrows()&&r.resources.rules()==hanime_extension::SchemeRules::FullPinyin);
 std::cout<<"PASS queued navigation does not retag old-frame numeric selection; edit/cancel/switch discard intent\n";
}
void bounded(){
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();
 for(unsigned n=0;n<40;++n)r.tap(0x51);
 CHECK(r.ime.queuedArrows()==32&&r.ime.arrowOverflow()==7); // first arrow is in flight
 r.tap(0x29);CHECK(!r.ime.queuedArrows());r.type("ui");r.settle();CHECK(r.ime.view()->row==0);
 const auto frame=r.frame;r.tap(0x51);r.tap(0x51);r.resources.mount()->epoch++;
 r.ime.service();CHECK(!r.ime.queuedArrows()&&!r.ime.chinese());(void)frame;
 std::cout<<"PASS bounded overflow explicit, canceled/new input and mount invalidation isolated\n";
}
int main(){try{
 Rig r;r.begin(false,"ui",false);r.tap(0x51);r.settle();
 CHECK(r.ime.view()->row==1);
 // These are three distinct presses, all captured against the same LCD frame.
 r.tap(0x51);r.tap(0x51);r.tap(0x51);r.settle();
 std::cout<<"BURST expected=4 actual="<<r.ime.view()->row<<'\n';
 CHECK(r.ime.view()->row==4);
 r.tap(0x51);r.tap(0x52);r.tap(0x52);r.settle();
 CHECK(r.ime.view()->row==3);
 ordered(false);ordered(true);selectionAndCancel();bounded();
 std::cout<<"PASS continuous relative navigation and reversal\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
