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
  if(app.renameSelected()){
   documentName(app.renameText(),85,font,[&](const char* t,int x,int y,bool){
    const int width=measure(t,font);
    rect(x,y,width,14);label(t,x,y,true);
   });
  }else documentName(app.renameText(),85,font,label);
  int caretX=8,caretY=85;std::string_view name(app.renameText());
  for(size_t i=0;i<app.renameCursor();i=editor::nextUtf8Boundary(name,i)){
   const int width=advance(font,scalar(name,i));
   if(caretX+width>288){caretX=8;caretY+=20;}
   caretX+=width;
  }
  rect(caretX,caretY,1,14);
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
