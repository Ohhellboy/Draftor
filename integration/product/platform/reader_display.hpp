#pragma once
#include "find_types.hpp"
#include "hanime/text_stats.hpp"
#include "font_port.hpp"
#include "ui_text.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <cstdio>

// Tiny display helpers shared by the real screen and the host raster check.
// Labels are English because this candidate keeps the existing Latin font.
namespace s3app::display {
constexpr int findTop=278;
constexpr int bodyHeight=244; // body starts at 34, ends before the Find bar
inline const char* findStatus(const hanime::find::ViewModel& v) noexcept {
 using S=hanime::find::Status;
 if(v.query.empty()&&v.open)return "Empty query";
 switch(v.status){
 case S::Searching:return "Searching...";
 case S::Match:return v.wrapped?"Match (wrapped)":"Match";
 case S::NotFound:return "No match";
 case S::QueryTooLong:return "Query limit: 256 bytes";
 case S::CancelledOrStale:return "Changed; press Enter";
 case S::AdapterFailure:return "Search error";
 default:return "Enter: next  Shift+Enter: previous";
 }
}
// drawText(text,x,y), drawGlyph(cp,x,y), drawCaret(x,y,h).
// Query scrolling is bounded by the 256-byte query, never by body length.
template<class Text,class Glyph,class Caret>
void findBar(const hanime::find::ViewModel& v,const s3host::FontPort& font,
             Text text,Glyph glyph,Caret caret) {
 text("FIND",8,findTop);
 const size_t cursor=std::min(v.query_cursor,v.query.size());
 size_t first=cursor;int leftWidth=0;
 // UTF-8 scalar decoder for the already validated bounded query.
 auto scalar=[&](size_t at){
  auto b=static_cast<unsigned char>(v.query[at]);uint32_t cp=b;
  size_t n=editor::nextUtf8Boundary(v.query,at)-at;
  if(n>1){cp=b&((1u<<(7-n))-1);for(size_t i=1;i<n;++i)cp=(cp<<6)|(static_cast<unsigned char>(v.query[at+i])&63);}
  return cp;
 };
 while(first>0){
  size_t prev=editor::previousUtf8Boundary(v.query,first);
  int width=std::max(1,int(font.glyph(scalar(prev)).advance));
  if(leftWidth+width>236)break;
  leftWidth+=width;first=prev;
 }
 int x=48;
 for(size_t at=first;at<v.query.size();){
  auto cp=scalar(at);int width=std::max(1,int(font.glyph(cp).advance));
  if(x+width>292)break;
  glyph(cp,x,findTop);x+=width;at=editor::nextUtf8Boundary(v.query,at);
 }
 caret(48+leftWidth,findTop,14);
 text(findStatus(v),8,findTop+19);
 text(v.case_sensitive?"Case: SENSITIVE    Ctrl+I: toggle":"Case: IGNORE A-Z   Ctrl+I: toggle",8,findTop+38);
 text("Esc: close",8,findTop+55);
}
template<class Text>
void statistics(const hanime::text_stats::Report& report,Text text) {
 using S=hanime::text_stats::State;
 text("DOCUMENT STATISTICS",8,48);
 text("F3 / Esc: close",8,70);
 if(!report.complete()){
  const char* state=report.state==S::Counting?"Counting current text...":
                    report.state==S::InvalidUtf8?"Invalid UTF-8":
                    report.state==S::InvalidSource?"Source changed unexpectedly":"Waiting for document...";
  text(state,8,112);char line[64];
  std::snprintf(line,sizeof(line),"Scanned %u / %u bytes",unsigned(report.scanned_bytes),unsigned(report.total_bytes));text(line,8,140);
  return; // never render partial/old counts as final numbers
 }
 text("Current text (includes unsaved edits)",8,96);
 const auto& c=report.counts;
 const char* labels[]={"Characters (with spaces)","Characters (no spaces)","English / Latin words","Han characters","Number tokens","Mixed words","UTF-8 bytes"};
 const uint64_t values[]={c.characters,c.characters_no_space,c.latin_words,c.han_characters,c.number_tokens,c.mixed_words(),report.total_bytes};
 for(unsigned i=0;i<7;++i){char line[80];std::snprintf(line,sizeof(line),"%s: %llu",labels[i],static_cast<unsigned long long>(values[i]));text(line,8,126+29*i);}
}
}
