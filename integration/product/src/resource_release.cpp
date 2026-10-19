#include "resource_release.hpp"
#include "release_pin.hpp"
#include "wubi_auto_safe_v1.hpp"
#if defined(DRAFTOR_BLOCK_BACKEND) && DRAFTOR_BLOCK_BACKEND
#include "block_release_pin.hpp"
#endif
namespace ime_device {
namespace {
const DirectRelease wubi_v1{"wubi86",&wubi_release_v1::wubi86,&wubi_release_v1::wubi86_base,
    "wubi86.hnm","wubi86.hnv","wubi86_base.hbo","wubi86_base.hnv",&wubi_release_v1::auto_safe};
}
ReleasePins productionPins(){
#if defined(DRAFTOR_BLOCK_BACKEND) && DRAFTOR_BLOCK_BACKEND
return {&hanime_release::shared,&hanime_release::xiaohe,&hanime_release::full_pinyin,&draftor_block_release::block_xiaohe,&draftor_block_release::block_full_pinyin,&wubi_v1};
#else
return {&hanime_release::shared,&hanime_release::xiaohe,&hanime_release::full_pinyin,&hanime_release::xiaohe_base,&hanime_release::full_pinyin_base,&wubi_v1};
#endif
}
}
