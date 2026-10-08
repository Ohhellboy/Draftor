// Fixed synthetic event replay using the production resource consumer. The
// exact-prefix setup isolates capacity from the separate long-string budget gate.
#define main capacity_existing_query_rows_main
#include "query_rows.cpp"
#undef main
#include "ui_trace.hpp"
#include <fstream>

struct CapacityRig : Rig {
    std::ofstream trace;
    std::vector<uint8_t> results;
    size_t accepted = 0;
    uint32_t event_sequence = 0;
    explicit CapacityRig(const std::string& name) : trace(name + ".trace.log") {
        UiTraceRecord old;
        while (uiTrace.pop(old)) {}
        uiTrace.setMode(1);
        trace << "[BOOT] HOST_SYNTHETIC_CAPACITY_REPLAY\n[TRACE_SCHEMA] version=2 mode=1\n";
    }
    void drain() {
        UiTraceRecord record;
        while (uiTrace.pop(record)) {
            if (record.kind == UiTraceKind::Intent) results.push_back(record.detail);
            if (record.kind == UiTraceKind::BodyAccepted) ++accepted;
            for (unsigned part = 0; part < UiTraceParts; ++part) {
                char line[384];
                const auto n = formatUiTrace(record, part, line, sizeof(line));
                trace.write(line, std::streamsize(n));
            }
        }
    }
    void key(uint8_t key, uint8_t modifiers = 0) {
        for (bool down : {true, false}) {
            s3hid::Event event{1, ++now, key, modifiers, down, 0, frame,
                              ime.attemptSerial(), ++event_sequence, clockUs()};
            event.ime_context = ime.ingressContext(); event.resource_owner = ime.resourceOwner();
            if (!ime.event(event)) app.key(event);
            drain();
        }
    }
    void letters(std::string_view text) { for (char c : text) key(uint8_t(c - 'a' + 4)); }
    void tick() { step(); drain(); }
    void ready(bool publish = true) {
        unsigned steps = 0;
        do { tick(); CHECK(++steps < 2000000); }
        while (!resources.ready() || !ime.composition() || ime.hasWork());
        if (publish) frame = ime.framePresented();
    }
    void full(bool py = false) {
        key(0x39); ready();
        if (py) { key(44, 1); ready(); }
        for (unsigned i = 0; i < 16; ++i) letters(py ? "wode" : "wjdj");
        CHECK(ime.raw().size() == 64);
        ime.window()->query(4); drain();
    }
    void finish(uint8_t choice) {
        // Sixteen formal four-byte members fit the fixed member limit. Each
        // selection is a real HID event; no test sink or fabricated Selection.
        for (unsigned i = 0; i < 17 && !ime.raw().empty(); ++i) {
            ready();
            if (ime.window()->state() != WindowState::Ready) {
                const auto snapshot = ime.composition()->snapshot();
                if (snapshot.focus < snapshot.raw.size()) {
                    ime.window()->query(snapshot.focus + 4); drain(); ready();
                }
            }
            key(choice); ready();
        }
    }
};

struct CapacitySnapshot {
    uint64_t input, revision;
    size_t focus, members, cursor;
    uint32_t query, layout;
    bool reselecting;
    EditTarget target;
    uint64_t view;
    size_t arrows, queued;
    std::string raw, selected;
    std::vector<hanime_extension::Member> values;
    explicit CapacitySnapshot(CapacityRig& rig) {
        const auto snap = rig.ime.composition()->snapshot();
        input = snap.input_id; revision = snap.revision; focus = snap.focus;
        members = snap.member_count; cursor = rig.ime.rawCursor();
        query = rig.ime.window()->querySerial(); layout = rig.ime.window()->layoutSerial();
        reselecting = rig.ime.reselecting(); raw = snap.raw; selected = snap.selected_text;
        values.assign(snap.members, snap.members + snap.member_count);
        target = rig.ime.editTarget(); view = digest(*rig.ime.view());
        arrows = rig.ime.queuedArrows(); queued = rig.ime.queuedInput();
    }
    bool same(const CapacitySnapshot& b) const {
        if (input != b.input || revision != b.revision || focus != b.focus ||
            members != b.members || cursor != b.cursor || query != b.query ||
            layout != b.layout || reselecting != b.reselecting || raw != b.raw || selected != b.selected ||
            target.kind != b.target.kind || target.begin != b.target.begin || target.end != b.target.end ||
            view != b.view || arrows != b.arrows || queued != b.queued) return false;
        for (size_t i = 0; i < members; ++i) {
            const auto& x = values[i]; const auto& y = b.values[i];
            if (x.key != y.key || x.ref != y.ref || x.begin != y.begin ||
                x.end != y.end || x.valid != y.valid) return false;
        }
        return true;
    }
};

