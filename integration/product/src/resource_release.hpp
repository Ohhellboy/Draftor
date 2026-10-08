#pragma once
#include "hanime/verified_resource.hpp"
#include "hanime_extension/extension.hpp"
#include "wubi_auto_safe.hpp"

namespace ime_device {
// Borrowed immutable trusted specs outlive Resources. Device values come from
// the release header compiled into firmware, never from a same-card manifest.
struct DirectRelease {
    const char* scheme_id;
    const hanime::ResourceReleaseSpec* scheme;
    const hanime::ResourceReleaseSpec* order;
    const char* code_file;
    const char* proof_file;
    const char* order_file;
    const char* order_proof;
    const WubiAutoSafe* auto_safe = nullptr;
};

struct ReleasePins {
    const hanime::ResourceReleaseSpec* shared;
    const hanime::ResourceReleaseSpec* xiaohe;
    const hanime::ResourceReleaseSpec* full_pinyin;
    const hanime::ResourceReleaseSpec* xiaohe_base = nullptr;
    const hanime::ResourceReleaseSpec* full_pinyin_base = nullptr;
    const DirectRelease* direct = nullptr;

    const hanime::ResourceReleaseSpec* order(hanime_extension::SchemeRules rules) const {
        if (rules == hanime_extension::SchemeRules::DirectExact) {
            return direct ? direct->order : nullptr;
        }
        return rules == hanime_extension::SchemeRules::R1Xiaohe ? xiaohe_base : full_pinyin_base;
    }

    const hanime::ResourceReleaseSpec& scheme(hanime_extension::SchemeRules rules) const {
        if (rules == hanime_extension::SchemeRules::DirectExact) {
            return *direct->scheme;
        }
        return *(rules == hanime_extension::SchemeRules::R1Xiaohe ? xiaohe : full_pinyin);
    }

    bool supports(hanime_extension::SchemeRules rules) const {
        using Rules = hanime_extension::SchemeRules;
        if (!shared) {
            return false;
        }
        if (rules == Rules::FullPinyin) {
            return full_pinyin;
        }
        if (rules == Rules::R1Xiaohe) {
            return xiaohe;
        }
        return rules == Rules::DirectExact && direct && direct->scheme_id && *direct->scheme_id &&
               direct->scheme && direct->order && direct->code_file && direct->proof_file &&
               direct->order_file && direct->order_proof;
    }
};

ReleasePins productionPins();
} // namespace ime_device
