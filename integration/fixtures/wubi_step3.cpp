// Product events use the same App, DeviceIme, CandidateWindow and resource
// activation as the existing synthetic and real-resource regression lanes.
#define main original_wubi_product_main
#include "wubi_product.cpp"
#undef main
#include "product_input.hpp"
#include "vertical_ime.hpp"
#include "horizontal_raw.hpp"
#include "loading_status.hpp"
extern std::size_t phase2c_new_calls;
namespace {
constexpr uint8_t Esc=0x29,Space=0x2c,Left=0x50,Right=0x4f,F4=0x3d;
struct UiRig:Rig {
    s3app::ProductInput product;
    explicit UiRig(unsigned id, ReleasePins pins=productionPins(),fs::path root=REAL_RESOURCES)
        :Rig(root,id,pins) {
        key(0x39);settle();CHECK(ime.chinese());
        if(!app.horizontalActive())key(F4);
    }
    void deliver(const s3hid::Event& e){
        if(!product.layout(app,e)&&!product.ime(app,&ime,e))app.key(e);
    }
    void key(uint8_t u,uint8_t mods=0){deliver(freshEvent(u,true,mods));deliver(freshEvent(u,false,mods));}
    void letters(std::string_view s){for(char c:s)key(uint8_t(c-'a'+4));}
    void change(){key(Space,1);settle();}
    void wb(){change();change();CHECK(resources.rules()==R::DirectExact);}
    void raw(bool vertical){
        if(!app.horizontalActive())key(F4);
        key(Left);CHECK(ime.rawEditing());
        if(vertical)key(F4);
        settle();CHECK(app.horizontalActive()!=vertical);
    }
    void previous(){key(app.horizontalActive()?Left:0x52);}
    void next(){key(app.horizontalActive()?Right:0x51);}
    void trace(const char* label){
        std::cout<<"STEP3 {\"label\":\""<<label<<"\",\"raw\":\""<<ime.raw()
        <<"\",\"raw_edit\":"<<ime.rawEditing()<<",\"cursor\":"<<ime.rawCursor()
        <<",\"scheme\":"<<unsigned(resources.rules())<<",\"owner\":"<<ime.resourceOwner()
        <<",\"horizontal\":"<<app.horizontalActive()<<",\"spans\":"<<ime.presentationNavigation().span_count
        <<",\"body_bytes\":"<<app.document.text().size()<<",\"revision\":"<<app.document.textRevision()
        <<",\"handles\":"<<metrics.resource_handles<<"}\n";
    }
};
void presentation(UiRig& r,const char* code){
    CHECK(r.ime.raw()==code&&r.ime.presentationNavigation().span_count==0);
    CHECK(r.ime.candidateAvailable());
    auto* owner=r.ime.window();auto* editor=&r.app.editor;auto* port=&r.app.presentation();
    CHECK(prepareImePresentation(&r.ime,r.app));
    const auto viewport=r.ime.editViewport(r.font);
    CHECK(viewport.begin==0&&viewport.end==r.ime.raw().size());
    if(r.ime.rawEditing())CHECK(viewport.cursor_x==viewport.at(r.ime.rawCursor()));
    const auto snapshot=r.ime.composition()->snapshot();
    const auto descriptor=verticalCandidateView(snapshot,*r.ime.view());
    CHECK(descriptor.count>0&&descriptor.rows[0].items[0].identity==&r.ime.view()->rows[0].items[0]);
    CHECK(descriptor.rows[0].items[0].text==std::string_view(r.ime.view()->rows[0].items[0].text.data(),r.ime.view()->rows[0].items[0].bytes));
    if(r.app.horizontalActive()){
        unsigned ink=0;
        s3app::HorizontalRawPort paint;
        paint.context=&ink;paint.ink=[](void* p,int,int){++*static_cast<unsigned*>(p);};
        paint.accent=[](void*,int,int,unsigned,unsigned){};
        s3app::paintHorizontalRaw(r.ime,0,paint);CHECK(ink>0);
    }
    if(!r.app.horizontalActive()){
        struct Paint {std::string raw;unsigned candidates=0;};Paint p;
        draftor::vertical::AbPort draw;draw.line=[](void*,draftor::vertical::Rect,bool){};
        draw.context=&p;draw.run=[](void* x,const draftor::vertical::AbRun& run){
            auto& out=*static_cast<Paint*>(x);
            if(run.role==draftor::vertical::AbRole::Composition)out.raw+=run.text;
            if(run.role==draftor::vertical::AbRole::CurrentCandidate){CHECK(run.identity);++out.candidates;}
        };
        const auto frame=verticalIme(&r.ime,r.app,r.now,0,draw);
        CHECK(frame.layout.status==draftor::vertical::AbStatus::Ready);
        CHECK(p.raw==code&&p.candidates>0);
        r.frame=publishVerticalFrame(&r.ime,r.app,frame);CHECK(r.frame);
    }
    CHECK(owner==r.ime.window()&&editor==&r.app.editor&&port==&r.app.presentation());
    CHECK(r.ime.composition()->count_begin_calls()==0);
}
void editing(bool vertical){
    UiRig r(vertical?502:501);r.wb();
    if(vertical)r.key(F4);
    for(const auto* code:{"a","vv","aaa"}){
        r.letters(code);r.settle();presentation(r,code);r.key(Esc);CHECK(r.ime.rawEditing()&&r.ime.raw()==code);r.key(Esc);r.settle();
        CHECK(r.ime.raw().empty());
    }
    r.letters("aaar");r.settle();
    auto* owner=r.ime.window();auto* composition=r.ime.composition();
    r.raw(vertical);presentation(r,"aaar");r.trace("raw_enter");
    const auto original=composition->raw_input().stamp;const auto query=owner->querySerial();
    const auto old_frame=r.frame;auto stale=r.freshEvent(Esc,true);
    r.key(0x2a);r.settle();CHECK(r.ime.raw()=="aar"&&r.ime.rawEditing());
    CHECK(!composition->input_is_current(original)&&owner->querySerial()>query);
    r.letters("a");r.settle();CHECK(r.ime.raw()=="aaar"&&r.ime.rawEditing());
    presentation(r,"aaar");
    const auto current=composition->raw_input().stamp;
    r.deliver(stale);r.deliver(r.freshEvent(Esc,false));
    CHECK(r.ime.rawEditing()&&composition->input_is_current(current)&&r.ime.raw()=="aaar");
    auto old_choice=r.freshEvent(31,true);old_choice.ime_frame=old_frame;
    r.deliver(old_choice);r.deliver(r.freshEvent(31,false));r.settle();
    CHECK(r.app.document.text().empty()&&r.ime.rawEditing());
    r.previous();const auto cursor=r.ime.rawCursor();r.next();CHECK(r.ime.rawCursor()==cursor+1);
    r.key(0x4c);r.settle();CHECK(r.ime.raw()=="aaa"&&r.ime.rawEditing());
    r.letters("r");r.settle();CHECK(r.ime.raw()=="aaar"&&r.ime.rawEditing());
    CHECK(owner==r.ime.window()&&composition==r.ime.composition());
    presentation(r,"aaar");r.trace("edited_refreshed");
    r.key(Esc);r.settle();CHECK(!r.ime.rawEditing()&&r.ime.raw().empty());r.letters("aaar");r.settle(); // Fresh candidate setup preserves full-range presentation checks.
    CHECK(r.ime.composition()->page().begin==0&&r.ime.composition()->page().end==4);
    presentation(r,"aaar");r.trace("fresh_candidates_after_cancel");
    r.key(Esc);CHECK(r.ime.rawEditing());r.key(Esc);r.settle();CHECK(r.ime.raw().empty()&&r.app.document.text().empty());
    // Both explicit selections work in raw edit, with one original held press.
    for(uint8_t choice:{Space,uint8_t(31)}){
        r.letters("aaar");r.settle();r.raw(vertical);presentation(r,"aaar");
        const auto item=r.ime.view()->rows[0].items[choice==Space?0:1];
        const std::string expected=std::string(r.app.document.text())+std::string(item.text.data(),item.bytes);
        const auto rev=r.app.document.textRevision();const auto undo=r.app.editor.undoCount();
        auto press=r.freshEvent(choice,true);r.deliver(press);r.settle();
        press.ms=++r.now;r.deliver(press);r.deliver(r.freshEvent(choice,false));r.settle();
        CHECK(std::string_view(r.app.document.text())==expected&&r.app.document.textRevision()==rev+1&&r.app.editor.undoCount()==undo+1);
        CHECK(r.ime.raw().empty());r.trace("selection_once");
    }
    // Full safe code edited under raw focus cannot arm an auto action, even
    // when Esc and the switch arrive before the next resource/service slice.
    r.letters("lti");r.settle();r.raw(vertical);r.next();r.letters("f");
    CHECK(r.ime.raw()=="ltif"&&r.ime.rawEditing());
    const auto body=std::string(r.app.document.text());r.key(Esc);r.settle();
    CHECK(r.ime.raw().empty()&&std::string_view(r.app.document.text())==body&&!r.ime.rawEditing());
    r.change();CHECK(r.resources.rules()==R::R1Xiaohe&&r.ime.raw().empty()&&std::string_view(r.app.document.text())==body);
    std::cout<<"PASS Step3 H/V presentation, byte edit/query refresh, stale Esc/frame, common Esc, Space/digit once\n";
}
void cycles(){
    UiRig r(503);const auto shared=r.resources.sharedCacheAllocation();
    const auto admission=metrics.phases[unsigned(Phase::Admission)].steps;
    for(unsigned i=0;i<6;++i){
        const auto from=r.resources.rules();
        r.letters(from==R::R1Xiaohe?"ni":from==R::FullPinyin?"ni":"aaar");r.settle();
        CHECK(r.ime.candidateAvailable());
        if(i>=3)r.raw(i==4);
        auto old_letter=r.freshEvent(4,true),old_choice=r.freshEvent(Space,true),old_esc=r.freshEvent(Esc,true);
        const auto owner=r.ime.resourceOwner();const auto old_item=r.ime.view()->rows[0].items[0];
        const auto old_scheme=old_item.scheme;
        const auto before=phase2c_new_calls;
        const uint8_t mods=i%3==0?1:i%3==1?8:9;
        auto press=r.freshEvent(Space,true,mods);r.deliver(press);
        const auto sync_calls=phase2c_new_calls-before;
        const auto expected=i%3==0?R::FullPinyin:i%3==1?R::DirectExact:R::R1Xiaohe;
        CHECK(r.resources.rules()==expected&&r.ime.raw().empty()&&!r.ime.rawEditing());
        CHECK(r.ime.resourceOwner()!=owner);
        press.ms=++r.now;r.deliver(press);CHECK(r.resources.rules()==expected);
        r.deliver(r.freshEvent(Space,false,mods));r.settle();CHECK(r.resources.rules()==expected);
        const auto all_calls=phase2c_new_calls-before;
        CHECK(r.resources.sharedCacheAllocation()==shared&&metrics.phases[unsigned(Phase::Admission)].steps==admission);
        CHECK(r.resources.policy().rules==expected&&r.resources.optimized());
        CHECK(r.ime.composition()->snapshot().scheme_epoch!=old_scheme);
        char caption[64];modeCaption(caption,sizeof(caption),true,expected,false);
        CHECK(std::string(caption).find(expected==R::FullPinyin?"PY":expected==R::DirectExact?"WB":"XH")!=std::string::npos);
        for(auto event:{old_letter,old_choice,old_esc}){r.deliver(event);event.down=false;event.ms=++r.now;r.deliver(event);}
        r.settle();CHECK(r.ime.raw().empty()&&r.app.document.text().empty());
        std::cout<<"COST {\"cycle\":"<<i<<",\"switch_new_calls\":"<<sync_calls<<",\"switch_and_activation_new_calls_with_host_scaffolding\":"<<all_calls<<",\"active_handles\":"<<metrics.resource_handles<<"}\n";
        r.trace("cycle");
    }
    r.wb();r.letters("lti");r.settle();r.letters("f");
    r.key(Left);CHECK(r.ime.rawEditing());r.key(Esc);r.settle();
    CHECK(r.ime.raw().empty()&&!r.ime.rawEditing()&&r.app.document.text().empty());
    r.key(Esc);r.settle();
    r.letters("ltif"); // pending auto, no settle before switching
    r.change();CHECK(r.resources.rules()==R::R1Xiaohe&&r.app.document.text().empty()&&r.ime.raw().empty());
    std::cout<<"PASS two full XH/PY/WB cycles: Ctrl/GUI/combined, release/repeat, unfinished input/raw edit, retired events, shared cache, pending auto\n";
}
void failures(){
    const auto missing=fs::current_path()/"step3-missing";fs::create_directories(missing);
    for(const auto& p:fs::directory_iterator(REAL_RESOURCES))if(p.path().filename()!="wubi86.hnv"){
        const auto target=missing/p.path().filename();if(!fs::exists(target))fs::create_symlink(fs::canonical(p.path()),target);
    }
    for(unsigned test=0;test<2;++test){
        auto pins=productionPins();auto direct=*pins.direct;auto spec=*direct.scheme;
        if(test){++spec.file_sha256[0];direct.scheme=&spec;pins.direct=&direct;}
        UiRig r(504+test,pins,test?fs::path(REAL_RESOURCES):missing);r.change();
        r.key(Space,1);CHECK(r.resources.rules()==R::DirectExact);auto old=r.freshEvent(4,true);
        for(unsigned i=0;i<100000&&r.resources.state()!=ResourceState::Error;++i)r.step();
        CHECK(r.resources.state()==ResourceState::Error);r.step();
        CHECK(!r.ime.chinese()&&r.ime.raw().empty()&&r.app.document.text().empty());
        r.deliver(old);old.down=false;old.ms=++r.now;r.deliver(old);CHECK(r.app.document.text().empty());
        r.letters("q");CHECK(r.app.document.text()=="q");
        CHECK(!r.resources.error().empty());r.trace("resource_error_body_available");
    }
    // Frozen build222 policy: wrong auto-safe binding denies AUTO; verified
    // real resources still permit an explicit current manual selection.
    auto pins=productionPins();auto direct=*pins.direct;auto meta=*direct.auto_safe;
    auto other=*meta.scheme;++other.file_sha256[0];meta.scheme=&other;direct.auto_safe=&meta;pins.direct=&direct;
    UiRig r(506,pins);r.wb();r.letters("ltif");r.settle();
    CHECK(!r.resources.autoSafe("ltif")&&r.ime.raw()=="ltif"&&r.app.document.text().empty());
    r.key(Space);r.settle();CHECK(r.ime.receipt().status==CS::Accepted&&r.ime.raw().empty());
    std::cout<<"PASS missing/mismatched release fails closed with EN body available; wrong auto-safe binding disables auto per frozen build222 policy\n";
}
void retrySwitch(){
    UiRig r(507);r.wb();EditorSink sink(r.app);
    while(r.app.document.text().size()<65535){CHECK(sink.bind(0,0));CHECK(sink.commit(std::string(std::min(size_t(8192),65535-r.app.document.text().size()),'q')));}
    r.letters("ltif");r.settle();CHECK(r.ime.receipt().status==CS::RetryableNoEffect);
    const auto serial=r.ime.attemptSerial();auto old=r.freshEvent(Space,true,1);old.ime_attempt=0;
    r.deliver(old);r.deliver(r.freshEvent(Space,false,1));
    CHECK(r.resources.rules()==R::DirectExact&&r.ime.attemptSerial()==serial&&r.ime.receipt().status==CS::RetryableNoEffect);
    r.change();CHECK(r.resources.rules()==R::R1Xiaohe&&r.ime.raw().empty()&&r.app.document.text().size()==65535);
    std::cout<<"PASS common retry switch cancels only original captured no-effect attempt; stale attempt cannot switch; no implicit commit\n";
}
}
int main(int argc,char** argv){try{
    CHECK(argc==2);const unsigned id=unsigned(std::stoul(argv[1]));
    if(id==501||id==502)editing(id==502);else if(id==503)cycles();else if(id==504)failures();else if(id==507)retrySwitch();else CHECK(false);
    CHECK(metrics.resource_handles==0&&metrics.document_handles==0);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
