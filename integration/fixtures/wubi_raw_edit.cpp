// Reuse the unchanged synthetic resources and real App/EditorSink harness.
// This executable adds product-event tests; the original executable still runs
// its original inputs and assertions with fixture(false).
#define main original_wubi_product_main
#include "wubi_product.cpp"
#undef main
#include "product_input.hpp"

namespace {
constexpr uint8_t Left = 0x50, Right = 0x4f, Down = 0x51, Up = 0x52;
constexpr uint8_t Backspace = 0x2a, Delete = 0x4c, Escape = 0x29, Space = 0x2c, F4 = 0x3d;
#if DRAFTOR_TELEMETRY_PROFILE
// The existing observer binds once per process. Its storage must outlive App
// and DeviceIme, including their destruction; each CTest case gets a process.
std::vector<std::max_align_t> observer_storage;
#endif

std::string hex(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : value) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}

struct EventRig : WubiRig {
    s3app::ProductInput product;
    unsigned record = 0;
#if DRAFTOR_TELEMETRY_PROFILE
    std::unique_ptr<WubiWire> wire;
#endif
    explicit EventRig(const fs::path& root, unsigned id) : WubiRig(root, id) {
        if (!app.horizontalActive()) {
            key(F4);
            wait();
        }
        CHECK(app.horizontalActive());
#if DRAFTOR_TELEMETRY_PROFILE
        const auto output = fs::current_path().parent_path() / ("wubi_raw_" + std::to_string(id) + ".bin");
        wire = std::make_unique<WubiWire>(output);
        observer_storage.resize((ime_device::behavior::storageBytes() + sizeof(std::max_align_t) - 1) /
                                sizeof(std::max_align_t));
        ime_device::behavior::bind(observer_storage.data(), 0x5755424952415700ULL + id, app, &ime);
        drain();
#endif
    }

    ~EventRig() {
#if DRAFTOR_TELEMETRY_PROFILE
        ime_device::behavior::stop();
        drain();
#endif
    }

    void drain() {
#if DRAFTOR_TELEMETRY_PROFILE
        if (wire) {
            for (unsigned count = 0; count < 40; ++count) {
                ime_device::behavior::service(*wire, false);
            }
        }
#endif
    }

    void observe(const char* label, const s3hid::Event* event = nullptr) {
        const auto snapshot = ime.composition()->snapshot();
        const auto page = ime.composition()->page();
        const std::string_view body = app.document.text();
        const bool completeBody = body.size() <= 64;
        const bool capacityBody = body.size() == 65535 && body.find_first_not_of('q') == std::string_view::npos;
        std::cout << "TRACE {\"case\":\"" << path.filename().string()
                  << "\",\"step\":" << ++record << ",\"label\":\"" << label
                  << "\",\"raw\":\"" << ime.raw() << "\",\"cursor\":" << ime.rawCursor()
                  << ",\"raw_edit\":" << ime.rawEditing() << ",\"horizontal\":" << app.horizontalActive()
                  << ",\"input\":" << snapshot.input_id << ",\"revision\":" << snapshot.revision
                  << ",\"query\":" << ime.window()->querySerial() << ",\"frame\":" << frame
                  << ",\"shown_frame\":" << ime.window()->onScreenGeneration()
                  << ",\"members\":" << snapshot.member_count << ",\"focus\":" << snapshot.focus
                  << ",\"attempt\":" << ime.attemptSerial() << ",\"receipt\":" << unsigned(ime.receipt().status)
                  << ",\"range_begin\":" << page.begin << ",\"range_end\":" << page.end
                  << ",\"window_state\":" << unsigned(ime.window()->state())
                  << ",\"document_revision\":" << app.document.textRevision()
                  << ",\"body_bytes\":" << body.size()
                  << ",\"body_hex\":\"" << hex(body.substr(0, 64))
                  << "\",\"body_hex_complete\":" << completeBody
                  << ",\"capacity_body_exact\":" << capacityBody << ",\"undo\":" << app.editor.undoCount();
        if (event) {
            std::cout << ",\"usage\":" << unsigned(event->usage) << ",\"down\":" << event->down
                      << ",\"original_frame\":" << event->ime_frame << ",\"original_attempt\":" << event->ime_attempt
                      << ",\"original_context\":" << event->ime_context;
        }
        std::cout << "}\n";
    }