void capacityCase(const std::string& timing, uint8_t choice, bool reject, bool observe, bool py = false) {
    const auto name = "capacity_" + timing + "_" + std::to_string(choice) + (reject ? "_reject" : "_control") + (py ? "_py" : "_xh");
    CapacityRig rig(name); rig.full(py);
    if (timing != "query") rig.ready();
    if (timing == "reselection") {
        rig.ime.window()->choose(0, rig.frame); rig.ready();
        rig.key(0x50);
        while (rig.ime.rawCursor() > 4) rig.key(0x50);
        while (rig.ime.rawCursor() < 4) rig.key(0x4f); // IC1 complete first left member, not cursor0.
        rig.key(0x51); rig.ready();
        CHECK(rig.ime.reselecting());
    }
    const CapacitySnapshot before(rig);
    const auto io = metrics.io();
    rig.results.clear();
    if (reject) rig.letters("a");
    const CapacitySnapshot after(rig);
    const auto delta = metrics.io() - io;
    const bool preserved = before.same(after);
    std::string disposition = rig.results.empty() ? "no_action" : intentName(rig.results.back());
    // Publish the empty/replacement frame in one timing; retain the old screen
    // in the other. The latter is an essential successful pre-fix control.
    if (timing == "published") rig.frame = rig.ime.framePresented();
    rig.key(choice); rig.ready();
    const bool selection_progress = !rig.ime.selected().empty() || !rig.app.document.text().empty();
    std::cout << "{\"case\":\"" << name << "\",\"raw_bytes\":" << after.raw.size()
              << ",\"revision_before\":" << before.revision << ",\"revision_after\":" << after.revision
              << ",\"query_before\":" << before.query << ",\"query_after\":" << after.query
              << ",\"layout_before\":" << before.layout << ",\"layout_after\":" << after.layout
              << ",\"members\":" << after.members << ",\"focus\":" << after.focus
              << ",\"target_begin\":" << after.target.begin << ",\"target_end\":" << after.target.end
              << ",\"preserved\":" << (preserved ? "true" : "false")
              << ",\"event_result\":\"" << disposition << "\",\"rejection_reads\":" << delta.reads
              << ",\"rejection_bytes\":" << delta.bytes << ",\"selection_progress\":" << (selection_progress ? "true" : "false") << "}\n";
    if (!observe) {
        CHECK(preserved);
        CHECK(!reject || disposition == "capacity_rejected");
        CHECK(selection_progress);
    }
    if (selection_progress) {
        rig.finish(choice);
        std::cout << "{\"case\":\"" << name << "\",\"final_body\":\"" << rig.app.document.text()
                  << "\",\"remaining_raw\":" << rig.ime.raw().size() << "}\n";
        if (!observe) {
            std::string expected;
            for (unsigned i = 0; i < 16; ++i) expected += py ? "我的" : "完蛋";
            CHECK(rig.ime.raw().empty() && std::string_view(rig.app.document.text()) == expected && rig.accepted == 1);
        }
    }
}

