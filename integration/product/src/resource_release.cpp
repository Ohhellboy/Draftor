#include "resource_release.hpp"
#include "release_pin.hpp"
#include "wubi_auto_safe_v1.hpp"
namespace ime_device {
namespace {
const DirectRelease wubi_v1{"wubi86",&wubi_release_v1::wubi86,&wubi_release_v1::wubi86_base,
    "wubi86.hnm","wubi86.hnv","wubi86_base.hbo","wubi86_base.hnv",&wubi_release_v1::auto_safe};
}
ReleasePins productionPins(){return {&hanime_release::shared,&hanime_release::xiaohe,&hanime_release::full_pinyin,&hanime_release::xiaohe_base,&hanime_release::full_pinyin_base,&wubi_v1};}
}
