#include "core_memory.hpp"
#pragma once

#include "src/hanime_document/storage_port.hpp"

#include "s3_vfs.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>



namespace hanime::documents {

struct SdPathStat {
  Status status = Status::NotFound;
  std::size_t size = 0U;
  bool is_directory = false;
};

// Synchronous Arduino SD.h adapter for the DocumentSystem storage boundary.
// The product owns SD.begin()/SD.end(), SPI arbitration and serialization.
class ArduinoSdStoragePort final : public StoragePort {
 public:
  inline static constexpr std::string_view kRoot = "/HANIME_S3_TEST/v01";
  inline static constexpr std::size_t kMaxDeviceListPaths = 32U;
  inline static constexpr std::size_t kVerifyBufferBytes = 512U;


  explicit ArduinoSdStoragePort(s3fs::Fs& sd) noexcept;
  bool documentEnumerationSupported() const noexcept override { return true; }
  Status visitDocumentIds(void*, Status(*)(void*, const DocumentId&)) override;

  // Call after every successful SD mount, including a same-card remount.
  // Each call creates a new non-zero epoch. Call noteUnmounted() before
  // SD.end() when the product detects removal or deliberately unmounts.
  void noteMounted() noexcept;
  void noteUnmounted() noexcept;

  MediumSession mediumSession() const noexcept override;
  ReadResult read(std::string_view path, std::size_t max_bytes) override;
  ReadResult readPrefix(std::string_view path,
                        std::size_t max_bytes) override;
  bool writesTruncate() const noexcept override { return true; }
  Status write(std::string_view path, std::string_view bytes) override;
  Status writeParts(std::string_view path, const std::string_view* parts,
                    std::size_t part_count) override;
  Status verifyParts(std::string_view path,
                     const std::string_view* parts,
                     std::size_t part_count) override;
  Status remove(std::string_view path) override;
  ListResult list(std::string_view prefix, std::string_view after,
                  std::size_t max_paths) override;

  bool cooperativeSupported() const noexcept override { return true; }
  CooperativeIoResult cooperativeExists(std::string_view path) override;
  CooperativeIoResult cooperativeMkdir(std::string_view path) override;
  CooperativeIoResult cooperativeRemove(std::string_view path) override;
  CooperativeIoResult cooperativeOpen(std::string_view path,
                                       CooperativeOpenMode mode) override;
  CooperativeIoResult cooperativeIsDirectory() override;
  CooperativeIoResult cooperativeSize() override;
  CooperativeIoResult cooperativeRead(std::uint8_t* out,
                                       std::size_t count) override;
  CooperativeIoResult cooperativeWrite(const std::uint8_t* bytes,
                                        std::size_t count) override;
  CooperativeIoResult cooperativeFlush() override;
  CooperativeIoResult cooperativeClose() override;

  // Product-side primitives used when preparing the namespace or publishing
  // a temporary file. They remain confined to kRoot.
  SdPathStat stat(std::string_view path);
  Status mkdirs(std::string_view path);
  Status rename(std::string_view from, std::string_view to);

 private:
  ReadResult readImpl(std::string_view path, std::size_t max_bytes,
                      bool prefix_only);
  Status mkdirsImpl(std::string_view path, std::uint64_t operation_epoch);
  Status collectDirectory(const draftor::memory::String& directory,
                          std::string_view prefix,
                          std::string_view after,
                          std::size_t keep_paths,
                          std::size_t depth,
                          std::uint64_t operation_epoch,
                          draftor::memory::Vector<draftor::memory::String>* paths);

  static bool isNamespacePath(std::string_view path,
                              bool allow_trailing_slash) noexcept;
  static bool isReadablePath(std::string_view path) noexcept;
  static bool isLegacyReadPath(std::string_view path) noexcept;
  Status sessionStatus(std::uint64_t operation_epoch) const noexcept;
  void advanceEpoch() noexcept;

  s3fs::Fs* sd_ = nullptr;
  bool available_ = false;
  std::uint64_t mount_epoch_ = 0U;
  s3fs::File cooperative_file_{};
  std::uint64_t cooperative_file_epoch_ = 0U;
  CooperativeOpenMode cooperative_mode_ = CooperativeOpenMode::Read;
};

}  // namespace hanime::documents
