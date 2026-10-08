#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
s3hid::Event receive(Rig&r,uint8_t key,uint32_t token=0){
 s3hid::Event e{1,++r.now,key,0,true,0,r.frame,r.ime.attemptSerial()};e.ime_context=token?token:r.ime.ingressContext();e.resource_owner=r.ime.resourceOwner();return e;
}
void deliver(Rig&r,s3hid::Event e){r.ime.event(e);e.down=false;e.ms=++r.now;r.ime.event(e);}
void untilReady(Rig&r){unsigned steps=0;while(r.ime.hasWork()){r.step();CHECK(++steps<2000000);}}
void first(bool py,uint8_t key,unsigned when,bool edited){
 Rig r;r.begin(py,py?"shi":"ui",false);r.tap(0x29);r.settle();r.type(py?"shi":"ui");
 if(edited){r.type("a");r.tap(0x2a);}r.frame=0;
 if(when){untilReady(r);}
 if(when==3)r.frame=r.ime.framePresented();
 const auto e=receive(r,key);
 if(when==0||when==2){untilReady(r);r.frame=r.ime.framePresented();}
 const auto& row=r.ime.view()->rows[0];
 std::string expected;
 if(key==0x2c)expected="是";else {if(!r.ime.view()->count)untilReady(r);const auto&i=row.items[key-30];expected.assign(i.text.data(),i.bytes);}
 deliver(r,e);r.settle();CHECK(std::string_view(r.app.document.text())==std::string_view(expected)&&r.ime.raw().empty());
 std::cout<<"TIMING py="<<py<<" key="<<unsigned(key)<<" when="<<when<<" edited="<<edited<<" result=PASS\n";
}
void replaced(unsigned change){
 Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();r.type(change==1?"wojbtm":"ui");r.frame=0;const auto e=receive(r,0x2c);
 if(change==0)r.ime.window()->query();
 else if(change==1)r.ime.window()->query(4);
 else if(change==2){r.ime.window()->cancel();CHECK(r.ime.composition()->edit_raw(0,2,"wo")==hanime_extension::Status::Ready);r.ime.window()->query();}
 else if(change==3)r.tap(0x29);
 else if(change==4)r.tap(0x2c,1);
 else if(change==5)r.app.commitText("x",r.app.editStamp());
 else if(change==6)r.ime.interrupt();
 else r.resources.mount()->epoch++;
 deliver(r,e);
 if(change!=7)r.settle();
 CHECK(r.app.document.text()==(change==5?"x":""));
 if(change<2)CHECK(r.ime.window()->state()==WindowState::Ready&&r.ime.view()->count);
 std::cout<<"TIMING replacement="<<change<<" rejected; replacement-query retained\n";
}
void numberedMissing(){
 Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();r.type("wjdjle");r.frame=0;const auto e=receive(r,35);deliver(r,e);r.settle();
 // Initial row contains fewer than six complete candidates for this long prefix.
 CHECK(r.app.document.text().empty()&&r.ime.selected().empty()&&r.ime.raw()=="wjdjle");
 std::cout<<"TIMING missing initial numbered slot refuses without clearing input\n";
}
void suffixAndQueue(bool py,bool digits){
 Rig r;r.begin(py,py?"shi":"ui",false);r.tap(0x29);r.settle();r.type(py?"wojintian":"wojbtm");r.frame=0;
 auto one=receive(r,digits?30:0x2c);deliver(r,one);auto two=receive(r,digits?30:0x2c);deliver(r,two);r.settle();CHECK(r.app.document.text()=="我今天");
 r.type(py?"wandanle":"wjdjle");r.frame=0;deliver(r,receive(r,0x2c));unsigned n=0;while(r.ime.selected().empty()){r.step();CHECK(++n<2000000);}r.frame=0;
 const auto end=receive(r,digits?30:0x2c);untilReady(r);r.frame=r.ime.framePresented();deliver(r,end);r.settle();CHECK(r.app.document.text()=="我今天完蛋了");
 std::cout<<"TIMING queued digits/suffix LCD crossing py="<<py<<" digits="<<digits<<" PASS\n";
}
void batch(bool cancel){
 Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();r.frame=0;const auto token=r.ime.ingressContext();
 std::vector<s3hid::Event> queued;const std::vector<uint8_t> keys=cancel?std::vector<uint8_t>{24,12,44,41,24,12,30}:std::vector<uint8_t>{24,12,44,24,12,30};
 for(auto key:keys)for(bool down:{true,false}){auto e=receive(r,key,token);e.down=down;e.event_id=uint32_t(queued.size()+1);queued.push_back(e);}
 for(const auto&e:queued){if(!r.ime.event(e))r.app.key(e);}r.settle();CHECK(r.app.document.text()==(cancel?"是":"是是"));
 std::cout<<"TIMING actual queued HID letters/choices cancel="<<cancel<<" PASS\n";
}
void rawAndNavigation(){
 Rig r;r.begin(false,"ui",false);r.tap(0x50);r.type("a");r.tap(0x2a);r.frame=0;const auto e=receive(r,31);untilReady(r);r.frame=r.ime.framePresented();const auto item=r.ime.view()->rows[0].items[1];deliver(r,e);r.settle();CHECK(std::string_view(r.app.document.text())==std::string_view(item.text.data(),item.bytes));
 r.type("ui");r.settle();r.tap(0x51);r.settle();for(unsigned i=0;i<6;++i)r.tap(0x51);
 const auto old=r.ime.window()->presented(r.frame);CHECK(old);const auto oldItem=old->rows[old->row-old->rows[0].index].items[0];
 deliver(r,receive(r,30));r.settle();CHECK(std::string_view(r.app.document.text()).substr(item.bytes)==std::string_view(oldItem.text.data(),oldItem.bytes));
 r.type("ui");r.settle();r.tap(0x51);r.frame=0;deliver(r,receive(r,31));r.settle();CHECK(r.ime.selected().empty()&&r.ime.raw()=="ui");
 std::cout<<"TIMING raw-editor early number, original displayed identity and unknown navigation refusal PASS\n";
}
void layoutReplacement(){
 for(bool pending:{false,true}){Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();r.type("ui");r.frame=0;const auto e=receive(r,31);
 if(pending){deliver(r,e);}
 r.ime.window()->request(1,0,true);if(!pending){deliver(r,e);}r.settle();CHECK(r.app.document.text().empty()&&r.ime.selected().empty()&&r.ime.view()->row==1);}
 std::cout<<"TIMING replacement layout invalidates before and after numeric demand; new target survives\n";
}
void unknownToken(){
 for(uint32_t token:{0u,1234567u}){Rig r;r.begin(false,"ui",false);r.tap(0x29);r.settle();r.type("ui");r.frame=0;auto e=receive(r,0x2c);e.ime_context=token;untilReady(r);r.frame=r.ime.framePresented();deliver(r,e);r.settle();CHECK(r.app.document.text().empty()&&r.ime.raw()=="ui");}
 std::cout<<"TIMING zero/foreign ingress token cannot bind latest query PASS\n";
}
void nonzeroCurrent(bool py,uint8_t key,unsigned mode){
 Rig r;r.begin(py,mode==2?(py?"wandanle":"wjdjle"):(py?"shi":"ui"),false);
 const auto physical=r.frame;CHECK(physical);
 if(mode==0){r.tap(0x2a);r.type("i");}
 else if(mode==1){for(unsigned i=0;i<(py?3u:2u);++i){r.tap(0x2a);}r.type("wo");}
 else {r.tap(0x2c);unsigned n=0;while(r.ime.selected().empty()){r.step();CHECK(++n<2000000);}}
 CHECK(r.frame==physical);const auto e=receive(r,key);const auto stamp=r.ime.composition()->raw_input().stamp;const auto query=r.ime.window()->querySerial();
 untilReady(r);r.frame=r.ime.framePresented();CHECK(r.frame!=physical);
 CHECK(e.ime_context==r.ime.ingressContext()&&query==r.ime.window()->querySerial()&&r.ime.composition()->input_is_current(stamp));
 const auto item=r.ime.view()->rows[0].items[key==44?0:key-30];const std::string expected=(mode==2?"完蛋":"")+std::string(item.text.data(),item.bytes);
 deliver(r,e);r.settle();CHECK(std::string_view(r.app.document.text())==std::string_view(expected));
 std::cout<<"NONZERO current py="<<py<<" key="<<unsigned(key)<<" mode="<<mode<<" old="<<physical<<" PASS\n";
}
void nonzeroInvalid(unsigned change){
 Rig r;r.begin(false,change==8?"wojbtm":"ui",false);r.tap(0x2a);r.type(change==8?"m":"i");const auto e=receive(r,44);
 if(change==0)r.ime.window()->query();
 else if(change==1)r.ime.window()->request(1,0,true);
 else if(change==2)r.type("a");
 else if(change==3)r.tap(0x29);
 else if(change==4)r.tap(0x2c,1);
 else if(change==5)r.app.commitText("x",r.app.editStamp());
 else if(change==6)r.ime.interrupt();
 else if(change==8)r.ime.window()->query(4);
 else ++r.resources.mount()->epoch;
 deliver(r,e);if(change!=7){r.settle();}
 CHECK(r.app.document.text()==(change==5?"x":""));
 if(change==0||change==8){CHECK(r.ime.window()->state()==WindowState::Ready);}
 if(change==1){CHECK(r.ime.view()->row==1);}
 std::cout<<"NONZERO stale change="<<change<<" rejected; replacement survives\n";
}
void nonzeroUnknown(){
 for(unsigned type=0;type<3;++type){Rig r;r.begin(false,"ui",false);r.tap(0x2a);r.type("i");auto e=receive(r,44);
 if(type==0)e.ime_frame=UINT32_MAX;else e.ime_context=type==1?0:1234567;
 r.settle();deliver(r,e);r.settle();CHECK(r.app.document.text().empty()&&r.ime.raw()=="ui");}
 std::cout<<"NONZERO foreign frame/zero/foreign context rejected\n";
}
int main(){try{
 for(bool py:{false,true}){for(uint8_t key:{uint8_t(0x2c),uint8_t(31)})for(unsigned w=0;w<4;++w)for(bool edited:{false,true})first(py,key,w,edited);for(bool digits:{false,true})suffixAndQueue(py,digits);}
 for(unsigned c=0;c<8;++c){replaced(c);}numberedMissing();batch(false);batch(true);rawAndNavigation();layoutReplacement();unknownToken();
 for(bool py:{false,true}){for(uint8_t key:{uint8_t(44),uint8_t(30),uint8_t(31)}){for(unsigned mode=0;mode<3;++mode){nonzeroCurrent(py,key,mode);}}}
 for(unsigned change=0;change<9;++change){nonzeroInvalid(change);}nonzeroUnknown();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
