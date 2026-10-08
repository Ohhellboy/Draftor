// Production resources and real DeviceIme -> Submission -> EditorSink -> App.
// Timings are Host-only; the expected body is explicit, not another IME model.
#include "device_ime.hpp"
#include "chinese_font.hpp"
#include "hanime_document_sd_storage.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace ime_device;
namespace fs = std::filesystem;
using CommitStatus = hanime::CompositionStatus;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " + #x); } while (0)

struct Scratch {
    fs::path path;
    Scratch() {
        char name[] = "space-commit-XXXXXX";
        CHECK(mkdtemp(name));
        path = fs::absolute(name);
    }
    ~Scratch() { std::error_code error; fs::remove_all(path, error); }
};

struct Rig {
    Scratch scratch;
    s3fs::Fs files{scratch.path.string()};
    hanime::documents::ArduinoSdStoragePort storage{files};
    ChineseFont font;
    s3app::App app{storage, {}, 7100, font};
    Resources resources{RESOURCE_ROOT, std::make_shared<Mount>(), productionPins(), INDEX_ROOT};
    DeviceIme ime{app, resources, font};
    uint32_t now = 100, frame = 0;

    Rig() {
        CHECK(font.begin());
        storage.noteMounted();
        CHECK(app.startup(now));
    }
    void send(const s3hid::Event& event) { if (!ime.event(event)) app.key(event); }
    s3hid::Event down(uint8_t key, uint8_t mods = 0) {
        s3hid::Event event{1, ++now, key, mods, true, 0, frame, ime.attemptSerial()};
        event.ime_context=ime.ingressContext();event.resource_owner=ime.resourceOwner();
        send(event);
        return event;
    }
    void up(s3hid::Event event) { event.ms = ++now; event.down = false; send(event); }
    void tap(uint8_t key, uint8_t mods = 0) { up(down(key, mods)); }
    void step() { ime.service(); }
    void settle() {
        unsigned steps = 0;
        while (ime.hasWork() && steps++ < 2000000) step();
        CHECK(steps < 2000000);
        frame = ime.framePresented();
    }
    void cn() { tap(0x39); settle(); CHECK(ime.chinese()); }
    void type(std::string_view raw) { for (char c : raw) tap(uint8_t(c - 'a' + 4)); }
    std::string first() const {
        const auto* view = ime.view();
        CHECK(view && view->count && view->rows[0].count);
        const auto& item = view->rows[0].items[0];
        return {item.text.data(), item.bytes};
    }
};

void oneSpaceAndNextInput(uint8_t selector = 0x2c) {
    Rig rig;
    rig.cn(); rig.type("ui"); rig.settle();
    CHECK(rig.first() == "是" && rig.app.document.text().empty());
    const auto original = rig.down(selector);
    auto repeated = original;
    repeated.ms = ++rig.now;
    rig.send(repeated);
    rig.up(original); // Release while the captured choice is still being read.
    rig.settle();
    CHECK(rig.app.document.text() == "是");
    CHECK(rig.ime.receipt().status == CommitStatus::Accepted && rig.ime.raw().empty());
    rig.send(original); // Replayed down/up cannot escape into the body.
    rig.up(original);
    rig.settle();
    CHECK(rig.app.document.text() == "是");
    rig.type("ui"); rig.settle(); rig.tap(selector); rig.settle();
    CHECK(rig.app.document.text() == "是是" && rig.ime.raw().empty());
    std::cout << "PASS ui one Space accepts once; release/repeat/replay isolated; next input binds afresh\n";
}

