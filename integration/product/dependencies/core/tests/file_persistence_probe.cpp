#include "hanime/file_user_state_persistence.hpp"
#include "hanime/small_fixture.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace hanime;

class MemoryReader final : public ResourceReader {
 public:
  explicit MemoryReader(std::vector<std::uint8_t> bytes)
      : bytes_(std::move(bytes)) {}

  std::size_t size() const override { return bytes_.size(); }

  bool read(std::size_t offset, void* destination,
            std::size_t byte_count) const override {
    if (offset > bytes_.size() || byte_count > bytes_.size() - offset) {
      return false;
    }
    if (byte_count != 0U) {
      std::memcpy(destination, bytes_.data() + offset, byte_count);
    }
    return true;
  }

 private:
  std::vector<std::uint8_t> bytes_;
};

class AcceptingPresenter final : public CandidatePresenter {
 public:
  bool present(std::uint64_t,
               const std::vector<CandidateView>&) override {
    return true;
  }
};

class AcceptingCommitSink final : public TextCommitSink {
 public:
  bool commit(std::string_view) override { return true; }
};

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

BuiltResources build_fixture(bool reversed) {
  ResourceBuilder builder;
  BuildResult result = builder.build(fixture::make_small_input(reversed));
  require(result.ok, "fixture build failed: " + result.error);
  return std::move(result.resources);
}

void mount(MountedResourceSet* mounted, const BuiltResources& built) {
  std::vector<std::shared_ptr<const ResourceReader>> scheme_readers;
  for (const auto& [scheme_id, bytes] : built.scheme_bundles) {
    static_cast<void>(scheme_id);
    scheme_readers.push_back(std::make_shared<MemoryReader>(bytes));
  }
  std::string error;
  require(mounted->mount(std::make_shared<MemoryReader>(built.shared_bundle),
                         scheme_readers, &error),
          "mount failed: " + error);
}

void write_state(const std::filesystem::path& path) {
  std::error_code remove_error;
  std::filesystem::remove(path, remove_error);
  require(!remove_error, "could not remove stale probe file");

  const BuiltResources built = build_fixture(false);
  MountedResourceSet resources;
  mount(&resources, built);
  FileUserStatePersistence persistence(path);
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  AcceptingPresenter presenter;
  AcceptingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "full_pinyin");
  std::string error;
  require(session.query_complete_code("yuanxingji", &error), error);
  const CandidateView candidate = session.current_candidates().at(0);
  require(candidate.stable_key == fixture::kPrototype &&
              candidate.handle.runtime_ref == 1U,
          "build A did not expose the expected prototype identity");
  require(session.confirm(candidate.handle) ==
              ConfirmResult::CommittedPersisted,
          "file-backed commit was not persisted");
  require(std::filesystem::is_regular_file(path),
          "state file was not created");
  require(persistence.write(fixture::kRegistryNamespace, fixture::kSameTextA, 7) &&
              persistence.write(fixture::kRegistryNamespace, fixture::kSameTextB, 19),
          "equal-prefix complete keys could not be persisted separately");
  std::cout << "{\"event\":\"FILE_STATE_WRITE\",\"count\":1,"
               "\"runtime_ref\":"
            << candidate.handle.runtime_ref << "}\n";
}

void read_state_after_restart(const std::filesystem::path& path) {
  require(std::filesystem::is_regular_file(path),
          "state file from the writer process is missing");
  const BuiltResources built = build_fixture(true);
  MountedResourceSet resources;
  mount(&resources, built);
  FileUserStatePersistence persistence(path);
  UserStateStore state(fixture::kRegistryNamespace, persistence);
  AcceptingPresenter presenter;
  AcceptingCommitSink sink;
  ImeSession session(resources, state, presenter, sink, "xiaohe");
  std::string error;
  require(session.query_complete_code("yrxkji", &error), error);
  const CandidateView candidate = session.current_candidates().at(0);
  require(candidate.stable_key == fixture::kPrototype,
          "reordered build resolved another stable identity");
  require(candidate.handle.runtime_ref != 1U,
          "reordered build did not change the runtime reference");
  require(candidate.selection_count == 1U,
          "fresh process did not reload the persisted count by stable key");
  std::uint32_t count_a = 0, count_b = 0;
  require(state.selection_count(fixture::kSameTextA, &count_a) &&
              state.selection_count(fixture::kSameTextB, &count_b) &&
              count_a == 7 && count_b == 19,
          "equal-prefix full keys collided across process restart");
  std::cout << "{\"event\":\"FULL_KEY_RESTART\",\"prefix_bytes_equal\":8,\"count_a\":"
            << count_a << ",\"count_b\":" << count_b << ",\"key_a\":\""
            << format_slk1(fixture::kSameTextA) << "\",\"key_b\":\""
            << format_slk1(fixture::kSameTextB) << "\"}\n";
  std::cout << "{\"event\":\"FILE_STATE_RESTART_READ\",\"count\":"
            << candidate.selection_count << ",\"runtime_ref\":"
            << candidate.handle.runtime_ref << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 3) {
      throw std::runtime_error("usage: probe <write|read> <state-file>");
    }
    const std::string mode = argv[1];
    const std::filesystem::path path = argv[2];
    if (mode == "write") {
      write_state(path);
    } else if (mode == "read") {
      read_state_after_restart(path);
    } else {
      throw std::runtime_error("unknown probe mode");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "{\"event\":\"FILE_STATE_PROBE_FAIL\",\"message\":\""
              << error.what() << "\"}\n";
    return 1;
  }
}
