#include "hanime/file_user_state_persistence.hpp"

#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace hanime {
namespace {
constexpr const char* kFileHeader = "HANIME_USER_STATE_V2";
using StateMap = std::map<StableLexemeKey, std::uint32_t>;
enum class ReadFileResult { Loaded, Missing, Incompatible, Error };

bool unsigned_number(const std::string& s, std::uint64_t* value) {
  if (s.empty()) return false;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), *value);
  return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}
ReadFileResult read_file(const std::filesystem::path& path,
                         RegistryNamespaceId expected_namespace, StateMap* state) {
  state->clear();
  std::ifstream input(path);
  if (!input.is_open()) {
    std::error_code e; const bool exists = std::filesystem::exists(path, e);
    return !e && !exists ? ReadFileResult::Missing : ReadFileResult::Error;
  }
  std::string line;
  if (!std::getline(input, line)) return ReadFileResult::Error;
  std::istringstream header(line);
  std::string magic, ns_text, format, extra;
  if (!(header >> magic) || magic != kFileHeader) return ReadFileResult::Incompatible;
  std::uint64_t ns = 0;
  if (!(header >> ns_text >> format) || (header >> extra) || !unsigned_number(ns_text, &ns))
    return ReadFileResult::Error;
  if (!ns || ns != expected_namespace || format != kStableKeyFormatName)
    return ReadFileResult::Incompatible;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string key_text, count_text;
    StableLexemeKey key; std::uint64_t count = 0;
    if (!(row >> key_text >> count_text) || (row >> extra) ||
        !parse_slk1("SLK1:" + key_text, &key) || key.empty() ||
        !unsigned_number(count_text, &count) || count > std::numeric_limits<std::uint32_t>::max() ||
        !state->emplace(key, static_cast<std::uint32_t>(count)).second)
      return ReadFileResult::Error;
  }
  return input.eof() && !input.bad() ? ReadFileResult::Loaded : ReadFileResult::Error;
}
bool write_file(const std::filesystem::path& path, RegistryNamespaceId ns, const StateMap& state) {
  std::ofstream output(path, std::ios::trunc);
  if (!output.is_open()) return false;
  output << kFileHeader << ' ' << ns << ' ' << kStableKeyFormatName << '\n';
  for (const auto& [key, count] : state)
    output << format_slk1(key).substr(5) << ' ' << count << '\n';
  output.flush();
  return output.good();
}
}  // namespace

FileUserStatePersistence::FileUserStatePersistence(std::filesystem::path path)
    : path_(std::move(path)) {}
PersistenceLoadResult FileUserStatePersistence::load(RegistryNamespaceId ns,
    StableLexemeKey key, std::uint32_t* count) {
  if (!ns || key.empty() || !count) return PersistenceLoadResult::Error;
  StateMap state; const auto result = read_file(path_, ns, &state);
  if (result == ReadFileResult::Missing) return PersistenceLoadResult::NotFound;
  if (result == ReadFileResult::Incompatible) return PersistenceLoadResult::Incompatible;
  if (result == ReadFileResult::Error) return PersistenceLoadResult::Error;
  const auto item = state.find(key);
  if (item == state.end()) return PersistenceLoadResult::NotFound;
  *count = item->second; return PersistenceLoadResult::Found;
}
bool FileUserStatePersistence::write(RegistryNamespaceId ns, StableLexemeKey key,
                                     std::uint32_t count) {
  if (!ns || key.empty()) return false;
  StateMap state; const auto result = read_file(path_, ns, &state);
  // In particular V1 must never be treated as an empty V2 file and overwritten.
  if (result == ReadFileResult::Error || result == ReadFileResult::Incompatible) return false;
  state[key] = count;
  return write_file(path_, ns, state);
}
const std::filesystem::path& FileUserStatePersistence::path() const { return path_; }
}  // namespace hanime
