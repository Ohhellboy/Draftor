// Standalone application using ONLY public headers and actual final document.
// No extension/editor implementation, count source, state file or learning queue.
#include <hanime/composition_commit.hpp>
#include <hanime/host_resource_file.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace hanime;
namespace {
void check(bool b,const char* s) {if (!b) throw std::runtime_error(s);}
StableLexemeKey key(std::string_view s) {StableLexemeKey k; check(parse_slk1(s,&k),"key literal"); return k;}
constexpr RegistryNamespaceId ns=0x48414e494d450001ULL;
constexpr WorkBudget budget{1,4096,64};
struct FinalDocument final : TextCommitSink {
  std::array<char,256> text{};
  std::size_t used{0},calls{0};
  bool reject{false};
  bool commit(std::string_view value) override {
    ++calls;
    if (reject || value.size()>text.size()-used) return false;
    if (!value.empty()) std::memcpy(text.data()+used,value.data(),value.size());
    used+=value.size(); return true; // Complete value now in the FINAL model.
  }
  std::string_view value() const {return {text.data(),used};}
};
void finish(CompositionCommitter& c,const CompositionHandle& h,const FinalDocument& doc,HostResourceMetrics& metrics) {
  const auto size=doc.used,calls=doc.calls;
  for (std::size_t i=0;i<100000;++i) {
    const auto read_calls=metrics.read_calls,bytes=metrics.requested_bytes;
    auto s=c.advance(h,budget);
    check(s.cost.read_calls<=1 && s.cost.read_bytes<=4096 && s.cost.work_items<=64,"step budget");
    check(s.cost.read_calls==metrics.read_calls-read_calls && s.cost.read_bytes==metrics.requested_bytes-bytes,"I/O accounting");
    check(doc.used==size && doc.calls==calls,"preparation changed final document");
    if (s.result.status==CompositionStatus::Ready) return;
    if (s.result.status!=CompositionStatus::Preparing) {
      std::cerr<<"prepare status="<<int(s.result.status)<<" output="<<int(s.result.output)<<" member="<<s.result.member_index<<'\n';
      throw std::runtime_error("preparation failed");
    }
  }
  throw std::runtime_error("demo step cap reached");
}
}
int main(int argc,char** argv) {
  try {
    check(argc==2,"usage: hanime_composition_consumer /path/to/frozen/order_a/shared.hns");
    HostResourceMetrics metrics;
    auto file=std::make_shared<HostResourceFile>(argv[1],4096,metrics); check(file->valid(),"file open");
    PagedSharedResource shared; std::string error;
    check(shared.open(file,ns,0,19,{4,4096},&error),"shared header");
    const auto ticket=shared.validation_cursor();
    for (std::size_t i=0;!shared.ready();++i) {
      check(i<2000000,"validation cap"); auto s=shared.advance_validation(ticket,budget);
      check(s.status==ValidationStepStatus::InProgress || s.status==ValidationStepStatus::Ready,"shared validation");
    }
    std::cout<<"PASS X1 formal R1 shared activation, validation_bytes="<<metrics.requested_bytes<<'\n';
    FinalDocument doc; CompositionCommitter core(shared,doc,{3,128,64,192}); check(core.valid(),"config");
    const auto ren=key("SLK1:8c816cf96715d449d21f7bd8ff7335bd39b9f6ddbecae776649bbf3bdbd7b0db");
    const auto machine=key("SLK1:e22fdc586736aa52e93e025cfb26125da5ee4e4fd815bc35f9bb094a714e3d8a");
    // Frozen order_a references. The core rereads and verifies EACH full key.
    std::array<CompositionMember,3> input{{{133129,ren,{0,3},{}},
        {214382,machine,{3,7},{}},{133129,ren,{7,10},{}}}};
    CompositionOrigin origin{1,1,{0,7}};
    const auto start=metrics.requested_bytes;
    auto first=core.prepare(shared.activation_handle(),origin,input.data(),2);
    check(first.status==CompositionStatus::Preparing && metrics.requested_bytes==start,"capture read");
    finish(core,first.transaction,doc,metrics);
    core.prepared(first.transaction,[](const auto& v) {check(v.text=="人机器" && v.size==2,"two-word preview");});
    auto accepted=core.confirm(first.transaction);
    check(accepted.status==CompositionStatus::Accepted && accepted.learning==CompositionLearning::DisabledByConfiguration && doc.value()=="人机器","real final accept");
    check(core.confirm(first.transaction).status==CompositionStatus::Accepted && doc.calls==1,"duplicate call");
    std::cout<<"PASS X2 two-word atomic/Disabled, prepared+confirm_bytes="<<metrics.requested_bytes-start<<'\n';
    origin={2,1,{0,10}}; input[1].output={"relation:R004",OutputInvocation::Profile};
    const auto before=metrics.requested_bytes;
    auto second=core.prepare(shared.activation_handle(),origin,input.data(),3);
    finish(core,second.transaction,doc,metrics);
    core.prepared(second.transaction,[](const auto& v) {
      check(v.text=="人機器人" && v.members[0].selected.expected_key==v.members[2].selected.expected_key,"generic form/repeated identity");
    });
    doc.reject=true;
    auto rejected=core.confirm(second.transaction);
    check(rejected.status==CompositionStatus::RetryableNoEffect && doc.value()=="人机器","reject mutated document");
    check(core.confirm(second.transaction).status==CompositionStatus::RetryableNoEffect && doc.calls==2,"auto retry");
    check(core.retry(rejected.retry).status==CompositionStatus::Preparing,"retry permit");
    finish(core,second.transaction,doc,metrics); doc.reject=false;
    check(core.confirm(second.transaction).status==CompositionStatus::Accepted && doc.value()=="人机器人機器人" && doc.calls==3,"three-word accepted");
    check(first.transaction.result().origin.input_revision==1 && second.transaction.result().origin.input_revision==2,"origin receipt");
    std::cout<<"PASS X3 three-word/repeated identity/generic output/retry, including_retry_bytes="<<metrics.requested_bytes-before<<'\n';
    const auto shi1=key("SLK1:3da586e699dd499839e5febcc599199fdf81873e7d271eee13e3bf4ec3d68913");
    const auto shi2=key("SLK1:52a02b7d8c3e37b29d9eb66bdceb4220f55071ba324cfe786d97f3d2504e3656");
    input[0]={58562,shi1,{0,2},{"m6-traditional",OutputInvocation::Profile}};
    input[1]={78423,shi2,{2,4},{"m6-traditional",OutputInvocation::Profile}};
    // The frozen m6 profile is explicit-only. First prove the denied request
    // cannot insert a prefix, then request the already-permitted invocation.
    auto denied=core.prepare(shared.activation_handle(),{3,1,{0,4}},input.data(),2);
    const auto old_calls=doc.calls,old_size=doc.used;
    for (std::size_t i=0;denied.transaction.result().status==CompositionStatus::Preparing;++i) {
      check(i<100000,"denied step cap"); core.advance(denied.transaction,budget);
    }
    check(denied.transaction.result().status==CompositionStatus::OutputRestricted &&
      denied.transaction.result().output==OutputStatus::InvocationDenied && doc.calls==old_calls && doc.used==old_size,"profile permission bypass");
    input[0].output.invocation=input[1].output.invocation=OutputInvocation::Explicit;
    auto same=core.prepare(shared.activation_handle(),{3,2,{0,4}},input.data(),2); finish(core,same.transaction,doc,metrics);
    core.prepared(same.transaction,[](const auto& v) {check(v.text=="時時" && v.members[0].selected.expected_key!=v.members[1].selected.expected_key,"same text identities");});
    check(core.confirm(same.transaction).status==CompositionStatus::Accepted,"same text accept");
    std::cout<<"PASS X4 same text distinct identities, no learning owner/file/queue\n";
    const auto memory=core.memory_usage();
    core.close();
    check(same.transaction.result().status==CompositionStatus::Accepted,"close lost accepted receipt");
    std::cout<<"PASS X5 explicit close, document_bytes="<<doc.used<<" sink_calls="<<doc.calls
      <<" impl="<<memory.impl_bytes<<" member_pool="<<memory.members_bytes<<" names="<<memory.request_names_bytes
      <<" scratch="<<memory.resolver_name_bytes+memory.member_text_bytes<<" commit_pool="<<memory.total_text_bytes
      <<" simultaneous_resource_files="<<metrics.peak_open_files<<'\n';
    return 0;
  } catch (const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}
}
