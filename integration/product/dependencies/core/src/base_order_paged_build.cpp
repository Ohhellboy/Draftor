#include "base_order_paged_build.hpp"
#include "base_order_wire.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace hanime::detail {
namespace {
constexpr std::size_t kPageBytes = 1024;
constexpr std::size_t kPageHeaderBytes = 16;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
BaseHeader source_header(const std::vector<std::uint8_t>& bytes) {
  BaseHeader header;
  require(bytes.size() >= kBaseHeader && base_header(bytes.data(), bytes.size(), header) &&
          header.format == 1 && header.codes != 0, "paged order requires nonempty validated HBO1");
  std::uint32_t start = 0;
  std::string_view previous;
  for (std::uint32_t i = 0; i < header.codes; ++i) {
    const auto* row = bytes.data() + kBaseHeader + std::size_t(i) * header.width;
    const auto length = bo32(row);
    const auto count = bo32(row + 8);
    require(length && length <= header.max_code && bo32(row + 4) == start && count &&
            count <= header.groups - start && bo32(row + 12) >= count,
            "paged source directory coverage invalid");
    const std::string_view code(reinterpret_cast<const char*>(row + 16), length);
    require((i == 0 || previous < code) &&
            std::all_of(row + 16 + length, row + header.width, [](auto b) { return b == 0; }),
            "paged source directory ordering/padding invalid");
    previous = code;
    start += count;
  }
  require(start == header.groups, "paged source has uncovered ranks");
  return header;
}
struct Node {
  std::uint32_t level, first, count, last_row, page;
};
class DirectoryChecker {
 public:
  DirectoryChecker(const ResourceReader& file, const std::vector<std::uint8_t>& source,
                   const BaseHeader& old_header, const BaseHeader& new_header)
      : file_(file), source_(source), old_(old_header), header_(new_header),
        visited_(header_.directory_pages, false) {}

  // Return the last original row covered by this subtree. This validates the
  // fence against decoded descendants, independently of the generator's nodes.
  std::uint32_t visit(std::uint32_t page, std::uint32_t expected_level, bool root) {
    require(page < visited_.size() && !visited_[page], "paged duplicate/cyclic/out-of-range child");
    visited_[page] = true;
    std::array<std::uint8_t, kPageBytes> bytes{};
    require(file_.read(kPageBytes + std::size_t(page) * kPageBytes, bytes.data(), bytes.size()),
            "paged directory short read");
    const auto level = bo32(bytes.data());
    const auto count = bo32(bytes.data() + 4);
    require(level <= 31 && (root || level == expected_level) && count &&
            count <= (kPageBytes - kPageHeaderBytes) / header_.width &&
            bo64(bytes.data() + 8) == 0, "paged page header invalid");
    std::uint32_t last = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
      const auto* row = bytes.data() + kPageHeaderBytes + std::size_t(i) * header_.width;
      if (level == 0) {
        require(next_row_ < old_.codes, "paged extra leaf row");
        const auto* expected = source_.data() + kBaseHeader + std::size_t(next_row_) * old_.width;
        require(std::equal(row, row + header_.width, expected), "paged leaf row differs from validated HBO1");
        last = next_row_++;
      } else {
        const auto child = bo32(row + 4);
        require(child > page && bo64(row + 8) == 0, "paged internal child metadata invalid");
        last = visit(child, level - 1, false);
        const auto* expected = source_.data() + kBaseHeader + std::size_t(last) * old_.width;
        require(bo32(row) == bo32(expected) &&
                std::equal(row + 16, row + header_.width, expected + 16),
                "paged internal fence differs from subtree maximum");
      }
    }
    require(std::all_of(bytes.begin() + kPageHeaderBytes + std::size_t(count) * header_.width,
                        bytes.end(), [](auto b) { return b == 0; }), "paged page padding invalid");
    return last;
  }
  void finish() const {
    require(next_row_ == old_.codes &&
            std::all_of(visited_.begin(), visited_.end(), [](bool value) { return value; }),
            "paged directory has missing rows or unreachable pages");
  }
 private:
  const ResourceReader& file_;
  const std::vector<std::uint8_t>& source_;
  const BaseHeader& old_;
  const BaseHeader& header_;
  std::vector<bool> visited_;
  std::uint32_t next_row_{0};
};
}

