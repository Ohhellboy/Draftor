#pragma once
#include "writing_app.hpp"
#include "ui_text.hpp"
#include <cstdio>
namespace s3app::display {
// Cached metadata only: drawing never calls storage. The same labels run in host QA.
template<class Label> void documentName(const char* name,int y,const s3host::FontPort& font,Label label){
 std::string_view remaining(name);
 while(!remaining.empty()){
  const auto count=fit(remaining,font,280);if(!count)return;
  char line[97]{};std::memcpy(line,remaining.data(),count);
  label(line,8,y,false);remaining.remove_prefix(count);y+=20;
 }

}
template<class Label,class Rect> bool documentMenu(const App& app,const s3host::FontPort& font,Label label,Rect rect){
 const auto& state=app.manager.state();
 if(app.renameOpen()){
  label("RENAME  Enter: name  Esc: cancel",8,35,false);
  label("CN: confirm text, then Enter name",8,55,false);
  size_t offset=0;
  documentName(app.renameText(),85,font,[&](const char* t,int x,int y,bool){
   const std::string_view part(t);const auto length=part.size();
   const auto begin=std::clamp(app.renameSelectionBegin(),offset,offset+length)-offset;
   const auto end=std::clamp(app.renameSelectionEnd(),offset,offset+length)-offset;
   const auto segment=[&](std::string_view text,bool selected){
    char line[97]{};std::memcpy(line,text.data(),text.size());const auto width=measure(text,font);
    if(selected)rect(x,y,width,14);label(line,x,y,selected);x+=width;
   };
   segment(part.substr(0,begin),false);segment(part.substr(begin,end-begin),true);segment(part.substr(end),false);offset+=length;
  });
  int caretX=8,caretY=85;std::string_view name(app.renameText());
  for(size_t i=0;i<app.renameCursor();i=editor::nextUtf8Boundary(name,i)){
   const int width=advance(font,scalar(name,i));
   if(caretX+width>288){caretX=8;caretY+=20;}
   caretX+=width;
  }
  if(!app.renameSelected())rect(caretX,caretY,1,14);
 }else if(state.pending!=ds::PendingAction::None){
  if(state.pending==ds::PendingAction::TrashSelected){
   label("Move to trash? Can be restored.",8,40,false);
   if(state.selected<state.page.items.size())documentName(state.page.items[state.selected].name.c_str(),70,font,label);
   label(state.trash_id.hex().c_str(),8,205,false);
  }else label("Restore selected saved version?",8,62,false);
  label("Enter: confirm   Esc: cancel",8,235,false);
 }else if(state.view==ds::FileManagerView::TrashedReturn){
  label("That document is in the trash.",8,70,false);
  label("Esc: files  F2: files",8,100,false);
 }else if(app.filesOpen()){
  bool trash=state.view==ds::FileManagerView::Trash;
  label(trash?"TRASH  Enter: restore  T: files":"FILES  N:new Enter:open R:rename",8,28,false);
  label(trash?"Left/Right: page  Esc: files":"D/Del:trash T:trash L/R:page",8,45,false);
  const size_t rows=app.restoredName()?4:8;
  size_t first=state.selected>=rows?state.selected-(rows-1):0;
  if(state.page.items.empty())label((state.last_status==ds::Status::Ok||state.last_status==ds::Status::OkDegraded||state.last_status==ds::Status::FirstPage||state.last_status==ds::Status::LastPage)?"Empty list":"List unavailable - reopen to retry",8,90,false);
  for(size_t i=first;i<state.page.items.size()&&i<first+rows;++i){int y=68+(i-first)*32;bool selected=i==state.selected;
   if(selected)rect(6,y-2,288,30);
   char preview[100]{};const auto& name=state.page.items[i].name;
   size_t n=fit(name,font,276);
   if(n<name.size())n=fit(name,font,276-measure("...",font));
   std::memcpy(preview,name.data(),n);
   if(n<name.size()){preview[n++]='.';preview[n++]='.';preview[n++]='.';}
   label(preview,12,y,selected);label(state.page.items[i].id.shortHex().c_str(),12,y+14,selected);
  }
  if(app.restoredName()){label("Restored to files (not opened):",8,204,false);documentName(app.restoredName(),222,font,label);}
  char page[48];snprintf(page,sizeof(page),trash?"Page %u%s  Esc: files":"Page %u%s U:USB I:in W:Wi-Fi",unsigned(state.page.offset/ds::kMaxListPage+1),state.page.has_more?" +":"");label(page,8,330,false);
 }else if(!app.session.hasCurrent()){
  label("No active document",8,70,false);label("F2 / Cmd+O: files   N: new",8,100,false);
 }else return false;
 return true;
}
}


