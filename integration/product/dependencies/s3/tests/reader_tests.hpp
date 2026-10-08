// Included by host_tests.cpp: use the same real App/storage/key fixture.
#include "reader_display.hpp"
#include "find_highlight.hpp"
#include <chrono>
namespace hf=hanime::find;
namespace ts=hanime::text_stats;
void readersDrain(Fixture& f){
 unsigned steps=0;
 do{f.app.service(++f.now,false);CHECK(++steps<150000);}
 while(f.app.find.view().status==hf::Status::Searching||f.app.statsReport().state==ts::State::Counting);
}
void query(Fixture& f,std::string_view text){CHECK(f.app.find.setQuery(text)!=hf::QueryEditResult::InvalidUtf8);f.key(40);readersDrain(f);}
void findBehaviour(){
 Fixture f;f.start();CHECK(f.app.commitText("One one ONE\n中文 one £",f.app.editStamp()));f.key(74,1);f.save();
 const auto body=f.app.document.text();const auto revision=f.app.document.textRevision();
 f.key(9,8);CHECK(f.app.findOpen());CHECK(f.app.layout.heightPx()==s3app::display::bodyHeight);
 CHECK(std::string(s3app::display::findStatus(f.app.find.view()))=="Empty query");
 query(f,"one");CHECK(f.app.find.view().match.begin==0);
 f.key(40);readersDrain(f);CHECK(f.app.find.view().match.begin==4);
 f.key(40,2);readersDrain(f);CHECK(f.app.find.view().match.begin==0);
 f.key(40,2);readersDrain(f);CHECK(f.app.find.view().wrapped&&f.app.find.view().match.begin==19);
 f.key(40);readersDrain(f);CHECK(f.app.find.view().wrapped&&f.app.find.view().match.begin==0);
 f.key(12,1);CHECK(f.app.find.view().case_sensitive&&!f.app.findDocument.matchHighlight().visible);
 f.key(40);readersDrain(f);CHECK(f.app.find.view().match.begin==4);
 query(f,"missing");CHECK(f.app.find.view().status==hf::Status::NotFound);
 query(f,"");CHECK(f.app.find.view().status==hf::Status::EmptyQuery);
 query(f,"中文");CHECK(f.app.find.view().match.begin==12&&f.app.find.view().match.end==18);
 auto before=f.app.find.view().query;CHECK(before=="中文");
 f.key(41);CHECK(!f.app.findOpen()&&!f.app.findDocument.matchHighlight().visible&&f.app.document.cursorByte()==18);
 CHECK(f.app.layout.heightPx()==306);f.key(9,1);CHECK(f.app.find.view().query=="中文");
 CHECK(f.app.find.setQuery("\x80")==hf::QueryEditResult::InvalidUtf8);
 CHECK(f.app.find.setQuery(std::string(257,'a'))==hf::QueryEditResult::CapacityExceeded);
 query(f,"one");f.key(41);
 CHECK(f.app.document.text()==body&&f.app.document.textRevision()==revision&&!f.app.document.dirty()&&!f.app.persistence.bodyDirty());
 f.key(29,1);CHECK(f.app.document.text().empty()); // Find created no history entries
 f.key(28,1);CHECK(f.app.document.text()==body);
}
void findInputBoundaries(){
 Fixture f;f.start();CHECK(f.app.commitText("Apple £ APPLE",f.app.editStamp()));f.key(74,1);
 f.key(9,1);f.key(4,2);f.key(19);f.key(19);f.key(15);f.key(8);CHECK(f.app.find.view().query=="Apple");
 auto stamp=f.app.queryStamp();CHECK(!f.app.commitText("leak",f.app.editStamp()));CHECK(f.app.commitQuery(" £",stamp));CHECK(!f.app.commitQuery("stale query",stamp));
 f.key(80);f.key(42);CHECK(f.app.find.view().query=="Apple£");f.key(76);CHECK(f.app.find.view().query=="Apple");
 CHECK(f.app.commitQuery("中文",f.app.queryStamp()));f.key(42);CHECK(f.app.find.view().query=="Apple中");
 f.key(41);CHECK(!f.app.commitQuery("stale",stamp));
 s3hid::InputRoute route;route.framePresented(f.app.focusToken());
 auto opening=s3hid::Event{1,++f.now,9,1,true,route.epoch()};auto oldBody=s3hid::Event{1,++f.now,27,0,true,route.epoch()};
 CHECK(route.accept(opening,f.now));f.app.key(opening);route.framePresented(f.app.focusToken());CHECK(!route.accept(oldBody,f.now));
 auto oldFind=s3hid::Event{1,++f.now,28,0,true,route.epoch()};auto oldStamp=f.app.queryStamp();
 f.app.disconnected();CHECK(!f.app.findOpen());route.framePresented(f.app.focusToken());CHECK(!route.accept(oldFind,f.now));
 CHECK(!f.app.commitQuery("late",oldStamp));auto focus=f.app.focusToken();f.app.disconnected();CHECK(f.app.focusToken()!=focus); // reconnect epoch too
 f.key(9,1);CHECK(f.app.findOpen());f.key(41);f.key(5);CHECK(f.app.document.text().find('b')!=draftor::memory::String::npos);
}
void findInvalidation(){
 Fixture f;f.start();CHECK(f.app.commitText("alpha alpha",f.app.editStamp()));f.key(74,1);f.key(9,1);query(f,"alpha");
 CHECK(f.app.findDocument.matchHighlight().visible);
 // Actual document mutation outside query focus, standing in for future host edits.
 CHECK(s3app::AppTestAccess::document(f.app).replaceRange(0,5,"omega",5)==editor::EditResult::Changed);f.app.session.onBodyMutation(++f.now);f.app.service(++f.now,false);
 CHECK(!f.app.findDocument.matchHighlight().visible&&f.app.find.view().status==hf::Status::CancelledOrStale);
 f.key(40);readersDrain(f);CHECK(f.app.find.view().match.begin==6);
 f.app.find.setQuery("absent");f.key(40);CHECK(f.app.find.view().status==hf::Status::Searching);
 CHECK(s3app::AppTestAccess::document(f.app).insertUtf8("!")==editor::EditResult::Changed);f.app.service(++f.now,false);CHECK(f.app.find.view().status==hf::Status::CancelledOrStale);
 // A direct stale close used to leave the reused controller stuck in FindBar.
 CHECK(s3app::AppTestAccess::document(f.app).insertUtf8("!")==editor::EditResult::Changed);f.app.findController.closeForExternalFocus(false);
 CHECK(f.app.findController.focus()==hf::input::Focus::Body&&!f.app.findOpen());
 f.app.disconnected();f.key(9,1);CHECK(f.app.findOpen());f.key(41);
 f.save();f.key(9,1);query(f,"alpha");auto stamp=f.app.queryStamp();
 CHECK(f.app.session.create("Direct install while finding",++f.now));
 CHECK(!f.app.findOpen()&&!f.app.findDocument.matchHighlight().visible);
 CHECK(f.app.findController.focus()==hf::input::Focus::Body);
 CHECK(!f.app.commitQuery("old document",stamp));readersDrain(f);CHECK(f.app.statsReport().counts.characters==0);
}
void statisticsRules(){
 struct Sample {const char* text;uint64_t chars,noSpace,latin,han,numbers;};
 const Sample samples[]={
  {"",0,0,0,0,0},{" \t\n\r",4,0,0,0,0},{"Hello world",11,10,2,0,0},
  {"中文 hello 123",12,10,1,2,1},{"don't re-enter 3.14 abc123",26,23,3,0,1},
  {"ＡＢＣ１２ 中\u0301 😀",10,8,1,1,0},{"café cafe\u0301",10,9,2,0,0}
 };
 for(const auto& sample:samples)for(size_t budget:{size_t(1),size_t(2),size_t(3),size_t(7),size_t(256)}){
  ts::Counter counter;ts::Source source{sample.text,0x100000001ULL,7};counter.observe(source);
  while(!counter.report().complete()){CHECK(counter.service(source,budget)<=budget);CHECK(counter.report().state==ts::State::Counting||counter.report().complete());}
  auto c=counter.report().counts;CHECK(c.characters==sample.chars);CHECK(c.characters_no_space==sample.noSpace);CHECK(c.latin_words==sample.latin);CHECK(c.han_characters==sample.han);CHECK(c.number_tokens==sample.numbers);
 }
 ts::Counter c;c.service({"\xf0\x9f",1,1},1);CHECK(c.report().state==ts::State::Counting);c.service({"\xf0\x9f",1,1},1);CHECK(c.report().state==ts::State::InvalidUtf8);
 c.reset();c.service({"abc",1,1},1);c.service({"abcd",1,1},1);CHECK(c.report().state==ts::State::InvalidSource);
}
void statsEdits(){
 Fixture f;f.start();readersDrain(f);CHECK(f.app.statsReport().complete()&&f.app.statsReport().counts.characters==0);
 CHECK(f.app.commitText("English 中文\n",f.app.editStamp()));readersDrain(f);CHECK(f.app.statsReport().counts.han_characters==2&&f.app.statsReport().counts.latin_words==1);
 auto old=f.app.statsReport();auto completed=f.app.scans.stats_completed;
 for(unsigned i=0;i<30;++i){f.key(80);f.app.navigator.sync();f.app.service(++f.now,false);}
 CHECK(f.app.statsReport().text_revision==old.text_revision&&f.app.scans.stats_completed==completed);
 f.key(4,1);f.key(6,1);f.key(77,1);f.key(25,1);readersDrain(f);CHECK(f.app.statsReport().counts.han_characters==4&&f.app.statsReport().counts.latin_words==2);
 f.key(29,1);readersDrain(f);CHECK(f.app.statsReport().counts.han_characters==2);f.key(28,1);readersDrain(f);CHECK(f.app.statsReport().counts.han_characters==4);
 f.key(4,1);f.key(42);readersDrain(f);CHECK(f.app.statsReport().counts.characters==0);f.key(29,1);readersDrain(f);CHECK(f.app.statsReport().counts.han_characters==4);
 auto text=f.app.document.text();auto rev=f.app.document.textRevision();auto cursor=f.app.document.cursorByte();f.key(60);CHECK(f.app.statsOpen());f.key(4);CHECK(f.app.document.text()==text);f.key(60);CHECK(!f.app.statsOpen());
 CHECK(f.app.document.cursorByte()==cursor&&f.app.document.textRevision()==rev);
 // Cross-step UTF-8 and a still-open word, then an append reuses the prefix.
 CHECK(s3app::AppTestAccess::document(f.app).replaceAll(std::string(255,'a')+"中文",261,true));f.app.service(++f.now,false);CHECK(f.app.statsReport().scanned_bytes==256);
 CHECK(s3app::AppTestAccess::document(f.app).insertUtf8("word")==editor::EditResult::Changed);f.app.service(++f.now,false);readersDrain(f);
 CHECK(f.app.statsReport().counts.characters==261&&f.app.statsReport().counts.han_characters==2&&f.app.statsReport().counts.latin_words==2);
}
void readersCombined(){
 Fixture f;f.start();auto a=f.app.session.currentId();CHECK(f.app.commitText("UnSaved alpha beta ALPHA",f.app.editStamp()));auto text=f.app.document.text();f.key(74,1);f.key(9,1);query(f,"alpha");
 // Idle AUTO runs even with query focus. No special reader save owner.
 f.now+=6000;f.app.service(f.now,false);CHECK(f.app.persistence.backgroundSaveActive());f.settle();CHECK(f.app.findOpen());
 CHECK(f.app.persistence.loadLane(a,ds::SaveLane::Auto).snapshot.text==text);CHECK(!f.app.document.dirty());
 f.key(41);auto cursor=f.app.document.cursorByte();f.save();CHECK(f.app.persistence.loadLane(a,ds::SaveLane::Manual).snapshot.cursor_byte==cursor);
 f.key(9,1);f.app.find.setQuery("missing");f.key(40);f.key(18,1);CHECK(f.app.filesOpen()&&!f.app.findOpen());
 f.key(17);f.settle();CHECK(!f.app.filesOpen()&&f.app.session.currentId()!=a);readersDrain(f);CHECK(f.app.statsReport().counts.characters==0);f.key(9,1);CHECK(f.app.find.view().query.empty());f.key(41);
 CHECK(f.app.session.open(a,++f.now));readersDrain(f);CHECK(f.app.document.text()==text&&f.app.document.cursorByte()==cursor);
 f.key(4);f.key(60);CHECK(f.app.statsOpen());f.now+=6000;f.app.service(f.now,false);CHECK(f.app.persistence.backgroundSaveActive());f.settle();CHECK(f.app.statsOpen());
 f.key(18,1);CHECK(!f.app.statsOpen()&&f.app.filesOpen());f.key(41);f.key(60);f.app.disconnected();CHECK(!f.app.statsOpen());
 // UI exit while save is underway must not cancel or alter its snapshot.
 f.key(60);f.key(22,1);CHECK(f.app.persistence.backgroundSaveActive());f.key(41);f.settle();CHECK(!f.app.statsOpen()&&f.app.lastOperation);
}
void readersLongBudget(){
 Fixture f;f.start();std::string text(65530,'a');text+="中文";CHECK(text.size()==65536);CHECK(s3app::AppTestAccess::document(f.app).replaceAll(text,0,true));s3app::AppTestAccess::editor(f.app).resetForDocument();f.app.session.onBodyMutation(++f.now);
 f.key(9,1);CHECK(f.app.find.setQuery(std::string(255,'a')+"b")==hf::QueryEditResult::Applied);f.key(40);
 auto source=f.app.document.text().data();const auto revision=f.app.document.textRevision();
 f.key(22,1);CHECK(f.app.persistence.backgroundSaveActive());
 auto progress=f.app.statsReport().scanned_bytes;auto findCpu=f.app.scans.find_us;
 f.app.service(++f.now,true);CHECK(f.app.statsReport().scanned_bytes==progress&&f.app.scans.find_us==findCpu);
 unsigned steps=0;uint64_t maxUs=0;bool savedWhileSearching=false;
 while(f.app.find.view().status==hf::Status::Searching||f.app.persistence.backgroundSaveActive()||!f.app.statsReport().complete()){
  auto begin=s3profile::clockUs();f.app.service(++f.now,false);maxUs=std::max(maxUs,s3profile::clockUs()-begin);
  CHECK(f.app.find.lastStepWork()<=256);CHECK(++steps<150000);
  if(!f.app.persistence.backgroundSaveActive()&&f.app.find.view().status==hf::Status::Searching)savedWhileSearching=true;
 }
 CHECK(savedWhileSearching&&f.app.find.view().status==hf::Status::NotFound);
 CHECK(f.app.scans.max_find_work<=256&&f.app.scans.max_stats_bytes<=256);
 CHECK(f.app.statsReport().counts.characters==65532&&f.app.statsReport().counts.han_characters==2&&f.app.statsReport().counts.latin_words==1);
 CHECK(f.app.document.text().data()==source&&f.app.document.text()==text.c_str()&&f.app.document.textRevision()==revision);
 query(f,"中文");CHECK(f.app.find.view().match.begin==65530);auto v=f.app.navigator.sync();CHECK(v.visual_line>=v.first_visible_line&&v.visual_line<v.first_visible_line+f.app.layout.visibleTextLineCount());
 f.key(41);CHECK(f.app.document.cursorByte()==65536);f.save();auto id=f.app.session.currentId();CHECK(f.app.session.create("Long reader other",++f.now));CHECK(f.app.session.open(id,++f.now));CHECK(f.app.document.cursorByte()==65536&&f.app.document.text()==text.c_str());
 std::printf("READERS_64K steps=%u max_host_service_us=%llu find_step_us=%llu stats_step_us=%llu max_find_work=%zu max_stats_bytes=%zu sizeof_App=%zu\n",steps,(unsigned long long)maxUs,(unsigned long long)f.app.scans.max_find_step_us,(unsigned long long)f.app.scans.max_stats_step_us,f.app.scans.max_find_work,f.app.scans.max_stats_bytes,sizeof(s3app::App));
}
void readersDisplay(){
 Fixture f;f.start();CHECK(f.app.commitText("alpha beta alpha\nFind keeps text unchanged.",f.app.editStamp()));f.key(74,1);f.key(9,1);query(f,"alpha");
 auto view=f.app.navigator.sync();hanime::renderer::Placement placements[80];auto line=f.app.layout.layoutVisualLine(f.app.document.text(),f.app.navigator.firstVisibleAnchor(),view.first_visible_line,placements,80);
 hf::display::HighlightRect rects[80];auto hl=hf::display::matchHighlightRects(f.app.findDocument.matchHighlight(),placements,line.placements_written,f.app.layout.lineHeightPx(),rects,80);CHECK(hl.rectangles_written==1&&rects[0].width>0);
 std::vector<std::string> labels;auto label=[&](const char* t,int x,int y){CHECK(x>=0&&y>=0&&y<340);labels.emplace_back(t);};
 s3app::display::findBar(f.app.find.view(),f.smallFont,label,[](uint32_t,int x,int y){CHECK(x>=48&&x<=292&&y==278);},[](int x,int y,int h){CHECK(x>=48&&x<=292&&y+h<300);});
 CHECK(labels[2].find("IGNORE")!=std::string::npos);
 f.app.find.setQuery(std::string(230,'W')+"中文");s3app::display::findBar(f.app.find.view(),f.smallFont,label,[](uint32_t,int x,int){CHECK(x<=292);},[](int x,int,int){CHECK(x<=292);});
 labels.clear();ts::Report counting;counting.state=ts::State::Counting;counting.counts.characters=99999;counting.total_bytes=65536;counting.scanned_bytes=256;
 s3app::display::statistics(counting,label);for(const auto& s:labels)CHECK(s.find("99999")==std::string::npos);
 labels.clear();readersDrain(f);s3app::display::statistics(f.app.statsReport(),label);CHECK(labels.size()==10);
}

