#pragma once
// Private implementation, never a candidate/selection authorization interface.
#include "hanime/bounded_output.hpp"
#include <algorithm>
#include <limits>

namespace hanime::detail {
class OutputReuseCache {
 public:
  struct Entry {
    StableLexemeKey key;
    RuntimeLexemeRef ref{0};
    OutputInvocation invocation{OutputInvocation::Profile};
    OutputStatus status{OutputStatus::Pending};
    std::uint32_t payload{kNoSharedString};
    std::size_t form_size{0}, text_size{0}, resolver_capacity{0};
    std::uint64_t revision{0}, age{0};
    bool valid{false};
  };
  explicit OutputReuseCache(OutputReuseConfig c) : config(c),
      entries(std::make_unique<Entry[]>(c.entries)),
      text(std::make_unique<char[]>(c.entries*c.text_bytes)),
      forms(std::make_unique<char[]>(c.entries*c.form_bytes)) {}
  static bool valid_config(OutputReuseConfig c) {
    return c.entries<=64 && (!c.entries || (c.text_bytes && c.text_bytes<=4096 &&
                                          c.form_bytes && c.form_bytes<=256));
  }
  bool candidate(std::size_t i,RuntimeLexemeRef ref,const StableLexemeKey& key,
                 OutputRequest request,std::size_t resolver_capacity) const {
    const auto& e=entries[i];
    return e.valid && e.ref==ref && e.key==key && e.invocation==request.invocation &&
        e.form_size==request.form.size() && e.resolver_capacity==resolver_capacity;
  }
  bool current(std::size_t i,std::uint64_t rev,bool complete) const {
    return i<config.entries && rev && entries[i].revision==rev && entries[i].valid==complete;
  }
  std::size_t reserve() {
    if (revision==std::numeric_limits<std::uint64_t>::max()) return config.entries;
    std::size_t slot=0;
    for (std::size_t i=0;i<config.entries;++i) {
      if (!entries[i].valid) {slot=i; break;}
      if (entries[i].age<entries[slot].age) slot=i;
    }
    if (entries[slot].valid) ++stats.evictions;
    entries[slot]={}; entries[slot].revision=++revision;
    return slot;
  }
  void touch(Entry& entry) {
    if (clock==std::numeric_limits<std::uint64_t>::max()) {
      for (std::size_t i=0;i<config.entries;++i) entries[i].age=0;
      clock=0;
    }
    entry.age=++clock;
  }
  OutputReuseStats snapshot() const {
    auto s=stats; s.entries=config.entries; s.fixed_bytes=sizeof(*this);
    s.metadata_bytes=config.entries*sizeof(Entry);
    s.text_bytes=config.entries*config.text_bytes; s.form_bytes=config.entries*config.form_bytes;
    for (std::size_t i=0;i<config.entries;++i) if (entries[i].valid) ++s.resident;
    return s;
  }
  OutputReuseConfig config;
  std::unique_ptr<Entry[]> entries;
  std::unique_ptr<char[]> text,forms;
  OutputReuseStats stats;
  std::uint64_t revision{0},clock{0};
};
} // namespace hanime::detail
