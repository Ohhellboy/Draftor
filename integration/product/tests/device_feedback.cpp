// Production resources, public input/selection/commit path; no device timing.
#define main existing_query_rows_main
#include "query_rows.cpp"
#undef main
void editorSelect(){
 for(bool py:{false,true})for(uint8_t key:{uint8_t(0x2c),uint8_t(0x1e)}){
  Rig r;r.begin(py,py?"wojintian":"wojbtm",false);
  r.tap(0x50);r.settle();CHECK(r.ime.rawEditing());
  r.tap(key);r.settle();CHECK(r.ime.selected()=="我");
  CHECK(r.app.document.text().empty());
  r.tap(key==0x2c?0x1e:0x2c);r.settle();CHECK(r.app.document.text()=="我今天");
 }
 std::cout<<"PASS moved cursor without edit: Space/digit continues both schemes once\n";
}

void editedSuffix(){
 for(uint8_t key:{uint8_t(0x2c),uint8_t(0x1e)}){
  Rig r;r.begin(false,"womktm",false);r.tap(0x2c);r.settle();
  CHECK(r.ime.selected()=="我");const auto member=r.ime.composition()->snapshot().members[0];
  r.tap(0x50);r.settle();CHECK(r.ime.rawCursor()==4);
  r.tap(0x2a);r.settle();r.type("k");r.settle();
  CHECK(r.ime.rawEditing()&&r.ime.raw()=="womktm"&&r.ime.selected()=="我");
  CHECK(r.ime.composition()->snapshot().members[0].key==member.key);
  r.tap(key);r.settle();CHECK(r.app.document.text()=="我明天"&&!r.ime.active());
 }
 std::cout<<"PASS one-letter suffix edit retains selected identity; either final key accepts once\n";
}
void reopened(){
 Rig r;r.begin(false,"womktm",false);r.tap(0x2c);r.settle();
 r.tap(0x50);r.tap(0x50);r.tap(0x50);r.settle();CHECK(r.ime.rawCursor()==0);
 r.tap(0x51);r.settle();CHECK(!r.ime.rawEditing()&&r.app.document.text().empty());
 // Confirmed Down-origin Esc returns to the same raw position; reopening
 // invalidates membership intentionally, and Esc never restores that validity.
 const auto member=r.ime.composition()->snapshot().members[0];
 r.tap(0x29);r.settle();CHECK(r.ime.rawEditing()&&r.ime.raw()=="womktm"&&r.ime.rawCursor()==0);
 CHECK(r.ime.composition()->snapshot().members[0].key==member.key&&!r.ime.composition()->snapshot().members[0].valid);
 // With the first member now invalid, zero is a boundary. Move one syllable.
 r.tap(0x4f);
 r.tap(0x51);r.settle();r.tap(0x1e);r.settle();CHECK(r.ime.selected()=="我");
 r.tap(0x50);r.settle();r.tap(0x2c);r.settle();CHECK(r.app.document.text()=="我明天");
 std::cout<<"PASS K1 reopened member then raw editing resumes suffix selection; no partial body\n";
}
void staleAndEnter(){
 for(uint8_t key:{uint8_t(0x2c),uint8_t(0x1e)}){
  Rig r;r.begin(false,"ui",false);r.tap(0x50);r.settle();r.tap(0x4f);r.settle();
  r.tap(0x2a);r.type("i"); // New press follows edit: default and numbered initial row bind this query.
  r.tap(key);CHECK(r.app.document.text().empty());r.settle();
  CHECK(r.app.document.text()=="是"&&r.ime.raw().empty());
 }
 {Rig r;r.begin(false,"ui",false);r.tap(0x50);r.settle();r.tap(0x28);r.settle();
 CHECK(r.ime.rawEditing()&&r.app.document.text().empty()&&r.ime.selected().empty());
 r.tap(0x29);r.settle();r.tap(0x28);r.settle();CHECK(r.ime.selected()=="是"&&r.app.document.text().empty());
 r.tap(0x28);r.settle();CHECK(r.app.document.text()=="是");}
 std::cout<<"PASS new current default after edit resolves; fixed initial number resolves; Enter unchanged\n";
}
void pixelsAndEdges(){
 s3host::BuiltinSmallFont font;
 CHECK(font.glyph('w').advance==11&&font.glyph('i').advance==3&&font.glyph('j').advance==4);
 for(const auto& raw:{std::string("wojbtmwomktm"),std::string(64,'w'),std::string(64,'i')}){
  for(size_t cursor=0;cursor<=raw.size();++cursor){
   const auto v=rawViewport(raw,cursor,true,font);unsigned cursorX=0,width=0;
   for(size_t i=v.begin;i<v.end;++i){if(i<cursor)cursorX+=font.glyph(raw[i]).advance;width+=font.glyph(raw[i]).advance;}
   CHECK(v.begin<=cursor&&cursor<=v.end&&v.end<=raw.size());
   CHECK(v.cursor_x==cursorX&&v.width==width&&width<284);
  }
 }
 Rig r;r.begin(false,"ui",false);r.tap(0x52);CHECK(r.ime.boundaryEdge()==BoundaryEdge::CandidateTop);
 for(size_t row=0;row<200;++row){r.ime.window()->request(row,0,true);r.settle();
  const auto& v=*r.ime.view();if(v.at_end&&v.row==v.rows[0].index+v.count-1)break;CHECK(row<199);}
 // Establish candidate branch (initial Down has its separate confirmed entry).
 r.tap(0x51);r.settle();
 for(size_t row=0;row<200;++row){r.ime.window()->request(row,0,true);r.settle();
  const auto& v=*r.ime.view();if(v.at_end&&v.row==v.rows[0].index+v.count-1)break;}
 const auto last=digest(*r.ime.view());r.tap(0x51);r.settle();
 CHECK(r.ime.boundaryEdge()==BoundaryEdge::CandidateBottom&&digest(*r.ime.view())==last);
 for(size_t rows:{1u,2u,3u}){auto rect=boundaryRect(r.ime.boundaryEdge(),210,rows);CHECK(rect.y==210+48+int(rows)*34-4);}
 r.tap(0x52);r.settle();CHECK(digest(*r.ime.view())!=last);
 Rig e;e.begin(false,"ui",false);e.tap(0x50);e.tap(0x50);e.settle();CHECK(e.ime.boundaryEdge()==BoundaryEdge::RawLeft);
 const auto rect=boundaryRect(e.ime.boundaryEdge(),278,1);CHECK(rect.y==308&&rect.height==14);
 std::cout<<"PASS real font pixel viewport/cursor, bottom edge by visible rows, reverse, raw edge separation\n";
}
int main(){try{editorSelect();editedSuffix();reopened();staleAndEnter();pixelsAndEdges();return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