    void deliver(const s3hid::Event& event) {
        if (!product.layout(app, event) && !product.ime(app, &ime, event)) {
            app.key(event);
        }
        drain();
        observe("event", &event);
    }
    void key(uint8_t usage) {
        deliver(freshEvent(usage, true));
        deliver(freshEvent(usage, false));
    }
    void letters(std::string_view value) {
        for (char letter : value) {
            key(uint8_t(letter - 'a' + 4));
        }
    }
    void wait() {
        // Fixed synthetic resources need only a small bounded number of slices.
        for (unsigned count = 0; count < 100000; ++count) {
            step();
            drain();
            CHECK(resources.state() != ResourceState::Error);
            if (count > 1 && !ime.hasWork() && !app.persistence.backgroundSaveActive() && !app.commandBarrier()) {
                frame = ime.framePresented();
                observe("settled");
                zeroPlanner();
                return;
            }
        }
        observe("wait_limit");
        throw std::runtime_error("bounded product-event wait exceeded: " + std::string(ime.notice()));
    }
    void previous() { key(app.horizontalActive() ? Left : Up); }
    void next() { key(app.horizontalActive() ? Right : Down); }
    void enterRaw(bool vertical) {
        key(Left); // Existing H entry starts one byte before the end.
        CHECK(ime.rawEditing());
        if (vertical) {
            const auto stamp = ime.composition()->raw_input().stamp;
            key(F4);
            wait();
            CHECK(!app.horizontalActive());
            CHECK(ime.rawEditing() && ime.composition()->input_is_current(stamp));
        }
    }
    void completeCandidate() {
        const auto page = ime.composition()->page();
        CHECK(page.begin == 0 && page.end == ime.raw().size());
        CHECK(ime.candidateAvailable());
        CHECK(ime.presentationNavigation().span_count == 0);
        CHECK(ime.composition()->snapshot().member_count == 0);
    }
    void returnProbe(bool requireReturn = false) {
        completeCandidate();
        const auto stamp = ime.composition()->raw_input().stamp;
        const auto query = ime.window()->querySerial();
        key(app.horizontalActive() ? Down : Left); // Existing RawCandidates mapping.
        wait();
        if (requireReturn) {
            CHECK(!ime.rawEditing());
        }
        // build236 resolves the historical OPEN: return uses the full current code.
        CHECK(!ime.rawEditing() && ime.editTarget().valid() &&
              ime.editTarget().begin == 0 && ime.editTarget().end == ime.raw().size());
        CHECK(ime.composition()->input_is_current(stamp) && ime.window()->querySerial() == query);
        CHECK(app.document.text().empty());
        std::cout << "PASS full-code Down/Left re-entry: " << ime.notice() << '\n';
    }
    void chooseOnce(uint8_t usage) {
        const auto undo = app.editor.undoCount();
        const auto revision = app.document.textRevision();
        key(usage); // Existing displayed-candidate selection is allowed in raw mode.
        wait();
        CHECK(ime.raw().empty() && ime.receipt().status == CS::Accepted);
        CHECK(app.document.text() == "同" && app.editor.undoCount() == undo + 1);
        CHECK(app.document.textRevision() == revision + 1);
        for (unsigned i = 0; i < 40; ++i) {
            step();
        }
        CHECK(app.editor.undoCount() == undo + 1 && app.document.textRevision() == revision + 1);
    }
};