void remainingRawAndHeldKey(uint8_t first = 0x2c, uint8_t last = 0x2c) {
    Rig rig;
    rig.cn(); rig.type("wjdjle"); rig.settle();
    CHECK(rig.first() == "完蛋");
    rig.tap(first); rig.settle();
    CHECK(rig.app.document.text().empty() && rig.ime.selected() == "完蛋");
    CHECK(rig.ime.raw() == "wjdjle" && rig.first() == "了");
    auto held = rig.down(last);
    rig.settle(); // The authorized selection completes even with key held down.
    CHECK(rig.app.document.text() == "完蛋了");
    auto repeated = held; repeated.ms = ++rig.now; rig.send(repeated);
    rig.up(held); rig.settle();
    CHECK(rig.app.document.text() == "完蛋了");
    std::cout << "PASS partial raw retained; final Space selects and submits entire body without another key\n";
}


void numberedItemsAndEditingBeforeFinal() {
    for (uint8_t slot = 0; slot < 6; ++slot) {
        Rig rig; rig.cn(); rig.type("ui"); rig.settle();
        if (slot >= rig.ime.view()->rows[0].count) break;
        const auto item = rig.ime.view()->rows[0].items[slot];
        CHECK(item.bytes);
        rig.tap(uint8_t(30 + slot)); rig.settle();
        CHECK(rig.app.document.text() == std::string_view(item.text.data(), item.bytes));
        CHECK(rig.ime.receipt().status == CommitStatus::Accepted);
    }
    {
        Rig rig; rig.cn(); rig.type("wjdjle"); rig.settle();
        rig.tap(30); rig.settle();
        rig.tap(0x2a); CHECK(rig.ime.raw() == "wjdjl");
        CHECK(rig.app.document.text().empty());
        rig.type("e"); rig.settle(); CHECK(rig.first() == "了");
        rig.tap(0x2c); rig.settle(); CHECK(rig.app.document.text() == "完蛋了");
    }
    {
        Rig rig; rig.cn(); rig.type("wjdjle"); rig.settle();
        rig.tap(0x2c); rig.settle();
        // Current unprocessed le is the new entry; two more Lefts reach wj.
        rig.tap(0x50); CHECK(rig.ime.rawCursor() == 6);
        rig.tap(0x50); rig.tap(0x50); CHECK(rig.ime.rawCursor() == 2);
        rig.tap(0x51); rig.settle(); CHECK(rig.first() == "玩");
        rig.tap(30); rig.settle(); CHECK(rig.app.document.text().empty());
        rig.tap(0x50); CHECK(rig.ime.rawCursor() == 4);
        rig.tap(0x51); rig.settle();
        CHECK(std::string_view(rig.ime.view()->rows[0].items[3].text.data(), rig.ime.view()->rows[0].items[3].bytes) == "蛋");
        rig.tap(33); rig.settle(); CHECK(rig.first() == "了" && rig.app.document.text().empty());
        rig.tap(30); rig.settle(); CHECK(rig.app.document.text() == "玩蛋了");
    }
    std::cout << "PASS all visible numbered slots exact body; Backspace and Left/K1 edit before final; mixed final commit\n";
}

void otherSelectionKeysStillStage() {
    for (const auto key : {uint8_t(0x28), uint8_t(0x2c)}) {
        Rig rig;
        rig.cn(); rig.type("ui"); rig.settle();
        const uint8_t mods = key == 0x2c ? 0x02 : 0;
        rig.tap(key, mods); rig.settle();
        CHECK(rig.app.document.text().empty() && rig.ime.selected() == "是");
        rig.tap(0x28); rig.settle();
        CHECK(rig.app.document.text() == "是");
    }
    std::cout << "PASS Enter and modified Space keep their former staging/confirmation behavior\n";
}

void waitingAndOldDisplayedChoice(uint8_t selector = 0x2c) {
    Rig rig;
    rig.cn(); rig.type("ui");
    rig.frame = rig.ime.framePresented();
    CHECK(!rig.frame && rig.ime.window()->busy());
    rig.tap(selector); rig.settle();
    CHECK(rig.app.document.text()=="是");rig.type("ui");rig.settle();
    CHECK(rig.first() == "是");
    rig.ime.window()->request(5, 0, true);
    CHECK(rig.ime.window()->busy());
    rig.tap(selector); // Capture the original on-screen item during demand text work.
    rig.settle();
    CHECK(rig.app.document.text() == "是是");
    std::cout << "PASS current-query Space/initial digit resolve; displayed original remains exact\n";
}

