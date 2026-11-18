#pragma once
#include "s3_vfs.hpp"
namespace draftor::board {
inline constexpr const char* NamespacePath="/WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112";
inline constexpr const char* NamespaceMarker="/WAVESHARE_R7_L1_AREV1_DOC_TEST_20261112/.draftor-namespace";
inline constexpr const char NamespaceIdentity[]="DRAFTOR_WS_R7_L1_DOC_TEST_V1\nlogical_root=/HANIME_S3_TEST/v01\n";
// Called only after mount, before any document owner. An existing unknown or
// damaged directory is never cleared, adopted, or redirected to the old root.
enum class NamespaceResult { Ready, Invalid, OutOfMemory };
// Includes construction of the VFS root. Failure never admits document owners;
// partially created or unknown directories remain untouched for review.
NamespaceResult prepareNamespace(const char* cardRoot) noexcept;
}