void insertion(const fs::path& root, bool vertical, bool requireReturn) {
    EventRig rig(root, vertical ? 102 : 101);
    rig.letters("aba");
    rig.wait();
    CHECK(rig.ime.view()->rows[0].count == 2);
    const auto oldPress = rig.freshEvent(30, true);
    const auto oldRelease = rig.freshEvent(30, false);

    rig.enterRaw(vertical);
    CHECK(rig.ime.rawCursor() == 2);
    rig.letters("b");
    rig.wait();
    CHECK(rig.ime.raw() == "abba" && rig.ime.rawCursor() == 3);
    rig.completeCandidate();
    const auto current = rig.ime.composition()->raw_input().stamp;
    rig.deliver(oldPress);
    rig.deliver(oldRelease);
    rig.wait();
    CHECK(rig.ime.composition()->input_is_current(current) && rig.app.document.text().empty());
    rig.returnProbe(requireReturn);
    rig.chooseOnce(Space);
}

void deletionAndRecovery(const fs::path& root, bool vertical) {
    EventRig rig(root, vertical ? 104 : 103);
    rig.letters("abba");
    rig.wait();
    rig.enterRaw(vertical);
    rig.previous();
    CHECK(rig.ime.rawCursor() == 2);
    rig.key(Backspace);
    rig.wait();
    CHECK(rig.ime.raw() == "aba" && rig.ime.rawCursor() == 1);
    rig.key(Delete);
    rig.wait();
    CHECK(rig.ime.raw() == "aa" && rig.ime.rawCursor() == 1);
    CHECK(rig.ime.window()->state() == WindowState::NoResults);
    rig.letters("b");
    rig.wait();
    CHECK(rig.ime.raw() == "aba" && rig.ime.rawCursor() == 2);
    rig.letters("d");
    rig.wait();
    CHECK(rig.ime.raw() == "abda" && rig.ime.window()->state() == WindowState::NoResults);
    rig.key(Backspace);
    rig.wait();
    CHECK(rig.ime.raw() == "aba" && rig.ime.rawCursor() == 2);
    rig.returnProbe();
    rig.chooseOnce(31);
}

void rejectedAndBoundary(const fs::path& root, bool vertical) {
    EventRig rig(root, vertical ? 106 : 105);
    rig.letters("abba");
    rig.wait();
    rig.enterRaw(vertical);
    rig.previous();
    const auto unchanged = rig.ime.composition()->raw_input().stamp;
    const auto query = rig.ime.window()->querySerial();
    const auto generation = rig.ime.view()->generation;
    const auto attempt = rig.ime.attemptSerial();
    for (const auto* rejected : {"a", "z"}) {
        rig.letters(rejected);
        CHECK(rig.ime.raw() == "abba" && rig.ime.rawCursor() == 2);
        CHECK(rig.ime.composition()->input_is_current(unchanged));
        CHECK(rig.ime.window()->querySerial() == query && rig.ime.view()->generation == generation);
        CHECK(rig.ime.attemptSerial() == attempt && rig.app.document.text().empty());
    }
    rig.previous();
    rig.previous();
    CHECK(rig.ime.rawCursor() == 0);
    rig.previous();
    rig.key(Backspace);
    rig.letters("z");
    CHECK(rig.ime.rawCursor() == 0 && rig.ime.composition()->input_is_current(unchanged));
    for (unsigned i = 0; i < 5; ++i) {
        rig.next();
    }
    CHECK(rig.ime.rawCursor() == 4);
    rig.key(Delete);
    CHECK(rig.ime.composition()->input_is_current(unchanged) && rig.ime.raw() == "abba");
    rig.key(Escape);
    rig.wait();
    // build235 raw Esc cancels; another Esc must not resurrect composition.
    CHECK(!rig.ime.rawEditing() && rig.ime.raw().empty());
    rig.key(Escape);
    rig.wait();
    CHECK(rig.ime.raw().empty() && rig.app.document.text().empty() && rig.app.editor.undoCount() == 0);

    // Preserve a real nonzero attempt before an edit. Filling the real editor
    // establishes the existing capacity-refusal condition; it is setup, not HID.
    EditorSink fill(rig.app);
    while (rig.app.document.text().size() < 65535) {
        CHECK(fill.bind(0, 0));
        CHECK(fill.commit(std::string(std::min(size_t(8192), 65535 - rig.app.document.text().size()), 'q')));
    }
    rig.observe("capacity_setup");
    rig.letters("abba");
    rig.wait();
    rig.key(Space);
    rig.wait();
    CHECK(rig.ime.receipt().status == CS::RetryableNoEffect);
    const auto oldAttempt = rig.ime.attemptSerial();
    const auto oldCancel = rig.freshEvent(Escape, true);
    const auto oldCancelRelease = rig.freshEvent(Escape, false);
    const auto oldChoice = rig.freshEvent(Space, true);
    const auto oldChoiceRelease = rig.freshEvent(Space, false);

    rig.key(Backspace);
    rig.wait();
    CHECK(rig.ime.raw() == "abb" && rig.ime.receipt().status == CS::Cancelled);
    rig.key(Space);
    rig.wait();
    CHECK(rig.ime.receipt().status == CS::RetryableNoEffect && rig.ime.attemptSerial() > oldAttempt);
    const auto edited = rig.ime.composition()->raw_input().stamp;
    const auto newAttempt = rig.ime.attemptSerial();
    rig.deliver(oldCancel);
    rig.deliver(oldCancelRelease);
    rig.deliver(oldChoice);
    rig.deliver(oldChoiceRelease);
    rig.wait();
    CHECK(rig.ime.composition()->input_is_current(edited) && rig.ime.raw() == "abb");
    CHECK(rig.ime.attemptSerial() == newAttempt && std::string(rig.app.document.text()) == std::string(65535, 'q'));
    rig.key(Escape); CHECK(rig.ime.rawEditing()&&rig.ime.raw()=="abb"); rig.key(Escape);
    rig.wait();
    CHECK(rig.ime.raw().empty() && std::string(rig.app.document.text()) == std::string(65535, 'q'));
}