void cancelledIntents(uint8_t selector = 0x2c) {
    for (unsigned action = 0; action < 5; ++action) {
        Rig rig;
        rig.cn(); rig.type("ui"); rig.settle();
        const auto original = rig.down(selector);
        rig.up(original); // Selection is captured, but no completion has run.
        if (action == 0) rig.tap(0x29);
        if (action == 1) rig.type("a");
        if (action == 2) rig.tap(0x2c, 0x01);
        if (action == 3) rig.app.disconnected();
        if (action == 4) ++rig.resources.mount()->epoch;
        rig.settle();
        const auto expected=action==1?"是":"";
        CHECK(rig.app.document.text()==expected);
        if(action==1)CHECK(rig.ime.raw()=="a"); // User now explicitly authorizes following letters.
        rig.send(original); rig.up(original); rig.settle();
        CHECK(rig.app.document.text()==expected);
    }
    Rig external;
    external.cn(); external.type("ui"); external.settle(); external.tap(selector);
    external.ime.window()->cancel();
    auto& composition = *external.ime.composition();
    CHECK(composition.cancel_input(composition.snapshot().input_id) == hanime_extension::Status::Cancelled);
    CHECK(composition.edit_raw(0, 0, "ui") == hanime_extension::Status::Ready);
    external.ime.window()->query();
    external.settle();
    CHECK(external.app.document.text().empty() && external.ime.selected().empty());
    std::cout << "PASS edit/cancel/switch/target/resource and unrelated new input invalidate original Space intent\n";
}

void retainedSuffixRecheck() {
    Rig rig;
    rig.cn(); rig.type("wjdjle"); rig.settle();
    rig.ime.window()->choose(0, rig.frame); rig.settle();
    rig.ime.window()->choose(0, rig.frame); rig.settle();
    CHECK(rig.ime.selected() == "完蛋了" && rig.app.document.text().empty());
    rig.tap(0x50); rig.tap(0x50);
    CHECK(rig.ime.rawCursor() == 2);
    rig.tap(0x51); rig.settle();
    CHECK(rig.first() == "玩");
    rig.tap(0x2c); rig.settle();
    CHECK(rig.app.document.text().empty());
    // Production djle also maps longer words (e.g. 淡了). K1 requests dj,
    // then the user moves to its actual fourth candidate, 蛋.
    rig.tap(0x50); CHECK(rig.ime.rawCursor() == 4);
    rig.tap(0x51); rig.settle();
    for (unsigned i = 0; i < 3; ++i) { rig.tap(0x4f); rig.settle(); }
    const auto& item = rig.ime.view()->rows[0].items[3];
    CHECK(std::string_view(item.text.data(), item.bytes) == "蛋");
    rig.tap(0x2c); rig.settle();
    CHECK(rig.app.document.text() == "玩蛋了");
    std::cout << "PASS final explicit Space may continue its own retained-suffix recheck, then submit once\n";
}

void capacityAndRetry(uint8_t selector = 0x2c) {
    Rig rig;
    EditorSink fill(rig.app);
    while (rig.app.document.text().size() < 65535) {
        CHECK(fill.bind(100, 1));
        CHECK(fill.commit(std::string(std::min(size_t(8192), 65535 - rig.app.document.text().size()), 'q')));
    }
    rig.cn(); rig.type("ui"); rig.settle(); rig.tap(selector); rig.settle();
    CHECK(rig.ime.receipt().status == CommitStatus::RetryableNoEffect);
    CHECK(rig.app.document.text().size() == 65535 && rig.ime.selected() == "是");
    const auto first = rig.ime.attemptSerial();
    const auto work = metrics.phases[unsigned(Phase::Submission)].steps;
    for (unsigned i = 0; i < 30; ++i) rig.step();
    CHECK(metrics.phases[unsigned(Phase::Submission)].steps == work);
    rig.tap(0x2c); rig.step();
    CHECK(rig.ime.attemptSerial() != first);
    rig.send({1, ++rig.now, 0x29, 0, true, 0, rig.frame, first, 0, 0, rig.ime.ingressContext(), rig.ime.resourceOwner()});
    rig.send({1, ++rig.now, 0x29, 0, false, 0, rig.frame, first, 0, 0, rig.ime.ingressContext(), rig.ime.resourceOwner()});
    rig.settle();
    CHECK(rig.ime.receipt().status == CommitStatus::RetryableNoEffect);
    CHECK(rig.app.document.text().size() == 65535);
    rig.tap(0x29); rig.settle();
    CHECK(rig.ime.raw().empty() && rig.app.document.text().size() == 65535);
    std::cout << "PASS capacity rejection keeps input; retry requires a new key/new attempt; old cancel rejected\n";
}

