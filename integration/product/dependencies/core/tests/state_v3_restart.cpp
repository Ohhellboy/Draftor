#include "state_test_support.hpp"
using namespace hanime;
using namespace hanime::state_test;
int main(int argc,char** argv) {
  if (argc!=3) return 2;
  try {
    const std::string command=argv[1],root=argv[2]; std::string dir;
    if (command=="write" || command=="uncertain-write") {
      dir=directory(root,"run"); std::ofstream pointer(root+"/last-run.txt"); pointer<<dir;
      check(bool(pointer),"write test-run pointer");
    } else {std::ifstream pointer(root+"/last-run.txt"); std::getline(pointer,dir); check(!dir.empty(),"read test-run pointer");}
    if (command=="write") {
      // Caller provides a fresh per-build test directory; never reset a store.
      std::filesystem::create_directories(dir);
      Ledger l(dir); check(l.open()==StateStatus::Ready,"write open");
      for (unsigned i=1;i<=32;++i) l.write(i,i+2);
      Resources h(32); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
      auto r=commit(s,l,1); check(r.learning==LearningStatus::Pending,"real pending");
      check(l.close()==StateStatus::ClosedSaved,"new V3 close");
      std::cout<<"PASS V3 writer: Accepted -> Pending -> RecordedDirty -> HostStorageConfirmed -> ClosedSaved key1=4\n";
    } else if (command=="read") {
      Ledger l(dir); check(l.open(false)==StateStatus::Ready,"read open");
      Resources h(32,true); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
      first(s,l,"k1"); auto p=page(s);
      check(p.size()==1 && p[0].ref==32 && p[0].key==session_key(1) && p[0].effective_count==4,"restart ref/key mismatch");
      for (unsigned i=2;i<=32;++i) check(l.count(i)==i+2,"full-key suffix lost");
      check(l.close()==StateStatus::ClosedSaved,"reader close");
      std::cout<<"PASS V3 new process: reversed ref1->32, full-key count=4; 31 neighboring full keys distinct\n";
    } else if (command=="uncertain-write") {
      std::filesystem::create_directories(dir); Ledger l(dir);
      check(l.open()==StateStatus::Ready,"uncertain writer open"); l.write(1,4);
      check(l.disk.begin_write(session_key(1),5)==StateStatus::Pending,"uncertain start");
      l.files.fault={StateFileOperation::Write,0,1,1,false};
      check(l.disk.advance_write(small_budget).status==StateStatus::Pending && l.disk.info().write_pending,"no partial write");
      std::cout<<"PASS V3 interrupted writer: one real tail byte, no close-save claim\n";
    } else if (command=="uncertain-read") {
      Ledger l(dir); auto a=bytes(l.a),b=bytes(l.b);
      check(l.open(false)==StateStatus::TailUncertain && bytes(l.a)==a && bytes(l.b)==b,"new process modified unknown tail");
      auto t=l.state.begin_count(l.state.view(),session_key(1)); CountResult r;
      do {r=l.advance(t);} while(r.status==CountStatus::Pending);
      check(r.status==CountStatus::Unavailable,"uncertain tail pretended zero");
      Resources h(2); CandidateSession s(h.base.full,h.base.shared,l.state,h.base.sink,session_config); admit(s);
      auto committed=commit(s,l,1);
      check(committed.status==CommitStatus::Accepted && committed.learning==LearningStatus::PausedUnavailable,"uncertain file blocked text or guessed count");
      check(bytes(l.a)==a && bytes(l.b)==b,"BaseOnly changed unknown file");
      std::cout<<"PASS V3 new process: TailUncertain, original bytes retained, BaseOnly text accepted / learning paused\n";
    } else return 2;
  } catch (const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
