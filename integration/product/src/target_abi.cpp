#include "device_ime.hpp"
#include "chinese_font.hpp"
static_assert(__cplusplus==201703L,"This adapter must be compiled as C++17");
extern "C" __attribute__((used)) const uint32_t s3_ime_memory_abi[]={sizeof(ime_device::Resources),sizeof(ime_device::DeviceIme),sizeof(ime_device::CandidateWindow),sizeof(ime_device::VisibleItem),sizeof(ime_device::CandidateRow),sizeof(ime_device::CandidateView),sizeof(hanime_extension::Selection),sizeof(hanime_extension::SubmissionHandle),ime_device::ChineseFont::Bytes,ime_device::OutputProfile,ime_device::SharedDataKiB,ime_device::SharedPages,ime_device::OutputEntries,ime_device::OutputTextBytes,ime_device::OutputFormBytes,ime_device::SharedAnchorSlots};
extern "C" __attribute__((used)) const uint32_t s3_ime_compile_abi[]={__cplusplus,__EXCEPTIONS,__GNUC__,__GNUC_MINOR__,sizeof(std::string)};

extern "C" __attribute__((used)) const uint32_t s3_next_stage_abi[]={sizeof(s3app::App),sizeof(ime_device::RawViewport),sizeof(s3app::App::FileTiming)};

// LC1 assembly receipt; constants use the same factories as DeviceIme.
namespace { constexpr auto lc1_xh=ime_device::compositionConfig(hanime_extension::SchemeRules::R1Xiaohe);
constexpr auto lc1_py=ime_device::compositionConfig(hanime_extension::SchemeRules::FullPinyin); }
extern "C" __attribute__((used)) const uint32_t s3_lc1_capacity_abi[]={
 lc1_xh.members,lc1_py.members,ime_device::submissionConfig(lc1_xh).max_members,
 ime_device::submissionConfig(lc1_py).max_members,lc1_xh.raw_bytes,lc1_xh.word_bytes,
 lc1_xh.name_bytes,lc1_xh.text_bytes,lc1_xh.request_bytes,sizeof(hanime_extension::Member),
 sizeof(hanime::CompositionMember),sizeof(hanime::PreparedCompositionMember),sizeof(size_t)};

extern "C" __attribute__((used)) const uint32_t s3_mem1_abi[]={sizeof(hanime_extension::StorageBackend),sizeof(ime_device::MemberStorageRecord),sizeof(ime_device::MemberStorage),32*(3*sizeof(hanime_extension::Member)+sizeof(hanime::CompositionMember)),64*(3*sizeof(hanime_extension::Member)+sizeof(hanime::CompositionMember))};
