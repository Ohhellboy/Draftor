#include "independent_ime_bridge.hpp"
#include <type_traits>
static_assert(std::is_base_of<hanime::TextCommitSink,s3host::IndependentTextCommitSink>::value);
static_assert(std::is_base_of<hanime::ResourceReader,s3host::IndependentResourceReader>::value);
