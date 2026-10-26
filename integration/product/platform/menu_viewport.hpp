#pragma once
#include "src/hanime_document/file_manager_model.hpp"
#include <algorithm>
namespace s3app {
// Presentation state only. The 16-item catalog page and selected ID remain
// owned by FileManagerModel; an anchor is validated against its real ID.
struct MenuViewport {
 size_t first=0,offset=0;
 hanime::documents::DocumentId anchor{};
 hanime::documents::FileManagerView view=hanime::documents::FileManagerView::Closed;
 bool valid=false;
 size_t place(const hanime::documents::FileManagerState& s,unsigned selectedLines) noexcept {
  const auto n=s.page.items.size();if(!n){valid=false;first=0;return 0;}
  const auto selected=std::min(s.selected,n-1);
  if(!valid||view!=s.view||offset!=s.page.offset||first>=n||s.page.items[first].id!=anchor)
   first=selected>=9?selected-8:0;
  first=std::min(first,selected);
  if(selected-first>=9)first=selected-8;
  const unsigned height=std::max(1u,selectedLines)*28;
  while(first<selected&&(selected-first)*28+height>316)++first;
  offset=s.page.offset;view=s.view;anchor=s.page.items[first].id;valid=true;
  return first;
 }
};
}
