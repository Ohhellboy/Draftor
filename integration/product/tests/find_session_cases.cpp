#include "find_session.hpp"
#include "find_input.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
using namespace hanime::find;
struct Port:DocumentPort {
 std::string body;SourceStamp stamp{};size_t cursor=0;MatchRange match{};
 SourceStamp sourceStamp()const noexcept override{return stamp;}
 std::string_view text()const noexcept override{return body;}
 size_t cursorByte()const noexcept override{return cursor;}
 bool beginFindSession()noexcept override{return true;}
 bool revealMatch(size_t b,size_t e)noexcept override{match={b,e,0,true};cursor=b;return true;}
 bool placeCaretAtByte(size_t n)noexcept override{cursor=n;return n<=body.size();}
 void clearMatchHighlight()noexcept override{match={};}
};
void settle(FindSession& s,Port& p,size_t budget=256){size_t n=0;do{s.step(p,budget);REQUIRE(s.lastStepWork()<=budget);REQUIRE(++n<1000000);}while(s.hasWork());}
std::vector<size_t> reference(std::string_view body,std::string_view query,bool sensitive){
 auto fold=[&](char c){return !sensitive&&c>='A'&&c<='Z'?char(c-'A'+'a'):c;};std::vector<size_t> out;
 for(size_t at=0;at<body.size();++at)if((uint8_t(body[at])&192)!=128&&at+query.size()<=body.size()){
  bool same=true;for(size_t j=0;j<query.size();++j)same&=fold(body[at+j])==fold(query[j]);if(same)out.push_back(at);
 }return out;
}
void checkNavigation(std::string body,std::string query,bool sensitive=false,size_t budget=256){
 Port p;p.body=body;FindSession s;REQUIRE(s.open(p,false));s.setCaseSensitive(sensitive);REQUIRE(s.setQuery(query)==QueryEditResult::Applied);
 const auto hits=reference(body,query,sensitive);s.start(Direction::Forward);settle(s,p,budget);
 if(hits.empty()){REQUIRE(s.view().status==Status::NotFound&&s.view().count_complete&&s.view().total_matches==0);return;}
 for(size_t i=0;i<hits.size()+1;++i){const auto v=s.view();REQUIRE(v.match.valid&&v.count_complete&&v.total_matches==hits.size());REQUIRE(v.match.begin==hits[i%hits.size()]&&v.current_index==i%hits.size()+1);s.start(Direction::Forward);settle(s,p,budget);}
 for(size_t i=0;i<hits.size()+1;++i){s.start(Direction::Backward);settle(s,p,budget);const auto v=s.view();REQUIRE(v.count_complete&&v.current_index>0&&v.current_index<=hits.size());REQUIRE(v.match.begin==hits[v.current_index-1]);}
 std::cout<<"PASS bounded count/navigation overlapping UTF8 hits="<<hits.size()<<" budget="<<budget<<'\n';
}
void partialSelection(){
 Port p;p.body="A中😀Z A中😀Z";FindSession s;REQUIRE(s.open(p,false));s.setQuery("A中😀Z");
 s.start(Direction::Forward);settle(s,p);const auto version=s.view().query_version;const auto match=s.view().match.begin;
 s.moveQueryCursorLeft();s.moveQueryCursorLeft(true);s.moveQueryCursorLeft(true);
 REQUIRE(s.view().selectionBegin()==1&&s.view().selectionEnd()==8&&s.view().query_anchor==8);
 REQUIRE(s.view().query_version==version&&s.view().match.begin==match&&s.view().total_matches==2);
 input::FindQueryCommitTarget producer(s);REQUIRE(producer.prepareCommit("文"));
 s.moveQueryCursorRight(true);s.moveQueryCursorLeft(true);REQUIRE(producer.commit("文")==input::CommitResult::Stale);
 REQUIRE(producer.prepareCommit("文")&&producer.finishCommit("文"));REQUIRE(s.view().query=="A文Z");
 s.moveQueryCursorHome();s.moveQueryCursorRight(true);s.moveQueryCursorRight(true);s.moveQueryCursorLeft(true);
 REQUIRE(s.view().selectionBegin()==0&&s.view().selectionEnd()==1);
 s.moveQueryCursorLeft(true);REQUIRE(!s.view().query_selected);s.moveQueryCursorEnd(true);
 REQUIRE(s.eraseQueryBackward()==QueryEditResult::Applied&&s.view().query.empty());
 s.setQuery("A中😀Z");s.moveQueryCursorLeft();s.moveQueryCursorLeft(true);REQUIRE(s.eraseQueryForward()==QueryEditResult::Applied&&s.view().query=="A中Z");
 s.moveQueryCursorHome();s.moveQueryCursorRight(true);s.moveQueryCursorRight();REQUIRE(!s.view().query_selected&&s.view().query_cursor==1);
 s.moveQueryCursorEnd(true);s.moveQueryCursorLeft();REQUIRE(!s.view().query_selected&&s.view().query_cursor==1);
 s.setQuery(std::string(256,'x'));s.moveQueryCursorLeft(true);
 REQUIRE(s.insertQueryUtf8("中")==QueryEditResult::CapacityExceeded&&s.view().query.size()==256&&s.view().selectionEnd()-s.view().selectionBegin()==1);
 s.moveQueryCursorLeft(true);s.moveQueryCursorLeft(true);REQUIRE(s.insertQueryUtf8("中")==QueryEditResult::Applied&&s.view().query.size()==256);
 std::cout<<"PASS partial UTF8 selection/reversal/collapse/delete/capacity/stale commit; selection does not restart search\n";
}
int main(){try{
 partialSelection();
 checkNavigation("aaaa","aa",false,1);checkNavigation("清风 aa 清风 清风","清风",false,7);checkNavigation("aAaA aa AA","aa");checkNavigation("aAaA aa AA","aa",true);checkNavigation("😀a😀 a😀","😀",true,1);checkNavigation("正文 abc","xyz");
 checkNavigation(std::string(64000,'x')+"汉a汉a汉", "汉a汉",true,256);
 Port p;p.body="abc abc";FindSession s;REQUIRE(s.open(p,false));REQUIRE(s.insertQueryUtf8("abc")==QueryEditResult::Applied);s.start(Direction::Forward);settle(s,p);
 const auto match=s.view().match;REQUIRE(s.insertQueryUtf8(std::string(254,'x'))==QueryEditResult::CapacityExceeded);REQUIRE(s.view().match.begin==match.begin&&s.view().total_matches==2&&s.view().capacity_exceeded&&s.view().query=="abc");
 s.selectQuery();REQUIRE(s.insertQueryUtf8("中😀A")==QueryEditResult::Applied);REQUIRE(s.view().query=="中😀A");REQUIRE(s.moveQueryCursorLeft()&&s.view().query_cursor==7);REQUIRE(s.eraseQueryBackward()==QueryEditResult::Applied&&s.view().query=="中A");s.moveQueryCursorHome();REQUIRE(s.eraseQueryForward()==QueryEditResult::Applied&&s.view().query=="A");
 s.selectQuery();REQUIRE(s.eraseQueryForward()==QueryEditResult::Applied&&s.view().query.empty()&&s.view().status==Status::EmptyQuery);REQUIRE(s.setQuery(std::string("\xc0\xaf",2))==QueryEditResult::InvalidUtf8);REQUIRE(s.view().query.empty());
 std::string cap;for(unsigned n=0;n<85;++n)cap+="中";cap+='a';REQUIRE(cap.size()==256);REQUIRE(s.setQuery(cap)==QueryEditResult::Applied);REQUIRE(s.insertQueryUtf8("b")==QueryEditResult::CapacityExceeded&&s.view().query==cap);s.selectQuery();REQUIRE(s.insertQueryUtf8("future 😀")==QueryEditResult::Applied);
 input::FindQueryCommitTarget producer(s);REQUIRE(producer.prepareCommit("国"));s.moveQueryCursorLeft();s.moveQueryCursorRight();REQUIRE(producer.commit("国")==input::CommitResult::Stale);REQUIRE(producer.prepareCommit("国"));s.selectQuery();REQUIRE(producer.commit("国")==input::CommitResult::Stale);
 s.setQuery("abc");s.start(Direction::Forward);s.step(p,1);++p.stamp.text_revision;REQUIRE(s.step(p)==StepResult::Stale&&!s.view().match.valid);
 s.documentSwitched(p);s.start(Direction::Forward);REQUIRE(s.step(p)==StepResult::Found);REQUIRE(s.hasWork());++p.stamp.text_revision;REQUIRE(s.step(p)==StepResult::Stale&&!s.view().count_complete);
 // Interrupt a count by repeated Enter, then replace query while counting.
 s.documentSwitched(p);s.start(Direction::Forward);s.step(p,3);s.start(Direction::Backward);settle(s,p,1);REQUIRE(s.view().match.begin==3*s.view().current_index-3+s.view().current_index-1);
 s.setQuery("missing");s.start(Direction::Forward);settle(s,p);REQUIRE(s.view().total_matches==0&&!s.view().match.valid);
 std::cout<<"PASS selection/atomic capacity/scalar edits/stale producer/revision replacement\n";
 std::cout<<"MEM FindSession="<<sizeof(FindSession)<<" ViewModel="<<sizeof(ViewModel)<<" no_match_list=1\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
