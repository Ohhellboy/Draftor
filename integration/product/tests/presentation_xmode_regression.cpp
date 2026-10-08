#define main inherited_query_rows_main
#include "query_rows.cpp"
#undef main
#include "product_input.hpp"
using Mode=s3app::LayoutMode;
const std::string AnchorBody="天地玄黄宇宙洪荒日月盈昃\n辰宿列张寒来暑往秋收冬藏\n闰余成岁律吕调阳云腾致雨\n露结为霜金生丽水玉出昆冈\n剑号巨阙珠称夜光果珍李柰\n菜重芥姜海咸河淡鳞潜羽翔\n龙师火帝鸟官人皇始制文字\n乃服衣裳推位让国有虞陶唐";
struct CrossRig:Rig {
 s3app::ProductInput input;
 void toggle(){for(bool down:{true,false})CHECK(input.layout(app,{1,++now,61,0,down}));}
 void prepare(){CHECK(app.preparePresentation());}
 void body(){CHECK(app.commitText(AnchorBody,app.editStamp()));app.moveToByte(157,false);}
};
int main(){try{
 // C2: destination V is independent of three distinct H source positions.
 std::uint32_t expected_v=UINT32_MAX;
 for(unsigned slot:{0U,3U,6U}){CrossRig r;r.body();r.app.beginCompositionPresentation(true);CHECK(r.app.navigator.placeCaretSlot(slot));r.prepare();r.toggle();r.prepare();auto&v=r.app.verticalPresentation();CHECK(v.viewport().visibleColumns()==5);if(expected_v==UINT32_MAX)expected_v=v.viewport().firstColumn();CHECK(v.viewport().firstColumn()==expected_v);std::cout<<"C2 source_H_slot="<<slot<<" target_V_first="<<expected_v<<" capacity=5\n";}
 // C3/C4/C5: each destination remembers its own first; changing V's width
 // cannot overwrite H's bookmark. Target H capacity is deliberately identical.
 {CrossRig r;r.body();r.app.layout.setHeightPx(244);CHECK(r.app.navigator.placeCaretSlot(3));r.prepare();const auto h=r.app.navigator.sync();r.app.beginCompositionPresentation(true);
  for(unsigned cols:{1U,3U,1U,2U,1U}){r.toggle();r.app.candidateColumns(cols);r.prepare();auto&v=r.app.verticalPresentation();std::cout<<"C4 V columns="<<cols<<" first="<<v.viewport().firstColumn()<<" capacity="<<v.viewport().visibleColumns()<<'\n';r.toggle();r.prepare();const auto back=r.app.navigator.sync();CHECK(back.first_visible_line==h.first_visible_line&&back.visual_line==h.visual_line&&back.cursor_x==h.cursor_x);std::cout<<"C4 H first="<<back.first_visible_line<<" slot="<<back.visual_line-back.first_visible_line<<'\n';}
 }
 // C6: first entry uses target capacity only, with one forward context slot.
 for(unsigned capacity:{3U,5U,7U,9U}){CrossRig r;r.toggle();r.prepare();r.body();r.app.beginCompositionPresentation(true);r.prepare();r.app.layout.setHeightPx(capacity*r.app.layout.lineHeightPx());r.toggle();r.prepare();const auto h=r.app.navigator.sync();CHECK(h.visible_lines==capacity);CHECK(h.first_visible_line==(h.visual_line>capacity-2?h.visual_line-(capacity-2):0));std::cout<<"C6 H capacity="<<capacity<<" first="<<h.first_visible_line<<" caret="<<h.visual_line<<'\n';}
 // Same-mode policy remains the native navigator, without entry repositioning.
 {CrossRig r;r.body();r.app.layout.setHeightPx(244);r.app.navigator.placeCaretSlot(0);const auto h=r.app.navigator.sync();for(unsigned i=0;i<8;++i)r.prepare();CHECK(r.app.navigator.sync().first_visible_line==h.first_visible_line);r.toggle();r.prepare();auto&v=r.app.verticalPresentation();CHECK(v.placeCaretSlot(0));const auto first=v.viewport().firstColumn();for(unsigned i=0;i<8;++i)r.prepare();CHECK(v.viewport().firstColumn()==first);}
 CHECK(metrics.resource_handles==0&&metrics.document_handles==0);std::cout<<"PASS presentation_xmode_regression C2_C6=1 target_only=1 same_mode_no_entry_policy=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