void renderReaders(const std::string& path){
 Fixture f;f.start();CHECK(f.app.commitText("Alpha beta ALPHA\nFind matches stay in the\ncurrent document.\n\nStatistics include edits\nbefore they are saved.",f.app.editStamp()));f.key(74,1);f.key(9,1);query(f,"alpha");
 constexpr int width=900,height=400;std::vector<unsigned char> pixels(width*height*3,255);
 int panel=0;
 auto pixel=[&](int x,int y,bool white){CHECK(x>=0&&x<300&&y>=0&&y<400);size_t at=(y*width+panel*300+x)*3;for(int c=0;c<3;++c)pixels[at+c]=white?255:0;};
 auto rect=[&](int x,int y,int w,int h,bool white){for(int r=0;r<h;++r)for(int c=0;c<w;++c)pixel(x+c,y+r,white);};
 auto glyph=[&](uint32_t cp,int x,int y,bool white,unsigned scale){auto g=scale==2?f.smallFont.glyph(cp):f.font.glyph(cp);for(unsigned r=0;r<g.height;++r)for(unsigned c=0;c<g.width;++c)if(g.bits[r*g.stride+c/8]&(0x80>>(c%8)))pixel(x+g.bearing+c,y+r,white);};
 auto label=[&](const char* text,int x,int y){std::string_view value(text);for(size_t at=0;at<value.size();at=editor::nextUtf8Boundary(value,at)){auto cp=s3app::display::scalar(value,at);auto g=f.smallFont.glyph(cp);if(x+g.advance>294)break;glyph(cp,x,y,false,2);x+=g.advance;}};
 for(panel=0;panel<3;++panel){
  label("S3 Writing v0.5.4 DocManager",8,5);
  if(panel==0){
   label("New acceptance document",8,20);
   auto v=f.app.navigator.sync();auto anchor=f.app.navigator.firstVisibleAnchor();hanime::renderer::Placement placements[80];
   auto match=f.app.findDocument.matchHighlight();
   unsigned whiteInk=0;
   for(unsigned row=0;row<f.app.layout.visibleTextLineCount()&&anchor.valid;++row){
    auto r=f.app.layout.layoutVisualLine(f.app.document.text(),anchor,v.first_visible_line,placements,80);
    hf::display::HighlightRect rectangles[80];auto h=hf::display::matchHighlightRects(match,placements,r.placements_written,f.app.layout.lineHeightPx(),rectangles,80);
    for(size_t i=0;i<h.rectangles_written;++i)rect(8+rectangles[i].x,34+rectangles[i].y,rectangles[i].width,rectangles[i].height,false);
    for(size_t i=0;i<r.placements_written;++i){auto p=placements[i];bool selected=match.visible&&p.byte_offset>=match.begin&&p.byte_offset<match.end;if(selected)++whiteInk;if(p.codepoint!='\n'&&p.codepoint!='\r')glyph(p.codepoint,p.x+8,p.y+34,selected,1);}
    if(r.document_end) { break; }
    anchor=r.next;
   }
   CHECK(whiteInk==5);
   s3app::display::findBar(f.app.find.view(),f.smallFont,label,[&](uint32_t cp,int x,int y){glyph(cp,x,y,false,2);},[&](int x,int y,int h){rect(x,y,1,h,false);});
   label("UNSAVED",8,348);label("108 B  cursor 0",82,348);
  }else if(panel==1){ts::Report r;r.state=ts::State::Counting;r.total_bytes=65536;r.scanned_bytes=32768;s3app::display::statistics(r,label);}
  else s3app::display::statistics(f.app.statsReport(),label);
  label("BLE ready",8,383);
 }
 std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<" "<<height<<"\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());CHECK(out.good());
 std::puts("PASS raster: actual font, black/white match, Find / counting / ready, labels fit screen");
}
