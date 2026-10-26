#include "find_display.hpp"
#include "reader_display.hpp"
#include <algorithm>
#include <cstdio>
namespace s3app::find_ui {
namespace {
using namespace ime_device;
const ui16::Font font;
unsigned extent(std::string_view s){unsigned n=0;for(size_t at=0;at<s.size();at=editor::nextUtf8Boundary(s,at))n+=font.glyph(display::scalar(s,at)).advance;return n;}
struct Paint {
 const PaintPort& port;
 void pixel(int x,int y,bool ink=true)const{if(x>=0&&x<300&&y>=0&&y<400)port.pixel(port.context,x,y,ink);}
 void rect(int x,int y,int w,int h,bool ink=true)const{for(int j=0;j<h;++j)for(int i=0;i<w;++i)pixel(x+i,y+j,ink);}
 void box(int x,int y,int w,int h)const{rect(x,y,w,1);rect(x,y+h-1,w,1);rect(x,y,1,h);rect(x+w-1,y,1,h);}
 int glyph(uint32_t cp,int x,int y,bool vertical=false,bool reverse=false)const{
  const auto g=font.glyph(cp);const bool rotate=vertical&&cp<128;
  const auto advance=vertical&&cp>=128?16:g.advance;
  if(reverse)rect(x,y,vertical?16:advance,vertical?advance:16);
  for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<std::min<unsigned>(g.width,advance);++col)
   if(g.bits[row*g.stride+col/8]&(0x80>>(col%8)))pixel(rotate?x+15-int(row):x+int(col),rotate?y+int(col):y+int(row),!reverse);
  return advance;
 }
 int text(std::string_view s,int x,int y,int available=284,bool vertical=false,bool reverse=false)const{
  int used=0;for(size_t at=0;at<s.size();at=editor::nextUtf8Boundary(s,at)){
   const auto cp=display::scalar(s,at);const int n=font.glyph(cp).advance;
   if(used+n>available)break;
   glyph(cp,vertical?x:x+used,vertical?y+used:y,vertical,reverse);used+=n;
  }return used;
 }
 void verticalLabel(std::string_view s,int x,int y,int step=19)const{
  for(size_t at=0;at<s.size();at=editor::nextUtf8Boundary(s,at)){glyph(display::scalar(s,at),x,y);y+=step;}
 }
};
struct Window {size_t begin=0,end=0;bool left=false,right=false;};
Window viewport(std::string_view text,size_t cursor,unsigned capacity){
 cursor=std::min(cursor,text.size());Window w;w.begin=cursor;unsigned used=0;
 // Reserve two ellipses and a caret. Walk only the bounded visible neighborhood.
 const unsigned body=capacity>34?capacity-34:0;
 while(w.begin){const auto previous=editor::previousUtf8Boundary(text,w.begin);const auto n=font.glyph(display::scalar(text,previous)).advance;if(used+n>body)break;used+=n;w.begin=previous;}
 w.left=w.begin!=0;used=w.left?16:0;w.end=w.begin;
 while(w.end<text.size()){const auto next=editor::nextUtf8Boundary(text,w.end);const auto n=font.glyph(display::scalar(text,w.end)).advance;if(used+n+16+2>capacity)break;used+=n;w.end=next;}
 w.right=w.end<text.size();return w;
}
void query(const Paint& p,const hanime::find::ViewModel& q,bool vertical,bool composing){
 const int x=vertical?100:14,y=vertical?84:242,capacity=vertical?239:272;
 p.box(vertical?96:8,vertical?78:236,vertical?25:284,vertical?251:28);
 const auto window=viewport(q.query,q.query_cursor,capacity);int used=0;
 const auto draw=[&](uint32_t cp,bool selected=false){const int n=p.glyph(cp,vertical?x:x+used,vertical?y+used:y,vertical,selected);used+=n;};
 if(window.left)draw(0x2026);
 for(size_t at=window.begin;at<window.end;at=editor::nextUtf8Boundary(q.query,at)){
  if(at==q.query_cursor&&!composing&&!q.query_selected)p.rect(vertical?x:x+used,vertical?y+used:y,vertical?16:1,vertical?1:16);
  draw(display::scalar(q.query,at),at>=q.selectionBegin()&&at<q.selectionEnd());
 }
 if(q.query_cursor==window.end&&!composing&&!q.query_selected)p.rect(vertical?x:x+used,vertical?y+used:y,vertical?16:1,vertical?1:16);
 if(window.right)draw(0x2026);
 if(q.query.empty()&&!composing){if(vertical)p.verticalLabel("输入内容",100,84);else p.text("输入查找内容",14,242);}
}
const char* mode(const DeviceIme* ime){return !ime||!ime->chinese()?"英文":ime->schemeRules()==hanime_extension::SchemeRules::FullPinyin?"全拼":ime->schemeRules()==hanime_extension::SchemeRules::DirectExact?"五笔":"双拼";}
void status(char* out,size_t capacity,const hanime::find::ViewModel& q,bool vertical){
 if(q.capacity_exceeded)std::snprintf(out,capacity,"%s",vertical?"查询已满":"查询已达上限");
 else if(q.query.empty())std::snprintf(out,capacity,"%s",vertical?"待输入":"请输入内容");
 else if(q.status==hanime::find::Status::NotFound)std::snprintf(out,capacity,"未找到");
 else if(q.status==hanime::find::Status::AdapterFailure)std::snprintf(out,capacity,"查找失败");
 else if(q.count_complete&&q.match.valid)std::snprintf(out,capacity,vertical?"%u/%u":"第%u项 / 共%u项",unsigned(q.current_index),unsigned(q.total_matches));
 else std::snprintf(out,capacity,"%s",vertical?"查找中":"正在查找");
}
void raw(const Paint& p,DeviceIme& ime,bool vertical){
 const auto text=ime.raw();const auto cursor=std::min(ime.rawCursor(),text.size());
 const bool editing=ime.rawEditing()||ime.reselecting();
 auto target=ime.editTarget();
 size_t mark_begin=target.begin,mark_end=target.end;
 if(!editing&&ime.window())if(const auto* item=ime.window()->focusedCurrent()){mark_begin=item->begin;mark_end=item->end;}
 p.text("原码",8,vertical?27:272);
 // Two 77 px rows in V; reserve extra width for a possible line break.
 const auto window=viewport(text,cursor,vertical?126:198);int x=vertical?8:48,y=vertical?48:272;
 const int start=x,limit=vertical?85:246;
 const auto draw=[&](uint32_t cp,size_t at){const int n=font.glyph(cp).advance;
  if(x+n>limit&&vertical){x=start;y=70;}
  if(at==cursor&&editing)p.rect(x,y,1,16);
  p.glyph(cp,x,y);
  if(at>=mark_begin&&at<mark_end)p.rect(x,y+16,n,1);
  x+=n;
 };
 if(window.left)draw(0x2026,SIZE_MAX);
 for(size_t at=window.begin;at<window.end;at=editor::nextUtf8Boundary(text,at))draw(display::scalar(text,at),at);
 if(cursor==window.end&&editing){if(x>=limit&&vertical){x=start;y=70;}p.rect(x,y,1,16);}
 if(window.right)draw(0x2026,SIZE_MAX);
}
void selected(const Paint& p,DeviceIme& ime,bool vertical){
 const auto text=ime.selected();
 if(text.empty()){if(!vertical){p.text("候选",8,296);p.text(ime.view()&&ime.view()->at_end?"已到末尾":ime.expanded()?"后续":"↓展开",228,296,64);}return;}
 const int width=vertical?112:240;const auto w=viewport(text,text.size(),width);
 int x=vertical?8:48;if(!vertical)p.text("已选",8,296);
 if(w.left){p.glyph(0x2026,x,vertical?338:296);x+=16;}
 p.text(text.substr(w.begin,w.end-w.begin),x,vertical?338:296,width-(w.left?16:0));
}
bool renderable(const VisibleItem& item){
 if(!item.bytes||item.bytes>64)return false;
 return ui16::canRender({item.text.data(),item.bytes});
}
bool whole(const Paint& p,const VisibleItem& item,bool vertical){
 const std::string_view text(item.text.data(),item.bytes);
 if(vertical){p.rect(8,26,82,20);p.text("1 全词",12,28,74,false,true);p.box(8,46,82,312);}
 else{p.text("1 完整候选",8,272);p.box(8,294,284,84);}
 int line=0,used=0;const int capacity=vertical?308:272,maxlines=vertical?3:4;
 for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
  const auto cp=display::scalar(text,at);const int n=font.glyph(cp).advance;
  if(used+n>capacity){used=0;++line;}if(line>=maxlines)return false;
  p.glyph(cp,vertical?72-line*28:14+used,vertical?48+used:300+line*20,vertical);used+=n;
 }return true;
}
const char* inputState(const DeviceIme& ime){
 using C=hanime::CompositionStatus;
 if(ime.receipt().status==C::UnknownOutcome)return "提交结果未知";
 if(ime.receipt().status==C::RetryableNoEffect)return "提交未完成";
 if(!ime.window())return "正在取词";
 switch(ime.window()->state()){
 case WindowState::NoResults:return "无候选";
 case WindowState::Empty:return ime.rawEditing()?"编码编辑":"正在取词";
 case WindowState::Loading:case WindowState::Busy:return "正在取词";
 case WindowState::OutputError:return "候选显示失败";
 case WindowState::Error:case WindowState::Unresolved:case WindowState::Limited:case WindowState::Capacity:return "输入法暂不可用";
 default:return ime.rawEditing()?"编码编辑":"正在取词";
 }
}
}
bool wholeCandidate(std::string_view text,bool vertical,const PaintPort& port) noexcept {
 if(!port.pixel||text.empty()||text.size()>64)return false;
 ime_device::VisibleItem item;item.bytes=uint16_t(text.size());std::copy(text.begin(),text.end(),item.text.begin());
 if(!renderable(item))return false;
 const Paint p{port};p.rect(0,vertical?22:266,vertical?94:300,vertical?336:114,false);
 return whole(p,item,vertical);
}
Receipt paint(App& app,ime_device::DeviceIme* ime,const PaintPort& port) noexcept {
 Receipt receipt;if(!app.findOpen()||!port.pixel)return receipt;
 const Paint p{port};const bool vertical=!app.horizontalActive(),active=ime&&ime->chinese()&&ime->active();
 const auto q=app.find.view();receipt.query=app.queryStamp();char caption[80];status(caption,sizeof(caption),q,vertical);
 // Find owns its header and reserved rail; the existing body renderer owns all body pixels.
 p.rect(0,0,300,vertical?20:32,false);
 p.text(app.currentName,vertical?std::max(8,292-int(std::min(284U,extent(app.currentName)))):8,vertical?3:8,284);
 if(vertical){p.rect(0,22,124,356,false);p.rect(123,22,1,356);p.verticalLabel("查找",100,27);p.text(caption,8,360,76);p.text(mode(ime),88,360,32);}
 else{p.rect(0,208,300,192,false);p.rect(0,208,300,1);p.text("查找",8,214);p.text(caption,292-int(extent(caption)),214);}
 query(p,q,vertical,active);
 bool full=false,failed=false,raw_long=false;
 const auto* view=ime?ime->view():nullptr;
 const bool ready=active&&ime->candidateAvailable()&&ime->window()&&
   (ime->window()->state()==WindowState::Ready||ime->window()->state()==WindowState::Boundary);
 if(ready&&view&&view->count&&view->row>=view->rows[0].index&&view->row<view->rows[0].index+view->count){
  const auto& row=view->rows[view->row-view->rows[0].index];
  if(view->col<row.count){
   const auto& item=row.items[view->col];
   raw_long=ime->rawEditing()&&ime->window()->focusedNeedsWholeText();
   full=!raw_long&&ime->window()->focusedNeedsWholeText();
   for(size_t r=0;r<view->count;++r)for(size_t c=0;c<view->rows[r].count;++c)failed|=!renderable(view->rows[r].items[c]);
   if(!failed&&full)failed=row.count!=1||!whole(p,item,vertical);
   if(!failed&&!full&&!raw_long){
    raw(p,*ime,vertical);selected(p,*ime,vertical);
    for(size_t r=0;r<view->count;++r){const auto& group=view->rows[r];int used=0;
     for(size_t c=0;c<group.count;++c){const auto& word=group.items[c];const auto text=std::string_view(word.text.data(),word.bytes);
      const bool current=group.index==view->row,focused=current&&c==view->col&&!ime->rawEditing();
      const int x=vertical?72-int(r)*28:8+used,y=vertical?110+used:318+int(r)*22;
      const int size=vertical?20+int(extent(text)):24+int(extent(text));
      if(focused)p.rect(x,y,vertical?16:size-12,vertical?size-4:16);
      if(current)p.glyph('1'+c,x,y,false,focused);
      const int capacity=vertical?196:272;const bool clipped=int(extent(text))>capacity;
      const auto painted=p.text(text,vertical?x:x+12,vertical?y+16:y,capacity-(clipped?16:0),vertical,focused);
      if(clipped)p.glyph(0x2026,vertical?x:x+12+painted,vertical?y+16+painted:y,vertical);
      used+=size;
     }
    }
   }
   receipt.candidates=!failed&&!raw_long;
  }
 }
 if(active&&!receipt.candidates){
  raw(p,*ime,vertical);selected(p,*ime,vertical);
  const auto* message=failed?"候选显示失败":raw_long?"长词待查看":inputState(*ime);
  if(vertical)p.verticalLabel(message,72,110);else p.text(message,8,318);
 }else if(!active){
  if(vertical){
   p.verticalLabel(q.match.valid?"已找到":q.query.empty()?"请输入内容":q.status==hanime::find::Status::NotFound?"未找到":"查找中",72,48);
   constexpr int hintTop=114;
   // Align the complete hints, including Shift+. With the pinned UI16 font,
   // 回 has 2 px of top ink inset; rotated S has 1 px, so compensate 1 px.
   p.verticalLabel("回车下一个",44,hintTop-1);
   const int shift=p.text("Shift+",16,hintTop,58,true);
   p.verticalLabel("回车上一个",16,hintTop+shift+4);
   p.text(q.case_sensitive?"区分大小写":"忽略大小写",8,338,112);
  }
  else{p.text(q.case_sensitive?"区分大小写":"忽略大小写",8,272);p.text("Ctrl+I",174,272);p.text("回车 下一个",8,318);p.text("Shift+回车 上一个",8,342);}
 }
 if(ime&&ime->inputUnavailable()){if(vertical)p.verticalLabel("输入法暂不可用",44,110);else p.text("输入法暂不可用",8,296);}
 if(!vertical)p.text(mode(ime),260,272,32);
 if(vertical&&full){status(caption,sizeof(caption),q,true);p.text(caption,8,360,76);p.text(mode(ime),88,360,32);}
 p.rect(0,380,300,20,false);
 if(q.match.valid&&q.wrapped&&!active)p.text("已回绕",8,384);
 else p.text(active?(full?"空格/回车选词  Esc取消组字":"数字选词  Esc取消组字"):"Esc退出  Ctrl+A全选",8,384);
 if(ime&&ime->composition()&&view){const auto s=ime->composition()->snapshot();receipt.input=s.input_id;receipt.revision=s.revision;receipt.generation=view->generation;receipt.row=view->row;}
 receipt.complete=true;return receipt;
}
uint32_t publish(App& app,ime_device::DeviceIme* ime,const Receipt& r) noexcept {
 const auto q=app.queryStamp();
 if(!r.complete||!app.findOpen()||q.focus!=r.query.focus||q.session!=r.query.session||q.revision!=r.query.revision||q.cursor!=r.query.cursor||q.selected!=r.query.selected||q.selection!=r.query.selection)return 0;
 if(!ime||!ime->window())return 0;
 const auto* view=ime->view();if(!view||view->generation!=r.generation||view->row!=r.row)return 0;
 if(ime->composition()){const auto s=ime->composition()->snapshot();if(s.input_id!=r.input||s.revision!=r.revision)return 0;}
 else if(r.input||r.revision)return 0;
 if(!r.candidates){ime->window()->frameHidden(r.generation);return 0;}
 if(!ime->candidateAvailable())return 0;
 return ime->framePresented();
}
}