void adjacentCases() {
    using A = RawAdmission;
    CHECK(admitRawEdit(64, 64, 64, "a") == A::Capacity);
    CHECK(admitRawEdit(64, 63, 64, "a") == A::Edit);
    CHECK(admitRawEdit(64, 0, 64, "ui") == A::Edit);
    CHECK(admitRawEdit(64, 64, 64, "") == A::NoChange);
    CHECK(admitRawEdit(0, 0, 0, "") == A::NoChange);
    CHECK(admitRawEdit(64, 65, 64, "") == A::Invalid);
    CHECK(admitRawEdit(64, 0, SIZE_MAX, "a") == A::Invalid);
    CHECK(admitRawEdit(SIZE_MAX, 0, 0, "a") == A::Capacity);
    CHECK(admitRawEdit(64, 0, 1, "A") == A::Invalid);
    {
        CapacityRig r("capacity_loading"); r.key(0x39);
        for (unsigned i = 0; i < 16; ++i) r.letters("wjdj");
        CHECK(!r.ime.composition() && r.ime.raw().size() == 64);
        const auto context = r.ime.ingressContext();
        r.letters("a");
        CHECK(r.ime.raw().size() == 64 && r.ime.ingressContext() == context);
        CHECK(r.results.back() == CapacityRejected && r.app.document.text().empty());
        r.key(42); r.letters("j"); CHECK(r.ime.raw().size() == 64);
        for (unsigned i = 0; i < 64; ++i) r.key(42);
        CHECK(!r.ime.active() && r.ime.raw().empty());
        r.ready(); r.letters("ui"); r.key(44); r.ready();
        CHECK(r.app.document.text() == "是" && r.accepted == 1);
    }
    {
        CapacityRig r("capacity_navigation"); r.full(); r.ready();
        r.key(81); const auto arrows = r.ime.queuedArrows();
        const auto discarded = r.ime.arrowDiscarded();
        const CapacitySnapshot before(r); r.letters("a");
        CHECK(before.same(CapacitySnapshot(r)) && r.ime.queuedArrows() == arrows);
        CHECK(r.ime.arrowDiscarded() == discarded); r.ready();
        CapacityRig control("capacity_navigation_control"); control.full(); control.ready();
        control.key(81); control.ready();
        CHECK(r.ime.view()->row == control.ime.view()->row);
        CHECK(digest(*r.ime.view()) == digest(*control.ime.view()));
        CHECK(r.ime.window()->state() == control.ime.window()->state());
    }
    {
        CapacityRig r("capacity_pending_reselection"); r.full(); r.ready();
        r.ime.window()->choose(0, r.frame); r.ready();
        r.key(80); while (r.ime.rawCursor()>4) r.key(80);
        while (r.ime.rawCursor()<4) r.key(79);
        r.ime.window()->query(); r.drain(); r.key(81);
        const CapacitySnapshot before(r); r.letters("a");
        CHECK(before.same(CapacitySnapshot(r)));
        r.key(44); r.ready(); CHECK(r.ime.selected() == "完蛋");
        r.finish(44); CHECK(r.ime.raw().empty() && r.accepted == 1);
    }
    {
        CapacityRig r("capacity_edit_recovery"); r.full(); r.ready(); r.letters("a");
        r.key(42); CHECK(r.ime.raw().size() == 63 && r.results.back() == RawEdited);
        r.letters("j"); r.ready(); r.key(80); // Raw cursor inside final syllable.
        const auto pos = r.ime.rawCursor(); const auto letter = r.ime.raw()[pos];
        r.key(76); r.letters(std::string(1, letter));
        while (r.ime.rawCursor() > 4) r.key(80);
        r.key(81); r.key(44); r.ready(); r.finish(44);
        std::string expected; for (unsigned i = 0; i < 16; ++i) expected += "完蛋";
        CHECK(std::string_view(r.app.document.text()) == expected && r.accepted == 1);
    }
    for (unsigned invalidation = 0; invalidation < 4; ++invalidation) {
        CapacityRig r("capacity_invalidation_" + std::to_string(invalidation)); r.full(); r.ready();
        const auto old = r.ime.composition()->operation();
        s3hid::Event stale{1, ++r.now, 41, 0, true, 0, r.frame, r.ime.attemptSerial()};
        stale.ime_context = r.ime.ingressContext(); stale.resource_owner = r.ime.resourceOwner();
        r.letters("a");
        if (invalidation == 0) { r.key(41); CHECK(r.ime.raw().size()==64&&r.ime.rawEditing()); r.ime.interrupt(); /* explicit internal discard reset */ r.letters("ui"); r.ime.event(stale); r.drain(); r.ready(); CHECK(r.ime.raw() == "ui"); CHECK(r.ime.composition()->cancel(old) == hanime_extension::Status::Stale); }
        if (invalidation == 1) { r.key(44); CHECK(r.app.commitText("body", r.app.editStamp())); r.letters("a"); r.ready(); CHECK(r.app.document.text() == "body" && r.ime.raw().empty()); }
        if (invalidation == 2) { ++r.resources.mount()->epoch; r.letters("a"); CHECK(r.results.back() == ResourceFailure && !r.ime.composition()); }
        if (invalidation == 3) { r.key(44, 1); r.ready(); CHECK(r.ime.raw().empty() && r.resources.rules() == hanime_extension::SchemeRules::FullPinyin); }
        CHECK(r.accepted == 0);
    }
    {
        CapacityRig r("capacity_empty_edit"); r.full(); r.ready(); r.key(80);
        while (r.ime.rawCursor()) r.key(80);
        const CapacitySnapshot before(r); r.key(42);
        CHECK(before.same(CapacitySnapshot(r)) && r.results.back() == EditNoChange);
    }
    {
        CapacityRig r("capacity_real_busy"); r.key(57); r.ready(); r.letters("ui"); r.ready();
        r.ime.window()->choose(0, r.frame); r.ready();
        const auto before = r.ime.composition()->snapshot().revision;
        const auto attempt = r.ime.submission()->prepare();
        CHECK(attempt.core.status == hanime::CompositionStatus::Preparing || attempt.core.status == hanime::CompositionStatus::Ready);
        r.letters("a");
        CHECK(r.results.back() == EditBusy && r.ime.raw() == "ui");
        CHECK(r.ime.composition()->snapshot().revision == before && r.app.document.text().empty());
        r.ime.submission()->cancel(attempt.transaction);
    }
    for (uint8_t key : {uint8_t(44), uint8_t(30)}) {
        CapacityRig r("capacity_successor_" + std::to_string(key)); r.full();
        for (unsigned i = 0; i < 15; ++i) {
            r.ready();
            r.ime.window()->query(r.ime.composition()->snapshot().focus + 4); r.drain(); r.ready();
            r.key(key); r.ready();
        }
        CHECK(r.ime.composition()->snapshot().member_count == 15);
        r.ime.window()->query(64); r.drain();
        r.letters("a"); r.key(key); r.letters("ui"); r.key(key); r.ready();
        std::string expected; for (unsigned i = 0; i < 16; ++i) expected += "完蛋"; expected += "是";
        CHECK(std::string_view(r.app.document.text()) == expected && r.ime.raw().empty() && r.accepted == 2);
    }
    std::cout << "{\"adjacent_cases\":\"PASS\",\"scope\":\"admission arithmetic/loading/navigation/pending reselection/edit recovery/invalidation/real Busy/ordered successors\"}\n";
}

int main(int argc, char** argv) {
    try {
        const bool observe = argc == 2 && std::string(argv[1]) == "--observe-baseline";
        for (const std::string timing : {"query", "retained", "published", "reselection"})
            for (uint8_t choice : {uint8_t(44), uint8_t(30)})
                for (bool reject : {false, true})
                    for (bool py : {false, true}) capacityCase(timing, choice, reject, observe, py);
        if (!observe) adjacentCases();
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