void pendingAutomatic(const fs::path& root) {
    EventRig rig(root, 107);
    rig.letters("abc");
    rig.wait();
    rig.letters("d"); // Original fourth-key automatic request is still pending.
    rig.enterRaw(false);
    rig.key(Backspace);
    rig.wait();
    CHECK(rig.ime.raw() == "abd" && rig.app.document.text().empty());
    rig.letters("c");
    rig.wait();
    CHECK(rig.ime.raw() == "abcd" && rig.app.document.text().empty());
    rig.completeCandidate();
    rig.key(Space);
    rig.wait();
    CHECK(rig.app.document.text() == "四" && rig.app.editor.undoCount() == 1);
}
}

int main(int argc, char** argv) {
    try {
        const bool requireReturn = argc == 2 && std::string_view(argv[1]) == "--require-return";
        const unsigned testCase = requireReturn || argc == 1 ? 101 : unsigned(std::stoul(argv[1]));
        auto name = (fs::current_path() / "wubi-raw-events-XXXXXX").string();
        CHECK(mkdtemp(name.data()));
        const auto parent = fs::current_path();
        fs::current_path(name);
        const auto root = fixture(true);
        switch (testCase) {
            case 101: insertion(root, false, requireReturn); break;
            case 102: insertion(root, true, requireReturn); break;
            case 103: deletionAndRecovery(root, false); break;
            case 104: deletionAndRecovery(root, true); break;
            case 105: rejectedAndBoundary(root, false); break;
            case 106: rejectedAndBoundary(root, true); break;
            case 107: pendingAutomatic(root); break;
            default: throw std::runtime_error("unknown product-event case");
        }
        CHECK(metrics.resource_handles == 0 && metrics.document_handles == 0);
        fs::current_path(parent);
        std::cout << "PASS tested event edits/whole-code query/Space+digit once/stale/cancel; "
                     "WUBI_RAW_RETURN_UI_OPEN; planner_storage_zero=1 count_begin_calls=0; "
                     "planner_call_absence_requires_separate_source_path_evidence; HOST_ONLY\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
