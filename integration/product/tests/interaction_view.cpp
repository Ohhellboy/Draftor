#define main original_query_rows_main
#include "query_rows.cpp"
#undef main
struct MixedFont: s3host::FontPort {
 s3host::GlyphView glyph(uint32_t cp)const noexcept override{ s3host::GlyphView g;g.advance=cp<128?7:cp>0xffff?30:28;return g;}
 uint16_t lineHeight()const noexcept override{return 34;}uint16_t defaultAdvance()const noexcept override{return 28;}
};
void viewport(){
 MixedFont font;SelectedViewport viewport;hanime_extension::Snapshot s;
 std::string text="甲乙丙丁戊己庚辛壬癸子丑寅卯辰巳午未申酉戌亥";
 std::array<hanime_extension::Member,22> members{};
 for(size_t i=0;i<members.size();++i){members[i].begin=i*2;members[i].end=i*2+2;members[i].text_begin=i*3;members[i].text_size=3;members[i].valid=true;}
 s.selected_text=text;s.members=members.data();s.member_count=members.size();s.focus=44;
 auto v=viewport.update(s,44,false,font);CHECK(v.left&&!v.right&&v.end==text.size());
 const auto original=text;
 v=viewport.update(s,0,true,font);CHECK(v.begin==0&&v.right);
 const auto first=v.begin;v=viewport.update(s,2,true,font);CHECK(v.begin==first);
 v=viewport.update(s,40,true,font);CHECK(v.begin>0&&v.end>=63);
 v=viewport.update(s,60,true,font);CHECK(v.end==text.size()); // unselected raw -> selected tail
 s.selected_text="甲a𠀀乙";s.member_count=1;members[0].text_size=s.selected_text.size();members[0].end=8;
 v=viewport.update(s,0,false,font);CHECK(v.begin==0&&v.end==s.selected_text.size());
 s.selected_text=text;members[0].text_size=text.size();members[0].end=44;
 v=viewport.update(s,0,true,font);CHECK(v.begin==0&&v.end>0&&v.right);
 v=viewport.update(s,42,true,font);CHECK(v.begin>0&&v.end==text.size());
 CHECK(text==original);s.selected_text={};v=viewport.update(s,0,false,font);CHECK(!v.begin&&!v.end);
 s.selected_text="甲a𠀀乙";members[0].text_size=s.selected_text.size();
 for(size_t cursor=0;cursor<44;++cursor){v=viewport.update(s,cursor,true,font,36);
  CHECK(v.begin<=v.end&&v.end<=s.selected_text.size());
  for(auto at:{v.begin,v.end})CHECK(at==s.selected_text.size()||(uint8_t(s.selected_text[at])&0xc0)!=0x80);
 }
 viewport.reset();v=viewport.update(s,0,false,font);CHECK(v.begin==0);
 std::cout<<"PASS selected viewport mixed widths supplementary wide-member shrink reset; bytes="<<sizeof(viewport)<<'\n';
}
void presentation(){
 Rig r;r.begin(false,"ui",false);r.ime.window()->request(4,0,true,true,22);r.settle();
 const auto view=*r.ime.view();r.ime.window()->request(6,0,true,true,23);
 CHECK(r.ime.window()->busy()&&r.ime.window()->requestedRow()==6&&r.ime.view()->row==4);
 UiTraceRecord record;while(uiTrace.pop(record)){}
 const auto& row=view.rows[view.row-view.rows[0].index];
 traceUi(UiTraceKind::Lcd,r.ime.composition(),r.ime.window(),nullptr,&row.items[view.col],0,22,100);
 CHECK(uiTrace.pop(record));CHECK(record.action==22&&record.target_row==4&&record.target_col==view.col);
 CHECK(record.view_frame==view.generation&&record.first_row==view.rows[0].index&&record.ref==row.items[view.col].ref);
 char output[384];CHECK(formatUiTrace(record,4,output,sizeof(output))>0);
 CHECK(std::string(output).find("target_row=4")!=std::string::npos);
 std::cout<<"PASS presented frame="<<record.view_frame<<" action="<<record.action<<" target="<<record.target_row<<" next_request="<<r.ime.window()->requestedRow()<<" trace_bytes="<<sizeof(record)<<'\n';
}
int main(){try{viewport();presentation();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