#include "secondary_menu.hpp"
#include "menu_name_layout.hpp"
#include "menu_name_preview.hpp"
namespace s3app::display {
inline const char* menuStatus(ds::Status status) noexcept {
 using S=ds::Status;
 switch(status){
 case S::Ok:return "操作完成";case S::OkDegraded:return "已完成，存储需检查";
 case S::Partial:return "部分完成，请检查存储";case S::NotFound:return "文档未找到";
 case S::MediaUnavailable:return "存储卡不可用";case S::SessionChanged:return "存储卡状态已变化";
 case S::IoError:return "存储读写失败";case S::Corrupt:return "数据校验失败";
 case S::UnsupportedVersion:return "数据版本不支持";case S::TooLarge:return "文档过大";
 case S::InvalidUtf8:return "文本编码无效";case S::InvalidCursor:return "光标位置无效";
 case S::InvalidName:return "名称无效";case S::Conflict:return "文档状态冲突";
 case S::Busy:return "正在处理…";case S::CompositionActive:return "请先结束中文输入";
 case S::NoCurrentDocument:return "没有当前文档";case S::NoManualVersion:return "没有手动保存版本";
 case S::ConfirmationRequired:return "等待确认";case S::StaleToken:return "状态已过期，请重试";
 case S::CapacityReached:return "文档数量已达上限";case S::InstallFailed:return "未能安装文档状态";
 case S::FirstPage:return "已是第一页";case S::LastPage:return "已是最后一页";
 case S::NameExists:return "已有同名文档";case S::ResourceExhausted:return "资源不足";
 }
 return "状态未知";
}
inline const char* menuProgress(const char* text) noexcept {
 const std::string_view s(text?text:"");
 if(s=="Reading document list...")return "正在读取文档列表…";
 if(s=="Opening / restoring...")return "正在打开或恢复文档…";
 if(s=="Applying confirmed action...")return "正在执行已确认操作…";
 if(s=="Creating document...")return "正在创建文档…";
 if(s=="Checking name / saving metadata...")return "正在检查并保存名称…";
 if(s=="Saving snapshot..."||s=="Waiting for save")return "正在等待保存完成…";
 return text; // Preserve an unknown real status, never invent success.
}
inline bool menuSurface(const App& app) noexcept {
 return app.renameOpen()||app.filesOpen()||app.statsOpen()||!app.session.hasCurrent()||
  app.manager.state().pending!=ds::PendingAction::None;
}
inline const char* renameError(const App& app) noexcept {
 const std::string_view notice(app.notice?app.notice:"");
 if(notice=="Name limit: 96 valid UTF-8 bytes")return "名称最多 96 个 UTF-8 字节";
 if(notice=="Invalid name: empty / control / slash")return "名称不能为空，不能含控制字符或 /";
 if(app.lastOperation.stage&&std::string_view(app.lastOperation.stage)=="rename"&&
    app.lastOperation.status!=ds::Status::Ok&&app.lastOperation.status!=ds::Status::Partial&&
    notice==ds::statusMessage(app.lastOperation.status))return menuStatus(app.lastOperation.status);
 return nullptr;
}
inline const char* menuNotice(const App& app) noexcept {
 const std::string_view n(app.notice?app.notice:"");
 if(n=="Turn Wi-Fi off before USB transfer")return "先关闭 Wi-Fi，再开启 USB";
 if(n=="Close USB transfer before Wi-Fi")return "先结束 USB，再开启 Wi-Fi";
 if(n=="USB unavailable: memory low")return "内存不足，USB 不可用";
 if(n=="Wi-Fi unavailable: memory low")return "内存不足，Wi-Fi 不可用";
 if(n=="Restore state unavailable; reopen list")return "移回状态不可用，请重开列表";
 if(n=="Memory low; operation failed")return "内存不足，操作失败";
 if(n=="Save failed; command cancelled")return "保存失败，命令已取消";
 if(n=="Open target changed; cancelled")return "打开目标已变化，操作已取消";
 if(n=="Command cancelled"||n=="Cancelled")return "操作已取消";
 if(n=="Name visible; storage warning")return "名称已显示，存储有警告";
 if(n=="Renamed; list read needs retry")return "已改名，请重新读取列表";
 return nullptr;
}
template<class Fill,class Aux> void menuFeedback(const App& app,Fill fill,Aux aux){
 if(const auto* text=menuNotice(app)){
  const int y=app.menuHelpOpen()?344:372;
  fill(0,y,300,24,false);aux(text,8,y,false);
 }
}
template<class Fill,class Aux> void menuHelp(const App& app,Fill fill,Aux aux){
 const bool trash=app.manager.state().view==ds::FileManagerView::Trash;
 secondary::header(trash?"回收站 · 快捷键":"文档列表 · 快捷键",fill,aux);
 // Exact Alpha2 commands. No catalog batch size is exposed in user help.
 static constexpr const char* files[]={
  "上下：选择文档","左右：翻页","Enter：打开选中文档","N：新建文档",
  "R：重命名选中文档","D / Del：请求移入回收站","T：进入回收站",
  "L：请求读取当前稿保存版","Esc / F2：返回写作","Ctrl/Cmd+S、F6：保存当前稿",
  "Ctrl/Cmd+O：回到文档列表","F7：请求读取手动保存版",
  "F8：请求读取回滚前保护版","F4：切换正文横竖排","W、F10：打开 Wi-Fi",
  "F9：打开 USB 传输","U：导出当前稿到 USB","I：从 USB 导入 TXT"};
 static constexpr const char* bin[]={
  "上下：选择文档","左右：翻页","Enter：移回列表，不打开","T / Esc：返回文档列表",
  "F2：关闭文档列表","L：请求读取当前稿保存版","Ctrl/Cmd+S、F6：保存当前稿",
  "Ctrl/Cmd+O：回到文档列表","F7：请求读取手动保存版","F8：请求读取回滚前保护版",
  "F4：切换正文横竖排","W、F10：打开 Wi-Fi","F9：打开 USB 传输",
  "U：导出当前稿到 USB","I：从 USB 导入 TXT"};
 const auto* rows=trash?bin:files;const unsigned count=trash?15:18,first=app.menuHelpPage()*12;
 for(unsigned i=first;i<count&&i<first+12;++i)aux(rows[i],8,44+int(i-first)*24,false);
 char footer[80];std::snprintf(footer,sizeof(footer),"左右翻页 %u/2  Esc：关闭帮助",app.menuHelpPage()+1);
 aux(footer,8,372,false);
}
template<class Content> void nameLines(std::string_view name,const MenuNameLines& lines,
                                      int x,int y,Content content){
 for(unsigned i=0;i<lines.count;++i)content(name.substr(lines.offsets[i],lines.offsets[i+1]-lines.offsets[i]),x,y+28*int(i),false);
}
// Product API: only cached state is read. The callbacks share the exact local
// MiSans FontPorts with Host raster evidence and the S3 framebuffer driver.
template<class Fill,class Aux,class Content>
bool documentMenu(const App& app,const s3host::FontPort& font,Fill fill,Aux aux,Content content,
                  int renameBottom=328){
 const auto& s=app.manager.state();
 if(app.menuHelpOpen()){menuHelp(app,fill,aux);return true;}
 if(app.renameOpen()){
  secondary::header("重命名",fill,aux);aux("文档名称",8,36,false);
  const std::string_view name(app.renameText());const auto lines=wrapMenuName(name,font,280);
  const auto* error=renameError(app);const int nameBottom=renameBottom-(error?24:0);
  unsigned caretLine=0;for(unsigned i=1;i<lines.count;++i)if(app.renameCursor()>=lines.offsets[i])caretLine=i;
  const unsigned visible=unsigned(std::max(1,(nameBottom-68)/28));
  const unsigned first=caretLine>=visible?caretLine-visible+1:0;
  for(unsigned i=first;i<lines.count&&i<first+visible;++i){
   const auto part=name.substr(lines.offsets[i],lines.offsets[i+1]-lines.offsets[i]);const int y=68+int(i-first)*28;
   const auto begin=std::clamp(app.renameSelectionBegin(),size_t(lines.offsets[i]),size_t(lines.offsets[i+1]))-lines.offsets[i];
   const auto end=std::clamp(app.renameSelectionEnd(),size_t(lines.offsets[i]),size_t(lines.offsets[i+1]))-lines.offsets[i];
   int x=8;content(part.substr(0,begin),x,y,false);x+=measure(part.substr(0,begin),font);
   if(end>begin){const auto selected=part.substr(begin,end-begin);const int width=measure(selected,font);fill(x,y,width,28,true);content(selected,x,y,true);x+=width;}
   content(part.substr(end),x,y,false);
  }
  const auto start=lines.count?lines.offsets[caretLine]:0;
  const int cx=8+measure(name.substr(start,app.renameCursor()-start),font);
  if(!app.renameSelected())fill(std::min(cx,292),68+int(caretLine-first)*28,1,24,true);
  if(error)aux(error,8,renameBottom-24,false);
  if(renameBottom>=328){
   aux("Enter：保存名称  Esc：取消",8,372,false);
  }
  return true;
 }
 if(s.pending!=ds::PendingAction::None){
  const bool trash=s.pending==ds::PendingAction::TrashSelected;
  secondary::header(trash?"移入回收站":"读取保存版本",fill,aux);
  aux(trash?"确认移入回收站？之后可移回。":"确认读取所选保存版本？",8,40,false);
  if(trash&&s.selected<s.page.items.size()){
   const std::string_view name=s.page.items[s.selected].name;nameLines(name,wrapMenuName(name,font,280),8,64,content);
  }
  aux("Enter：确认  Esc：取消",8,372,false);return true;
 }
 if(s.view==ds::FileManagerView::TrashedReturn){
  secondary::header("文档已在回收站",fill,aux);aux("该文档目前位于回收站。",8,76,false);
  aux("Esc / F2：文档列表",8,372,false);return true;
 }
 if(app.filesOpen()){
  const bool trash=s.view==ds::FileManagerView::Trash;
  char title[80];std::snprintf(title,sizeof(title),"%s · 第 %u 页",trash?"回收站":"文档列表",unsigned(s.page.offset/ds::kMaxListPage+1));
  secondary::header(title,fill,aux);
  if(s.page.items.empty()){
   const bool empty=s.last_status==ds::Status::Ok||s.last_status==ds::Status::OkDegraded||s.last_status==ds::Status::FirstPage||s.last_status==ds::Status::LastPage;
   aux(empty?(trash?"回收站为空":"文档列表为空"):menuStatus(s.last_status),8,76,false);
  }else{
   const auto selected=std::min(s.selected,s.page.items.size()-1);
   const auto selectedLines=wrapMenuName(s.page.items[selected].name,font);
   const auto first=app.menuViewport.place(s,selectedLines.count);int y=44;
   for(size_t i=first;i<s.page.items.size()&&i<first+9;++i){
    const bool active=i==selected;const int height=active?28*selectedLines.count:28;
    if(y+height>360)break;
    if(active){secondary::triangle(y,fill);nameLines(s.page.items[i].name,selectedLines,28,y,content);}
    else {const auto preview=menuNamePreview(s.page.items,i,font);content(preview.text(),28,y,false);}
    y+=height;
   }
  }
  // One real operation/status line; detailed commands are explicitly requested.
  const char* footer="F1 查看快捷键";
  if(app.restoredName())footer=app.lastOperation.status==ds::Status::Partial?"移回有警告 · F1 查看快捷键":"已移回，未打开 · F1 查看快捷键";
  else if(s.last_status==ds::Status::FirstPage)footer="已是第一页 · F1 查看快捷键";
  else if(s.last_status==ds::Status::LastPage)footer="已是最后一页 · F1 查看快捷键";
  else if(app.lastOperation.status!=ds::Status::Ok&&app.lastOperation.status!=ds::Status::OkDegraded&&
          app.lastOperation.status!=ds::Status::FirstPage&&app.lastOperation.status!=ds::Status::LastPage)
   footer=menuStatus(app.lastOperation.status);
  aux(footer,8,372,false);return true;
 }
 if(!app.session.hasCurrent()){
  secondary::header("Draftor",fill,aux);aux("没有当前文档",8,76,false);
  aux("F2：文档列表  N：新建",8,372,false);return true;
 }
 return false;
}
}
