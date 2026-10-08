#include "device_ime.hpp"
#include "behavior_observer.hpp"
#include "wubi_product.hpp"
#include "loading_status.hpp"
#include "chinese_font.hpp"
#include "hanime/resource_publisher.hpp"
#include "hanime_document_sd_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace ime_device;
namespace fs = std::filesystem;
using R = hanime_extension::SchemeRules;
using S = hanime_extension::Status;
using CS = hanime::CompositionStatus;
#define CHECK(x) do {if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(0)
std::array<hanime::ResourceReleaseSpec,7> specs;
const auto synthetic_auto_bits = [] {
    std::array<uint8_t,(390625u+7u)/8u> bits{};
    // The original abcd fixture has duplicate rows but exactly one identity.
    constexpr unsigned index = ((0u*25u+1u)*25u+2u)*25u+3u;
    bits[index/8] |= uint8_t(1u << (index%8));
    return bits;
}();
const WubiAutoSafe synthetic_auto{synthetic_auto_bits.data(),synthetic_auto_bits.size(),&specs[0],&specs[3],&specs[6]};
DirectRelease direct{
    "wubi86",&specs[3],&specs[6],"wubi86.hnm","wubi86.hnv","wubi86_base.hbo","wubi86_base.hnv",&synthetic_auto
};
ReleasePins fixturePins(){
    return {
        &specs[0],&specs[1],&specs[2],&specs[4],&specs[5],&direct
    };
}
fs::path fixture(bool rawEditCodes = false){
    const auto path = fs::current_path()/"resources";
    fs::create_directories(path);
    hanime::ResourceBuildInput in;
    in.registry_namespace = Resources::Namespace;
    in.source_name = "Core-frozen Wubi integration synthetic fixture";
    in.source_version = "1";
    in.relation_basis = "artificial local test, NOT production Wubi table";
    for(unsigned i = 1;i<=56;++i){
        hanime::LexemeSourceRecord x;
        x.stable_key.bytes[31] = uint8_t(i);
        x.status = hanime::RegistryStatus::Approved;
        x.kind = hanime::LexemeKind::Word;
        x.identity_definition = "synthetic identity "+std::to_string(i);
        x.provenance = "local synthetic";
        x.base_weight = 1000-int(i);
        x.default_output_utf8 = i == 1?"一":i == 2?"二":i == 3?"三":i == 4?"四":i == 7?"五":i == 8?"六":"同";
        if(i == 2){
            hanime::OutputRelationInput relation;
            relation.form_id = "blocked";
            relation.text_utf8 = "不可";
            relation.admission = hanime::OutputAdmission::Blocked;
            x.output_relations.push_back(relation);
        }
        in.lexemes.push_back(x);
    }
    auto map = [&](const char* scheme,const char* code,unsigned key,int weight = 0){
        in.mappings.push_back({
            scheme,code,in.lexemes.at(key-1).stable_key,weight
        });
    };
    for (auto entry : {std::pair<const char*, unsigned>{"a", 1},
                       {"ab", 2}, {"abc", 3}, {"abcd", 4},
                       {"bbbb", 5}, {"bbbb", 6}, {"v", 7}, {"vv", 8},
                       {"av", 7}, {"avv", 8}}) {
        map("wubi86", entry.first, entry.second);
    }
    map("wubi86","abcd",4,1);
    // duplicate mapping must not invent a second identity
    if(rawEditCodes){
        // Separate event-edit configuration. The original fixture(false)
        // keeps exactly its original mappings and collision depths.
        for (const auto* code : {"aba", "abba", "abb"}) {
            map("wubi86",code,5);
            map("wubi86",code,6);
        }
    }
    for (unsigned i = 9; i <= 56; ++i) {
        map("wubi86", "dddd", i);
    }
    map("xiaohe","aa",4);
    map("full_pinyin","si",4);
    auto built = hanime::ResourceBuilder().build(in, {256, 256});
    CHECK(built.ok);
    auto put = [&](const std::string& name,const auto& bytes){
        std::ofstream f(path/name,std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
        CHECK(f.good());
    };
    put("shared.hns",built.resources.shared_bundle);
    for (const auto& item : built.resources.scheme_bundles) {
        put(item.first + ".hnm", item.second);
    }
    hanime::PublishVerifiedOptions options;
    options.shared_path = (path/"shared.hns").string();
    options.output_directory = (path/"verified").string();
    options.registry_namespace = Resources::Namespace;
    options.base_order = true;
    for (const auto* name : {"xiaohe", "full_pinyin", "wubi86"}) {
        options.schemes.push_back({name, (path / (std::string(name) + ".hnm")).string()});
    }
    const auto result = hanime::publish_verified_resources(options);
    CHECK(result.ok && result.resources.size() == 7);
    for(const auto& item:result.resources){
        const auto n = item.logical_name;
        const auto i = n == "shared"?0:n == "xiaohe"?1:n == "full_pinyin"?2:n == "wubi86"?3:n == "xiaohe_base"?4:n == "full_pinyin_base"?5:6;
        specs[i] = item.spec;
        fs::copy_file(path/"verified"/fs::path(item.index_file).filename(),path/fs::path(item.index_file).filename());
        if (n.find("_base") != std::string::npos) {
            fs::copy_file(path / "verified" / item.source_file, path / item.source_file);
        }
    }
    return path;
}

struct Rig {
    fs::path path;
    s3fs::Fs files;
    hanime::documents::ArduinoSdStoragePort storage;
    ChineseFont font;
    s3app::App app;
    Resources resources;
    DeviceIme ime;
    uint32_t now = 100,frame = 0;
    Rig(const fs::path& root, unsigned number, ReleasePins pins = fixturePins())
        : path(fs::current_path() / ("adapter-doc-" + std::to_string(number))),
          files(path.string()), storage(files), app(storage, {}, 1234 + number, font),
          resources(root.string(), std::make_shared<Mount>(), pins), ime(app, resources, font) {
        fs::remove_all(path);
        fs::create_directories(path);
        CHECK(font.begin());
        storage.noteMounted();
        CHECK(app.startup(now));
    }
    void send(s3hid::Event e){
        if (!ime.event(e)) {
            app.key(e);
        }
    }
    // Each call represents a fresh report in this synchronous fixture. Captured
    // events keep these identities when sent later; send() never re-stamps them.
    s3hid::Event freshEvent(uint8_t u,bool down,uint8_t mods = 0){
        s3hid::Event e{
            1,++now,u,mods,down,0,frame,ime.attemptSerial()
        };
        e.ime_context = ime.ingressContext();
        e.resource_owner = ime.resourceOwner();
        return e;
    }
    void tap(uint8_t u,uint8_t mods = 0){
        for (bool down : {true, false}) {
            send(freshEvent(u, down, mods));
        }
    }
    void type(std::string_view text){
        for (char character : text) {
            tap(uint8_t(character - 'a' + 4));
        }
    }
    void step(){
        ime.service();
        app.service(++now,false);
    }
    void settle(){
        for(size_t i = 0;i<50000000;++i){
            step();
            const auto* w = ime.window();
            if (resources.state() == ResourceState::Error) {
                throw std::runtime_error("unexpected resource error in " + path.string() + ": " + resources.error());
            }
            if(i>1 && resources.ready() && (ime.composition() || !ime.chinese()) && (!w || !w->busy()) && !ime.hasWork() && ime.receipt().status != hanime::CompositionStatus::Preparing && !app.persistence.backgroundSaveActive() && !app.commandBarrier()){
                frame = ime.framePresented();
                return;
            }
        }
        throw std::runtime_error("settle exceeded: "+path.string()+" resource="+std::to_string(unsigned(resources.state()))+" window="+std::to_string(ime.window()?unsigned(ime.window()->state()):99)+" raw="+std::string(ime.raw())+" work="+std::to_string(ime.hasWork()));
    }
    void cn(){
        tap(0x39);
        settle();
        CHECK(ime.chinese());
    }
};
struct WubiRig:Rig {
    WubiRig(const fs::path& root,unsigned id):Rig(root,id){
        resources.start(R::DirectExact);
        cn();
    }
    void zeroPlanner(){
        const auto p = ime.composition()->planning_memory_usage();
        CHECK(p.owner == 0 && p.impl == 0 && p.buffers == 0);
        CHECK(ime.composition()->count_begin_calls() == 0);
    }
};
void wubiBehavior(const fs::path& root){
    WubiRig rig(root,1);
    for (const auto* code : {"a", "ab", "abc", "v", "vv"}) {
        rig.type(code);
        rig.settle();
        CHECK(rig.ime.raw() == code && rig.ime.candidateAvailable());
        CHECK(rig.app.document.text().empty());
        CHECK(rig.ime.presentationNavigation().span_count == 0);
        rig.zeroPlanner();
        rig.tap(0x29); CHECK(rig.ime.rawEditing()&&!rig.ime.raw().empty()); rig.tap(0x29);
        rig.settle();
        CHECK(rig.ime.raw().empty());
    }
    for (const auto* code : {"a", "ab", "abc", "v", "vv"}) {
        const auto undo = rig.app.editor.undoCount();
        rig.type(code);
        rig.settle();
        const auto item = rig.ime.view()->rows[0].items[0];
        const std::string before(rig.app.document.text());
        rig.tap(0x2c);
        rig.settle();
        CHECK(rig.ime.raw().empty() && rig.ime.receipt().status == CS::Accepted);
        CHECK(std::string(rig.app.document.text()) == before+std::string(item.text.data(),item.bytes));
        CHECK(rig.app.editor.undoCount() == undo+1);
    }
    rig.type("abcd");
    CHECK(rig.ime.raw() == "abcd");
    const auto before = std::string(rig.app.document.text());
    for(unsigned i = 0;i<2;++i){
        rig.step();
        CHECK(std::string_view(rig.app.document.text()) == before);
    }
    // no early provisional uniqueness
    rig.settle();
    CHECK(std::string(rig.app.document.text()) == before+"四" && rig.ime.raw().empty());
    rig.zeroPlanner();
    const auto committed = rig.app.document.textRevision();
    for (unsigned i = 0; i < 100; ++i) {
        rig.step();
    }
    CHECK(rig.app.document.textRevision() == committed);
    rig.type("bbbb");
    rig.settle();
    CHECK(rig.ime.raw() == "bbbb" && rig.ime.view()->rows[0].count == 2);
    const auto* c = rig.ime.composition();
    const auto stamp = c->raw_input().stamp;
    const auto query = rig.ime.window()->querySerial();
    const auto generation = rig.ime.view()->generation;
    const auto first = rig.ime.view()->rows[0].items[0];
    rig.type("a");
    CHECK(c->input_is_current(stamp) && rig.ime.raw() == "bbbb" && query == rig.ime.window()->querySerial() && generation == rig.ime.view()->generation);
    // Step 3 enables the switch chord; its discard/retirement is tested
    // by wubi_step3. Keep this fixture focused on invalid-edit retention.
    rig.type("z");
    CHECK(c->input_is_current(stamp) && rig.ime.raw() == "bbbb" && query == rig.ime.window()->querySerial() && generation == rig.ime.view()->generation && first.key == rig.ime.view()->rows[0].items[0].key);
    CHECK(rig.ime.view()->rows[0].items[0].key != rig.ime.view()->rows[0].items[1].key);
    // equal text is not identity
    const auto undo = rig.app.editor.undoCount();
    rig.tap(31);
    rig.settle();
    CHECK(rig.ime.receipt().status == CS::Accepted && rig.ime.raw().empty() && rig.app.editor.undoCount() == undo+1);
    rig.type("abcc");
    rig.settle();
    CHECK(rig.ime.window()->state() == WindowState::NoResults && rig.ime.raw() == "abcc");
    // abc exists, must not shorten
    rig.tap(0x2a);
    rig.settle();
    CHECK(rig.ime.raw() == "abc" && rig.ime.candidateAvailable());
    rig.tap(0x29); CHECK(rig.ime.rawEditing()&&!rig.ime.raw().empty()); rig.tap(0x29);
    rig.settle();
    CHECK(rig.ime.raw().empty());
    rig.type("dddd");
    rig.settle();
    const auto key = rig.ime.view()->rows[0].items[0].key;
    rig.ime.window()->request(5,0,true);
    rig.settle();
    CHECK(rig.ime.view()->row == 5 && rig.ime.view()->rows[0].items[0].key != key);
    rig.ime.window()->request(0,0,false);
    rig.settle();
    CHECK(rig.ime.view()->rows[0].items[0].key == key && rig.ime.raw() == "dddd");
    rig.zeroPlanner();
    std::cout<<"PASS Wubi lengths/v/vv/z/full scene/no-prefix-fallback/paging/one-press Space+digit/auto once; planner_storage_zero=1 count_begin=0; planner-entry absence audited separately\n";
}
S finish(hanime_extension::Composition& c){
    auto s = c.status();
    const auto op = c.operation();
    for(unsigned n = 0;s == S::Working;++n){
        CHECK(n<1000000);
        s = c.tick(op,Resources::Budget).status;
    }
    return s;
}

// Public lower-level selection failure and real EditorSink Unknown boundary.
void boundaries(const fs::path& root){
    WubiRig rig(root,2);
    rig.type("ab");
    rig.settle();
    auto* c = rig.ime.composition();
    const auto selected = c->selection(0);
    CHECK(c->select(selected,{
        "blocked",hanime::OutputInvocation::Explicit
    }) == S::Working);
    CHECK(finish(*c) == S::OutputRestricted);
    CHECK(c->snapshot().member_count == 0 && rig.app.document.text().empty());
    CHECK(c->select(selected) == S::Working && finish(*c) == S::Ready);
    CHECK(c->snapshot().state == hanime_extension::CompositionState::AllSelected && rig.app.document.text().empty() && c->count_begin_calls() == 0);
    EditorSink sink(rig.app);
    CHECK(sink.bind(c->snapshot().input_id,c->snapshot().revision));
    struct ThrowAfter:hanime::TextCommitSink {
        EditorSink& real;
        unsigned calls = 0;
        explicit ThrowAfter(EditorSink& s):real(s){
        }
        bool commit(std::string_view text)override{
            ++calls;
            CHECK(real.commit(text));
            throw std::runtime_error("after actual App acceptance");
        }
    }
    wrapped(sink);
    hanime_extension::Submission submission(*c,rig.resources.shared,wrapped,submissionConfig(compositionConfig(R::DirectExact)));
    auto result = submission.prepare();
    const auto attempt = result.transaction;
    for(unsigned i = 0;result.core.status == CS::Preparing;++i){
        CHECK(i<1000000);
        result = submission.advance(attempt,Resources::Budget).result;
    }
    CHECK(result.core.status == CS::Ready && rig.app.document.text().empty());
    CHECK(submission.confirm(attempt).core.status == CS::UnknownOutcome && rig.app.document.text() == "二");
    CHECK(submission.retry(attempt).core.status == CS::UnknownOutcome && submission.cancel(attempt).core.status == CS::UnknownOutcome && wrapped.calls == 1);
    CHECK(c->cancel_input(c->snapshot().input_id) == S::Busy);
    std::cout<<"PASS member output failure recoverable; staged member/body separate; real EditorSink post-accept exception Unknown quarantines no replay\n";
}

void staleAndIdentity(const fs::path& root){
    WubiRig rig(root,3);
    rig.type("ab");
    rig.settle();
    auto* c = rig.ime.composition();
    auto old = c->selection(0);
    const auto frame = rig.frame;
    rig.type("c");
    rig.settle();
    CHECK(c->select(old) == S::Stale && rig.app.document.text().empty());
    CHECK(rig.ime.window()->choose(0,frame) == ChoiceResult::ExpiredInput || rig.ime.window()->state() == WindowState::Stale);
    rig.tap(0x29); CHECK(rig.ime.rawEditing()&&!rig.ime.raw().empty()); rig.tap(0x29);
    rig.settle();
    rig.type("bbbb");
    rig.settle();
    auto stale = rig.freshEvent(30,true);
    ++rig.resources.mount()->epoch;
    rig.step();
    CHECK(rig.resources.state() == ResourceState::Error);
    rig.send(stale);
    rig.send(stale);
    CHECK(rig.app.document.text().empty() && !rig.ime.chinese());
    for(const auto rules:{
        R::DirectExact,R::FullPinyin,R::R1Xiaohe
    }){
        Resources resources(root.string(),std::make_shared<Mount>(),fixturePins());
        resources.start(rules);
        for(unsigned i = 0;!resources.ready();++i){
            CHECK(i<1000000 && resources.state() != ResourceState::Error);
            resources.service();
        }
        hanime_extension::Composition x(resources.scheme,resources.shared,resources.policy());
        const auto code = rules == R::DirectExact?"abcd":rules == R::FullPinyin?"si":"aa";
        CHECK(x.edit_raw(0,0,code) == S::Ready);
        CHECK(x.choose_prefix(std::strlen(code)) == S::Working && finish(x) == S::PageReady);
        CHECK(x.page().size == 1 && x.page().items[0].key.bytes[31] == 4);
        hanime_extension::Composition other(resources.scheme,resources.shared,resources.policy());
        CHECK(other.edit_raw(0,0,code) == S::Ready);
        CHECK(other.select(x.selection(0)) == S::Stale);
    }
    auto missing_direct = productionPins();
    missing_direct.direct = nullptr;
    Resources absent(root.string(),std::make_shared<Mount>(),missing_direct);
    absent.start(R::DirectExact);
    CHECK(absent.state() == ResourceState::Error);
    std::cout<<"PASS stale input/frame/owner/mount; shared stable key across 3 schemes; missing Wubi pins fail closed\n";
}

void retry(const fs::path& root){
    WubiRig rig(root,4);
    EditorSink fill(rig.app);
    while(rig.app.document.text().size()<65535){
        CHECK(fill.bind(0,0));
        CHECK(fill.commit(std::string(std::min(size_t(8192),65535-rig.app.document.text().size()),'q')));
    }
    rig.type("abcd");
    rig.settle();
    CHECK(rig.ime.receipt().status == CS::RetryableNoEffect && rig.ime.raw() == "abcd" && rig.app.document.text().size() == 65535);
    const auto first = rig.ime.attemptSerial();
    // Scheme switching is exercised separately; this sequence retains the
    // original retry handle to test retry and stale Esc below.
    auto stale = rig.freshEvent(0x29,true);
    for (unsigned i = 0; i < 50; ++i) {
        rig.step();
    }
    CHECK(rig.ime.attemptSerial() == first);
    rig.tap(0x2c);
    rig.step();
    CHECK(rig.ime.attemptSerial() != first);
    rig.send(stale);
    CHECK(!rig.ime.raw().empty());
    rig.send(rig.freshEvent(0x29,false));
    rig.settle();
    CHECK(rig.ime.receipt().status == CS::RetryableNoEffect && rig.app.document.text().size() == 65535);
    const auto unchanged = rig.ime.composition()->raw_input().stamp;
    auto stale_edit = rig.freshEvent(0x2a,true);
    stale_edit.ime_attempt = first;
    rig.send(stale_edit);
    rig.send(rig.freshEvent(0x2a,false));
    CHECK(rig.ime.composition()->input_is_current(unchanged) && rig.ime.raw() == "abcd");
    const auto previous = rig.ime.attemptSerial();
    rig.tap(0x2a);
    rig.settle();
    CHECK(rig.ime.raw() == "abc" && rig.ime.composition()->snapshot().member_count == 0 && rig.ime.candidateAvailable() && rig.ime.receipt().status == CS::Cancelled);
    rig.tap(0x2c);
    rig.settle();
    CHECK(rig.ime.receipt().status == CS::RetryableNoEffect && rig.ime.attemptSerial()>previous && rig.app.document.text().size() == 65535);
    rig.type("d");
    rig.settle();
    CHECK(rig.ime.raw() == "abcd" && rig.ime.receipt().status == CS::RetryableNoEffect && rig.app.document.text().size() == 65535);
    rig.tap(0x29); CHECK(rig.ime.rawEditing()&&!rig.ime.raw().empty()); rig.tap(0x29);
    rig.settle();
    CHECK(rig.ime.raw().empty());
    std::cout<<"PASS rejected-commit backspace/append retire captured old attempt and query full new code\n";
    std::cout<<"PASS zero-effect editor rejection; explicit original-handle retry; old cancel cannot cancel newer attempt; no automatic retry\n";
}

// Lower-level atomic edit assertion; product-event coverage lives in wubi_raw_edit.cpp.
void wholeCodeEdit(const fs::path& root){
    WubiRig rig(root,8);
    rig.type("ab");
    rig.settle();
    auto* c = rig.ime.composition();
    const auto selection = c->selection(0);
    CHECK(c->select(selection) == S::Working && finish(*c) == S::Ready);
    CHECK(c->snapshot().member_count == 1 && rig.app.document.text().empty());
    const auto revision = c->snapshot().revision;
    CHECK(c->edit_raw(2,2,"c") == S::Ready);
    CHECK(c->snapshot().member_count == 0 && c->snapshot().focus == 0 && c->snapshot().revision == revision+1);
    CHECK(c->query_next() == S::Working && finish(*c) == S::PageReady && c->page().items[0].key.bytes[31] == 3);
    CHECK(c->select(selection) == S::Stale);
    std::cout<<"PASS exact-code edit invalidates whole selected code, one revision, no retained partial member\n";
}

void pending(const fs::path& root){
    Rig loading(root,6);
    loading.resources.start(R::DirectExact);
    loading.tap(0x39);
    loading.type("abcd");
    CHECK(!loading.ime.composition() && loading.ime.raw() == "abcd" && loading.app.document.text().empty());
    loading.type("a");
    loading.type("z");
    CHECK(loading.ime.raw() == "abcd");
    loading.settle();
    CHECK(loading.app.document.text() == "四" && loading.ime.raw().empty() && loading.app.editor.undoCount() == 1);
    WubiRig explicit_choice(root,7);
    explicit_choice.type("abcd");
    explicit_choice.tap(0x2c);
    explicit_choice.settle();
    CHECK(explicit_choice.app.document.text() == "四" && explicit_choice.ime.raw().empty() && explicit_choice.app.editor.undoCount() == 1);
    std::cout<<"PASS loading full-code request binds only after real Composition; early explicit selection supersedes auto; no duplicate commit\n";
}
#if DRAFTOR_TELEMETRY_PROFILE
struct WubiWire: draftor::telemetry::TelemetrySink {
    std::ofstream file;
    explicit WubiWire(const fs::path& p):file(p,std::ios::binary){
    }
    draftor::telemetry::SinkResult try_write_frame(const uint8_t* p,size_t n)noexcept override{
        file.write(reinterpret_cast<const char*>(p),n);
        return draftor::telemetry::SinkResult::Accepted;
    }
    draftor::telemetry::SinkPollResult poll()noexcept override{
        return draftor::telemetry::SinkPollResult::NoLoss;
    }
};
void telemetry(const fs::path& root,const fs::path& output){
    WubiRig rig(root,5);
    WubiWire wire(output);
    std::vector<std::max_align_t> observer((ime_device::behavior::storageBytes()+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    ime_device::behavior::bind(observer.data(),0x5755424953594e54ULL,rig.app,&rig.ime);
    auto drain = [&](){
        for (unsigned i = 0; i < 40; ++i) {
            ime_device::behavior::service(wire, false);
        }
    };
    drain();
    auto settle = [&](){
        for(unsigned i = 0;;++i){
            CHECK(i<1000000);
            rig.step();
            ime_device::behavior::service(wire,false);
            if (!rig.ime.hasWork()) {
                break;
            }
        }
        rig.frame = rig.ime.framePresented();
        drain();
    };
    for(char letter:std::string("ab")){
        rig.tap(uint8_t(letter-'a'+4));
        drain();
    }
    settle();
    CHECK(rig.app.document.text().empty());
    rig.tap(0x2c);
    drain();
    settle();
    CHECK(rig.app.document.text() == "二");
    for(char letter:std::string("abcd")){
        rig.tap(uint8_t(letter-'a'+4));
        drain();
    }
    settle();
    CHECK(rig.app.document.text() == "二四");
    const auto* health = ime_device::behavior::health();
    CHECK(health && health->dropped_queue == 0 && health->dropped_invalid == 0 && health->dropped_encode == 0);
    ime_device::behavior::stop();
    drain();
    std::cout<<"PASS Wubi r2 producer actual selection/member/document points; wire="<<output.string()<<" host_injection_only=1\n";
}
#endif
int main(){
    try{
        auto name = (fs::current_path()/"wubi-product-XXXXXX").string();
        CHECK(mkdtemp(name.data()));
        const auto parent = fs::current_path();
        fs::current_path(name);
        const auto root = fixture();
        wubiBehavior(root);
        boundaries(root);
        staleAndIdentity(root);
        retry(root);
        wholeCodeEdit(root);
        pending(root);
#if DRAFTOR_TELEMETRY_PROFILE
        telemetry(root,parent/"wubi_host.bin");
#endif
        CHECK(metrics.resource_handles == 0 && metrics.document_handles == 0);
        std::cout<<"HOST_ONLY synthetic=1 sizeof_DeviceIme="<<sizeof(DeviceIme)<<" sizeof_Resources="<<sizeof(Resources)<<" largest_read="<<metrics.largest_read<<"\n";
        fs::current_path(parent);
        return 0;
    }
    catch(const std::exception& e){
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
