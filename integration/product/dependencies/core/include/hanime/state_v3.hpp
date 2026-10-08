#pragma once
#include "hanime/core.hpp"
#include <array>

namespace hanime {
struct StateBudget {
  std::size_t read_calls{1}, write_calls{1}, read_bytes{64}, write_bytes{64};
  std::size_t control_calls{1}, work_items{1};
};
struct StateCost {
  std::size_t read_calls{0}, write_calls{0}, read_bytes{0}, write_bytes{0};
  std::size_t control_calls{0}, work_items{0};
  void add(const StateCost& c);
};
enum class StateStatus { Pending, Ready, Found, NotFound, HostStorageConfirmed,
  MissingStore, Incompatible, Corrupt, TailUncertain, Unavailable, Stale,
  InvalidRequest, InvalidBudget, MaintenanceBlocked, ClosedSaved, CloseUnsaved };
struct StateStep {
  StateStatus status{StateStatus::Pending}; StateCost cost;
  bool needs_budget{false}; std::uint32_t count{0};
};
enum class FileResult { Ok, Missing, Error };
struct FileTransfer { bool ok{false}; std::size_t bytes{0}; };
// One call = one measured physical operation. Only the Host implementation
// uses OS files. No hidden whole-file reads, buffered streams or durable dtor.
class StateFilePort {
 public:
  virtual ~StateFilePort() = default;
  virtual FileResult open(unsigned file, bool exclusive_create) = 0;
  virtual bool size(unsigned file, std::uint64_t&) = 0;
  virtual FileTransfer read(unsigned file, std::uint64_t, void*, std::size_t) = 0;
  virtual FileTransfer write(unsigned file, std::uint64_t, const void*, std::size_t) = 0;
  virtual bool sync(unsigned file) = 0;
  virtual bool truncate(unsigned file, std::uint64_t) = 0;
  virtual bool close(unsigned file) = 0;
};
struct StateIoIdentity {};
enum class StateReadRole : unsigned { RankLookup=0, IntentLoad=1 };
struct StateIoTicket {
  std::shared_ptr<const StateIoIdentity> activation;
  StateReadRole role{StateReadRole::RankLookup}; std::uint64_t revision{0};
};
struct StateV3Config {
  RegistryNamespaceId registry_namespace{0}; std::uint64_t lineage{0};
  std::size_t max_single_io{64}; std::uint64_t tail_threshold{32};
  std::uint64_t max_file_bytes{16*1024*1024};
};
struct StateFileInfo {
  std::uint64_t generation{0}, snapshot_keys{0}, tail_frames{0}, sequence{0}, bytes{0};
  unsigned active_file{0}; bool maintaining{false}, write_pending{false};
};
struct StateV3Memory {
  std::size_t owner{0}, impl{0}, reads{0}, validation{0}, maintenance{0}, write_copy{0};
};
class StateV3 {
 public:
  StateV3(StateFilePort&, StateV3Config);
  ~StateV3(); // Releases memory only. Explicit close owns OS close operations.
  StateV3(const StateV3&)=delete; StateV3& operator=(const StateV3&)=delete;
  StateStatus begin_open(bool create_if_both_missing=false);
  StateStep advance_open(const StateBudget&);
  StateStatus status() const;
  bool ready() const;
  StateIoTicket begin_lookup(StateReadRole, const StableLexemeKey&);
  StateStep advance_lookup(const StateIoTicket&, const StateBudget&);
  void cancel_read(const StateIoTicket&); // Ownership, not availability.
  bool read_is_owned(const StateIoTicket&) const;
  StateStatus begin_write(const StableLexemeKey&, std::uint32_t absolute);
  StateStep advance_write(const StateBudget&);
  StateStatus begin_maintenance();
  StateStep advance_maintenance(const StateBudget&);
  StateStep advance_close(const StateBudget&);
  StateFileInfo info() const;
  std::shared_ptr<const StateIoIdentity> activation() const;
  RegistryNamespaceId registry_namespace() const;
  StateV3Memory memory_usage() const;
 private: struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace hanime
