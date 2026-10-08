#include "wubi_product.hpp"
#include "device_ime.hpp"
#include "hanime_wubi/wubi.hpp"

namespace ime_device {

RawAdmission admitWubiEdit(std::string_view raw, size_t begin, size_t end,
                           std::string_view text) {
    using namespace hanime::wubi;
    if (begin > end || end > raw.size()) {
        return RawAdmission::Invalid;
    }
    if (begin == end && text.empty()) {
        return RawAdmission::NoChange;
    }

    // Validate replacement characters before checking the resulting capacity:
    // z stays InvalidCode even when the original code already has four keys.
    for (char character : text) {
        Wubi86Decoder character_decoder;
        if (character_decoder.append(character) != EditStatus::Changed) {
            return RawAdmission::Invalid;
        }
    }

    Wubi86Decoder staged;
    for (auto part : {raw.substr(0, begin), text, raw.substr(end)}) {
        for (char character : part) {
            const auto result = staged.append(character);
            if (result == EditStatus::Full) {
                return RawAdmission::Capacity;
            }
            if (result != EditStatus::Changed) {
                return RawAdmission::Invalid;
            }
        }
    }
    return RawAdmission::Edit;
}

bool wubiFullCode(std::string_view raw) {
    hanime::wubi::Wubi86Decoder decoder;
    for (char character : raw) {
        if (decoder.append(character) != hanime::wubi::EditStatus::Changed) {
            return false;
        }
    }
    return decoder.profile().auto_unique_full_code && decoder.plan().full_code;
}

void DeviceIme::armAuto(const s3hid::Event& event) {
    if (!auto_request_) {
        return;
    }
    *auto_request_ = {
        event,
        composition_ ? composition_->raw_input().stamp : hanime_extension::InputStamp{},
        current_action_,
        !raw_edit_ && wubiFullCode(raw()),
        bool(composition_)
    };
}

void DeviceIme::serviceAuto() {
    if (!auto_request_ || !auto_request_->pending) {
        return;
    }
    auto& request = *auto_request_;
    if (!wubi() || !composition_ || !window_ || !request.bound ||
        !composition_->input_is_current(request.stamp) || !has_target_ || !sink_.current() ||
        request.event.epoch != epoch_ || flow_.stage != FlowStage::Idle ||
        following_count_ || raw_edit_ || arrow_count_) {
        request.pending = false;
        return;
    }
    if (window_->busy()) {
        return;
    }

    // Each admitted full code proposes one request. A failed or non-unique
    // result consumes it; service never turns it into an automatic retry.
    request.pending = false;
    const auto page = composition_->page();
    const auto& view = window_->view();
    if (window_->state() != WindowState::Ready || !wubiFullCode(raw()) || !resources_.autoSafe(raw()) ||
        page.begin != 0 || page.end != raw().size() || page.size != 1 || page.has_more ||
        view.row != 0 || view.count != 1 || view.rows[0].index != 0 || view.rows[0].count != 1) {
        return;
    }
    const auto& item = view.rows[0].items[0];
    if (!item.bytes || item.page != 0 || item.slot != 0 ||
        item.key != page.items[0].key || item.ref != page.items[0].ref || !targetMatches(item)) {
        return;
    }

    // Keep the ORIGINAL fourth-key receipt. CandidateWindow captures a real
    // Selection, then the existing flow verifies its member and submits through
    // CompositionCommitter/EditorSink. The decoder never commits document text.
    const auto saved_action = current_action_;
    current_action_ = request.action;
    if (window_->chooseDefault(request.action) == ChoiceResult::Captured) {
        startFlow(request.event, FlowStage::Selecting);
        flowCaptured(true);
    }
    current_action_ = saved_action;
}

} // namespace ime_device