std::vector<std::uint8_t> build_paged_base_order(const std::vector<std::uint8_t>& source,
                                               std::size_t* auxiliary_capacity_bytes) {
  const auto old = source_header(source);
  const auto fanout = static_cast<std::uint32_t>((kPageBytes - kPageHeaderBytes) / old.width);
  require(fanout >= 2, "paged directory fanout too small");
  std::vector<Node> nodes;
  std::vector<std::pair<std::size_t, std::size_t>> levels;
  for (std::uint32_t first = 0; first < old.codes;) {
    const auto count = std::min(fanout, old.codes - first);
    nodes.push_back({0, first, count, first + count - 1, 0});
    first += count;
  }
  levels.emplace_back(0, nodes.size());
  while (levels.back().second > 1) {
    const auto lower = levels.back();
    const auto begin = nodes.size();
    require(levels.size() <= 31, "paged directory too deep");
    for (std::size_t first = lower.first; first < lower.first + lower.second;) {
      const auto count = std::min<std::size_t>(fanout, lower.first + lower.second - first);
      require(first <= std::numeric_limits<std::uint32_t>::max(), "paged node index overflow");
      nodes.push_back({static_cast<std::uint32_t>(levels.size()), static_cast<std::uint32_t>(first),
                       static_cast<std::uint32_t>(count), nodes[first + count - 1].last_row, 0});
      first += count;
    }
    levels.emplace_back(begin, nodes.size() - begin);
  }
  require(nodes.size() <= std::numeric_limits<std::uint32_t>::max(), "paged page count overflow");
  std::uint32_t next_page = 0;
  for (auto level = levels.rbegin(); level != levels.rend(); ++level) {
    for (std::size_t i = level->first; i < level->first + level->second; ++i) nodes[i].page = next_page++;
  }
  const std::uint64_t rank_offset = kPageBytes * (1ULL + nodes.size());
  const auto tail_bytes = source.size() - static_cast<std::size_t>(old.rank_offset);
  require(rank_offset <= std::numeric_limits<std::size_t>::max() - tail_bytes, "paged file size overflow");
  std::vector<std::uint8_t> result(static_cast<std::size_t>(rank_offset + tail_bytes), 0);
  auto* header = result.data();
  put32(header, 0x324f4248); put32(header + 4, 2);
  put32(header + 8, old.width); put32(header + 12, old.codes);
  put32(header + 16, old.groups); put32(header + 20, old.max_code);
  put32(header + 24, kBaseItem); put32(header + 28, next_page);
  put64(header + 32, kPageBytes); put64(header + 40, rank_offset);
  put64(header + 48, rank_offset + std::uint64_t(old.groups) * kBaseItem);
  put64(header + 56, result.size());
  for (const auto& node : nodes) {
    auto* page = result.data() + kPageBytes + std::size_t(node.page) * kPageBytes;
    put32(page, node.level); put32(page + 4, node.count);
    for (std::uint32_t i = 0; i < node.count; ++i) {
      auto* row = page + kPageHeaderBytes + std::size_t(i) * old.width;
      const auto source_row = node.level == 0 ? node.first + i : nodes[node.first + i].last_row;
      const auto* original = source.data() + kBaseHeader + std::size_t(source_row) * old.width;
      if (node.level == 0) {
        std::copy_n(original, old.width, row);
      } else {
        put32(row, bo32(original));
        put32(row + 4, nodes[node.first + i].page);
        std::copy_n(original + 16, old.max_code, row + 16);
      }
    }
  }
  std::copy(source.begin() + old.rank_offset, source.end(), result.begin() + rank_offset);
  if (auxiliary_capacity_bytes) {
    *auxiliary_capacity_bytes = nodes.capacity() * sizeof(Node) + levels.capacity() * sizeof(levels.front());
  }
  return result;
}

bool check_paged_base_order(const ResourceReader& file, const std::vector<std::uint8_t>& source,
                           std::string* error) {
  try {
    const auto old = source_header(source);
    std::array<std::uint8_t, kPageBytes> bytes{};
    BaseHeader header;
    require(file.read(0, bytes.data(), bytes.size()) && base_header(bytes.data(), file.size(), header) &&
            header.format == 2 && header.codes == old.codes && header.groups == old.groups &&
            header.width == old.width && header.max_code == old.max_code,
            "paged final header differs from source");
    require(std::all_of(bytes.begin() + kBaseHeader, bytes.end(), [](auto b) { return b == 0; }),
            "paged header padding invalid");
    DirectoryChecker directory(file, source, old, header);
    directory.visit(0, 0, true);
    directory.finish();
    // Byte equivalence covers full keys, scores, representative ordinals,
    // reserved bytes and the inverse member permutation, not just visible text.
    const auto tail = source.size() - static_cast<std::size_t>(old.rank_offset);
    for (std::size_t offset = 0; offset < tail;) {
      const auto count = std::min(bytes.size(), tail - offset);
      require(file.read(header.rank_offset + offset, bytes.data(), count), "paged rank/member short read");
      require(std::equal(bytes.begin(), bytes.begin() + count, source.begin() + old.rank_offset + offset),
              "paged rank/member bytes differ from validated HBO1");
      offset += count;
    }
    if (error) error->clear();
    return true;
  } catch (const std::exception& problem) {
    if (error) *error = problem.what();
    return false;
  }
}
}