void plannedPrefixViaExistingK1() {
    for (auto first : {uint8_t(30), uint8_t(0x2c)}) {
        for (auto last : {uint8_t(30), uint8_t(0x2c)}) {
            Rig rig; rig.cn(); rig.type("wojbtm"); rig.settle();
            CHECK(rig.first() == "我"); // Q1 default changed; explicit K1 remains available.
            rig.tap(0x50); rig.tap(0x50); CHECK(rig.ime.rawCursor() == 2);
            rig.tap(0x51); rig.settle(); CHECK(rig.first() == "我");
            rig.tap(first); rig.settle(); CHECK(rig.first() == "今天");
            CHECK(rig.app.document.text().empty());
            rig.tap(last); rig.settle(); CHECK(rig.app.document.text() == "我今天");
            CHECK(rig.ime.raw().empty());
        }
    }
    std::cout << "PASS wojbtm existing Left/Left/Down K1, digit/Space/mixed whole commit with Q1\n";
}
void automaticSegmentation() {
    for (bool full : {false, true}) for (uint8_t first : {uint8_t(30), uint8_t(0x2c)})
        for (uint8_t last : {uint8_t(30), uint8_t(0x2c)}) {
            Rig rig; rig.cn();
            if (full) { rig.tap(0x2c, 1); rig.settle(); }
            rig.type(full ? "wojintian" : "wojbtm"); rig.settle();
            CHECK(rig.first() == "我" && rig.ime.view()->rows[0].items[0].end == 2);
            const auto plan = rig.ime.composition()->planning_report();
            CHECK(rig.ime.composition()->plan_is_current(plan));
            rig.tap(first); rig.settle();
            CHECK(rig.app.document.text().empty() && rig.ime.selected() == "我" && rig.first() == "今天");
            CHECK(!rig.ime.composition()->plan_is_current(plan));
            rig.tap(last); rig.settle();
            CHECK(rig.app.document.text() == "我今天" && rig.ime.raw().empty());
            CHECK(rig.ime.receipt().status == CommitStatus::Accepted);
        }
    std::cout << "PASS Q1 xiaohe/full default me+today, digits/Space/mixed, no partial body, final accepted once\n";
}
int main() {
    try {
        oneSpaceAndNextInput(30);
        oneSpaceAndNextInput();
        for (auto first : {uint8_t(30), uint8_t(0x2c)})
            for (auto last : {uint8_t(30), uint8_t(0x2c)}) remainingRawAndHeldKey(first, last);
        numberedItemsAndEditingBeforeFinal();
        otherSelectionKeysStillStage();
        waitingAndOldDisplayedChoice();
        waitingAndOldDisplayedChoice(30);
        cancelledIntents();
        cancelledIntents(30);
        retainedSuffixRecheck();
        capacityAndRetry();
        capacityAndRetry(30);
        CHECK(metrics.resource_handles == 0 && metrics.document_handles == 0);
        plannedPrefixViaExistingK1();
        automaticSegmentation();
        std::cout << "PASS space_commit Space/digit/mixed behavior groups; no device timing claim\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
