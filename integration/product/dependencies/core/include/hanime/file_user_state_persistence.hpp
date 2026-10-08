#pragma once

#include "hanime/core.hpp"

#include <filesystem>

namespace hanime {

// A minimal Host adapter used to verify persistence across process restarts.
// The portable core remains unaware of files and only depends on
// UserStatePersistence. This adapter deliberately makes no crash-consistency
// or power-loss guarantee.
class FileUserStatePersistence final : public UserStatePersistence {
 public:
  explicit FileUserStatePersistence(std::filesystem::path path);

  PersistenceLoadResult load(RegistryNamespaceId registry_namespace,
                             StableLexemeKey stable_key,
                             std::uint32_t* selection_count) override;
  bool write(RegistryNamespaceId registry_namespace,
             StableLexemeKey stable_key,
             std::uint32_t selection_count) override;

  const std::filesystem::path& path() const;

 private:
  std::filesystem::path path_;
};

}  // namespace hanime
