// Reuse the build212 real App/EditorSink harness; its synthetic cases still
// execute independently. Only this lane opens the new fixed-source resource.
#define main original_wubi_product_main
#include "wubi_product.cpp"
#undef main
#include "hanime/host_resource_file.hpp"
#include <map>
#include <set>
#include <sstream>
extern std::size_t phase2c_new_calls;
extern bool phase2c_forbid_new;

namespace {
struct Sample { std::string code; unsigned count=0; bool complete=false,safe=false; };
std::map<std::string,Sample> samples;
std::map<std::string,std::map<hanime::StableLexemeKey,hanime::RuntimeLexemeRef>> identities;
std::ofstream cost;
void loadExpectations() {
    std::ifstream input(fs::path(REAL_EVIDENCE)/"real_samples.tsv");
    std::string label;Sample s;
    while(input>>label>>s.code>>s.count>>s.complete>>s.safe) samples[label]=s;
    CHECK(samples.size()==11);
    std::ifstream refs(fs::path(REAL_EVIDENCE)/"real_sample_identities.tsv");
    std::string code,key;hanime::RuntimeLexemeRef ref;
    while(refs>>code>>key>>ref) {
        hanime::StableLexemeKey k; CHECK(hanime::parse_slk1(key,&k)); identities[code][k]=ref;
    }
}
struct RealRig:Rig {
    explicit RealRig(unsigned id,ReleasePins pins=productionPins()):Rig(REAL_RESOURCES,id,pins) {
        resources.start(R::DirectExact);cn();CHECK(resources.ready());
    }
    void clear(){tap(0x29);CHECK(ime.rawEditing()&&!ime.raw().empty());tap(0x29);settle();CHECK(ime.raw().empty());}
};
struct Cost {
    IoCounters io=metrics.io();
    std::size_t allocations=phase2c_new_calls;
    uint64_t work=0;
    Cost(){for(const auto& phase:metrics.phases)work+=phase.work;}
    void record(const char* name) const {
        const auto delta=metrics.io()-io;uint64_t total=0;
        for(const auto& phase:metrics.phases)total+=phase.work;
        cost<<"{\"path\":\""<<name<<"\",\"logical_reads\":"<<delta.requests
            <<",\"requested_bytes\":"<<delta.requested_bytes<<",\"work\":"<<total-work
            <<",\"posix_reads\":"<<delta.reads<<",\"posix_bytes\":"<<delta.bytes
            <<",\"new_calls_with_host_scaffolding\":"<<phase2c_new_calls-allocations<<"}\n";
    }
};
void identity(const std::string& code,const VisibleItem& item) {
    CHECK(identities.at(code).count(item.key));CHECK(identities.at(code).at(item.key)==item.ref);
}
void metadata() {
    auto pins=productionPins();const auto* m=pins.direct->auto_safe;
    CHECK(m && m->bytes==48829);
    std::ifstream in(fs::path(REAL_EVIDENCE)/"auto_safe_expectations.tsv");
    std::string code;bool expected;unsigned rows=0,allowed=0;
    while(in>>code>>expected){
        CHECK(wubiAutoSafe(code,m,pins.shared,pins.direct->scheme,pins.direct->order)==expected);
        ++rows;allowed+=expected;
    }
    CHECK(rows==99054 && allowed==48756);
    CHECK(!wubiAutoSafe("zzzz",m,pins.shared,pins.direct->scheme,pins.direct->order));
    CHECK(!wubiAutoSafe("ltifa",m,pins.shared,pins.direct->scheme,pins.direct->order));
    // Every pin field participates: mutate each separately, keep bytes valid.
    auto checkBad=[&](const hanime::ResourceReleaseSpec& bad){
        CHECK(!wubiAutoSafe(samples.at("unique").code,m,pins.shared,&bad,pins.direct->order));
    };
    const auto original=*pins.direct->scheme;
#define BAD_FIELD(field) {auto bad=original;++bad.field;checkBad(bad);}
    BAD_FIELD(rule_version);BAD_FIELD(resource_format);BAD_FIELD(key_format);
    {auto bad=original;bad.role=hanime::ResourceReleaseRole::Shared;checkBad(bad);}
    BAD_FIELD(registry_namespace);BAD_FIELD(shared_bundle_id);BAD_FIELD(file_bytes);
    BAD_FIELD(file_sha256[0]);BAD_FIELD(merkle_root[0]);BAD_FIELD(required_shared_sha256[0]);
    BAD_FIELD(runtime_ref_count);BAD_FIELD(text_capacity);BAD_FIELD(name_capacity);BAD_FIELD(scheme_id[0]);
    BAD_FIELD(bootstrap_root_offset);BAD_FIELD(bootstrap_root_bytes);BAD_FIELD(required_scheme_sha256[0]);
    BAD_FIELD(ranking_rule_version);BAD_FIELD(verification_format);
#undef BAD_FIELD
    // Constant lookup must not allocate or touch TF, including long repeats.
    Cost before;phase2c_forbid_new=true;
    for(unsigned n=0;n<100000;++n)CHECK(wubiAutoSafe("ltif",m,pins.shared,pins.direct->scheme,pins.direct->order));
    phase2c_forbid_new=false;
    CHECK(metrics.requests==before.io.requests && metrics.reads==before.io.reads && phase2c_new_calls==before.allocations);
    before.record("100000_auto_safe_lookups");
    std::cout<<"PASS 99054 source groups / 48756 four-code auto-safe; 19 pin-field mismatches; zero lookup allocation/I/O\n";
}
void realBehavior() {
    RealRig r(401);
    for(const auto* label:{"one","two","three","v","vv"}) {
        const auto& s=samples.at(label);Cost before;r.type(s.code);r.settle();before.record(label);
        CHECK(r.ime.raw()==s.code && r.ime.candidateAvailable() && r.app.document.text().empty());
        identity(s.code,r.ime.view()->rows[0].items[0]);r.clear();
    }
    for(const auto* label:{"collision","incomplete_single"}) {
        const auto& s=samples.at(label);Cost before;r.type(s.code);r.settle();before.record(label);
        CHECK(r.ime.raw()==s.code && r.app.document.text().empty());
        CHECK(r.ime.composition()->page().size==s.count && !r.ime.composition()->page().has_more);
        CHECK(!r.resources.autoSafe(s.code));
        for(unsigned i=0;i<100;++i)r.step();
        CHECK(r.app.document.text().empty());r.clear();
    }
    const auto& absent=samples.at("no_result");r.type(absent.code);r.settle();
    CHECK(r.ime.raw()==absent.code && r.ime.window()->state()==WindowState::NoResults);
    r.tap(0x2a);r.settle();CHECK(r.ime.raw()==samples.at("prefix").code && r.ime.candidateAvailable());r.clear();
    const auto& deep=samples.at("deep");Cost first;r.type(deep.code);r.settle();first.record("deep_first_page");
    const auto first_item=r.ime.view()->rows[0].items[0];identity(deep.code,first_item);
    const auto stamp=r.ime.composition()->raw_input().stamp;
    r.type("a");r.type("z");CHECK(r.ime.raw()==deep.code && r.ime.composition()->input_is_current(stamp));
    const auto depth=unsigned((deep.count-1)/6);CHECK(depth>=1);
    Cost forward;r.ime.window()->request(depth,0,true);r.settle();forward.record("deep_last_page");
    CHECK(r.ime.view()->row==depth);
    // A two-row viewport may retain page zero while focus is on page one.
    // Locate the requested logical row, not the first visible row.
    const CandidateRow* last=nullptr;
    for(unsigned i=0;i<r.ime.view()->count;++i)if(r.ime.view()->rows[i].index==depth)last=&r.ime.view()->rows[i];
    CHECK(last && last->count);identity(deep.code,last->items[0]);
    CHECK(last->items[0].key!=first_item.key);
    Cost backward;r.ime.window()->request(0,0,false);r.settle();backward.record("deep_backward_page");
    CHECK(r.ime.view()->rows[0].items[0].key==first_item.key && r.ime.view()->rows[0].items[0].ref==first_item.ref);r.clear();
    // Space and digit go through the same real selection and EditorSink path.
    for(const auto* label:{"one","collision","incomplete_single"}) {
        const auto& s=samples.at(label);r.type(s.code);r.settle();
        const unsigned slot=std::string_view(label)=="collision"?1:0;
        const auto item=r.ime.view()->rows[0].items[slot];identity(s.code,item);
        const std::string before(r.app.document.text());const auto undo=r.app.editor.undoCount();
        r.tap(slot?31:0x2c);r.settle();
        CHECK(r.ime.raw().empty() && r.ime.receipt().status==CS::Accepted && r.app.editor.undoCount()==undo+1);
        CHECK(std::string(r.app.document.text())==before+std::string(item.text.data(),item.bytes));
    }
    const auto revision=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
    Cost automatic;r.type(samples.at("unique").code);r.settle();automatic.record("complete_unique_auto_path");
    CHECK(r.ime.raw().empty() && r.ime.receipt().status==CS::Accepted);
    CHECK(r.app.editor.undoCount()==undo+1 && r.app.document.textRevision()==revision+1);
    for(unsigned n=0;n<100;++n)r.step();
    CHECK(r.app.document.textRevision()==revision+1 && r.app.editor.undoCount()==undo+1);
    CHECK(r.ime.composition()->count_begin_calls()==0);
    std::cout<<"PASS real lengths/v/vv/z/fifth/no-result/no-prefix; first/deep/backward stable key+ref; Space/digit; complete auto exactly once; collision/incomplete no auto\n";
}
void fallback() {
    for(unsigned mode=0;mode<3;++mode) {
        auto pins=productionPins();auto descriptor=*pins.direct;auto m=*descriptor.auto_safe;
        auto stale=*descriptor.scheme;
        if(mode==0)descriptor.auto_safe=nullptr;
        if(mode==1){++stale.file_sha256[0];m.scheme=&stale;descriptor.auto_safe=&m;}
        if(mode==2){--m.bytes;descriptor.auto_safe=&m;}
        pins.direct=&descriptor;RealRig r(410+mode,pins);
        r.type(samples.at("unique").code);r.settle();
        CHECK(r.ime.candidateAvailable() && r.ime.raw()==samples.at("unique").code && r.app.document.text().empty());
        CHECK(!r.resources.autoSafe(r.ime.raw()));r.tap(0x2c);r.settle();
        CHECK(r.ime.receipt().status==CS::Accepted && !r.app.document.text().empty());
    }
    std::cout<<"PASS missing/stale/truncated metadata no auto, identity-valid manual selection retained\n";
}
void staleRetryUnknown() {
    {
        RealRig r(420);r.type(samples.at("one").code);r.settle();
        const auto old_selection=r.ime.composition()->selection(0);auto old_event=r.freshEvent(0x2c,true);
        r.type("b");r.settle();CHECK(r.ime.composition()->select(old_selection)==S::Stale);
        r.send(old_event);r.send(r.freshEvent(0x2c,false));r.settle();CHECK(r.app.document.text().empty());
        auto old_mount=r.freshEvent(0x2c,true);++r.resources.mount()->epoch;r.step();
        CHECK(r.resources.state()==ResourceState::Error);r.send(old_mount);r.send(old_mount);CHECK(r.app.document.text().empty());
    }
    {
        RealRig r(421);EditorSink fill(r.app);
        while(r.app.document.text().size()<65535){CHECK(fill.bind(0,0));CHECK(fill.commit(std::string(std::min(size_t(8192),65535-r.app.document.text().size()),'q')));}
        r.type(samples.at("unique").code);r.settle();CHECK(r.ime.receipt().status==CS::RetryableNoEffect);
        const auto serial=r.ime.attemptSerial();for(unsigned n=0;n<100;++n)r.step();
        CHECK(r.ime.attemptSerial()==serial && r.app.document.text().size()==65535);
        r.tap(0x2c);r.settle();CHECK(r.ime.attemptSerial()>serial && r.ime.receipt().status==CS::RetryableNoEffect);
        CHECK(r.app.document.text().size()==65535);r.clear();
    }
    {
        RealRig r(422);r.type(samples.at("one").code);r.settle();auto* c=r.ime.composition();
        const auto item=r.ime.view()->rows[0].items[0];CHECK(c->select(c->selection(0))==S::Working && finish(*c)==S::Ready);
        EditorSink sink(r.app);CHECK(sink.bind(c->snapshot().input_id,c->snapshot().revision));
        struct After:hanime::TextCommitSink {
            EditorSink& sink;unsigned calls=0;explicit After(EditorSink& s):sink(s){}
            bool commit(std::string_view t)override{++calls;CHECK(sink.commit(t));throw std::runtime_error("after actual real-resource commit");}
        } wrapped(sink);
        hanime_extension::Submission submission(*c,r.resources.shared,wrapped,submissionConfig(compositionConfig(R::DirectExact)));
        auto result=submission.prepare();const auto tx=result.transaction;
        for(unsigned n=0;result.core.status==CS::Preparing;++n){CHECK(n<1000000);result=submission.advance(tx,Resources::Budget).result;}
        CHECK(result.core.status==CS::Ready && r.app.document.text().empty());
        CHECK(submission.confirm(tx).core.status==CS::UnknownOutcome);
        CHECK(std::string_view(r.app.document.text())==std::string_view(item.text.data(),item.bytes));
        CHECK(submission.retry(tx).core.status==CS::UnknownOutcome && submission.cancel(tx).core.status==CS::UnknownOutcome && wrapped.calls==1);
        CHECK(c->cancel_input(c->snapshot().input_id)==S::Busy);
    }
    std::cout<<"PASS real stale input/event/mount; RetryableNoEffect explicit retry no automatic replay; post-accept Unknown quarantined\n";
}
void crossScheme() {
    hanime::HostResourceMetrics metrics;
    auto open=[&](const char* name){return std::make_shared<hanime::HostResourceFile>((fs::path(REAL_RESOURCES)/name).string(),128u*1024u*1024u,metrics);};
    hanime::MountedResourceSet mounted;std::string error;
    CHECK(mounted.mount(open("shared.hns"),{open("wubi86.hnm"),open("full_pinyin.hnm"),open("xiaohe.hnm")},&error));
    std::ifstream input(fs::path(REAL_EVIDENCE)/"cross_scheme.tsv");std::string wubi,key,py,xh;unsigned compared=0;
    while(input>>wubi>>key>>py>>xh){
        hanime::StableLexemeKey expected;CHECK(hanime::parse_slk1(key,&expected));hanime::RuntimeLexemeRef reference=0;
        for(const auto& pair:{std::make_pair("wubi86",wubi),std::make_pair("full_pinyin",py),std::make_pair("xiaohe",xh)}){
            bool found=false;
            for(const auto& hit:mounted.query(pair.first,pair.second)){
                hanime::RuntimeLexemeInfo info;CHECK(mounted.lexeme_info(hit.runtime_ref,&info));
                if(info.stable_key==expected){
                    if(reference)CHECK(reference==hit.runtime_ref);else reference=hit.runtime_ref;
                    if(std::string_view(pair.first)=="wubi86")CHECK(hit.scheme_weight==0);
                    found=true;
                }
            }
            CHECK(found);++compared;
        }
    }
    CHECK(compared>=9);std::cout<<"PASS cross-scheme comparisons="<<compared<<" full StableLexemeKey + RuntimeLexemeRef, no text identity join\n";
}
}
int main(){
    try {
        loadExpectations();auto path=(fs::current_path()/"wubi-real-XXXXXX").string();CHECK(mkdtemp(path.data()));fs::current_path(path);
        cost.open(fs::path(REAL_COST_OUTPUT));CHECK(cost.good());
        metadata();realBehavior();fallback();staleRetryUnknown();crossScheme();
        CHECK(metrics.resource_handles==0 && metrics.document_handles==0);
        std::cout<<"PASS WUBI86_REAL_RESOURCE_HOST_VERIFIED; device NOT_RUN\n";return 0;
    } catch(const std::exception& e){phase2c_forbid_new=false;std::cerr<<e.what()<<'\n';return 1;}
}
