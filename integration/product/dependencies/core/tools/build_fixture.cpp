#include "hanime/core.hpp"
#include "hanime/small_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool write_binary(const std::filesystem::path& path,
                  const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

bool write_text(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output << text;
  return output.good();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: hanime_build_fixture OUTPUT_DIRECTORY\n";
    return 2;
  }
  const std::filesystem::path output_directory(argv[1]);
  std::error_code filesystem_error;
  std::filesystem::create_directories(output_directory, filesystem_error);
  if (filesystem_error) {
    std::cerr << "could not create output directory: "
              << filesystem_error.message() << '\n';
    return 3;
  }

  const hanime::BuildResult result = hanime::ResourceBuilder().build(
      hanime::fixture::make_small_input(false));
  if (!result.ok) {
    std::cerr << "fixture build failed: " << result.error << '\n';
    return 4;
  }

  if (!write_binary(output_directory / "shared.hns",
                    result.resources.shared_bundle)) {
    std::cerr << "could not write shared bundle\n";
    return 5;
  }
  for (const auto& [scheme_id, bytes] : result.resources.scheme_bundles) {
    if (!write_binary(output_directory / (scheme_id + ".hnm"), bytes)) {
      std::cerr << "could not write scheme bundle: " << scheme_id << '\n';
      return 6;
    }
  }
  if (!write_text(output_directory / "manifest.json",
                  result.resources.manifest.to_json())) {
    std::cerr << "could not write manifest\n";
    return 7;
  }

  std::cout << result.resources.manifest.to_json();
  return 0;
}
