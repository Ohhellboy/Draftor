#pragma once
#include "hanime/core.hpp"
#include "hanime/count_batch.hpp"
#include <array>

namespace hanime {
struct StateBudget {
  std::size_t read_calls{1}, write_calls{1}, read_bytes{64}, write_bytes{64};
  std::size_t control_calls{1}, work_items{1};
  bool (*stop_requested)(void*){nullptr}; void* stop_context{nullptr};
  bool should_stop() const { return stop_requested && stop_requested(stop_context); }
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
  // Same-source compaction advanced this read's captured cutoff. The owner may
  // restart its reservation after checking the current logical state view.
  bool restart_lookup{false};
  bool progressed{false}; // Actual cursor/phase progress, not inferred from charged work.
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
  // No-I/O lifetime/epoch check, including runtime-index and RAM cache hits.
  // Host ports with a stable lifetime need not override this method.
  virtual bool current() const { return true; }
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
  // Optional bounded byte windows. Zero slots retains the direct-read path.
  // Each physical fill is still limited by max_single_io and the current budget.
  std::size_t read_window_bytes{0}, read_window_slots{0};
  // Runtime-only, per-confirmed-frame index. Zero keeps the scanning fallback.
  // Full keys and frame ordinals retain historical lookup cutoffs. Max 4096.
  std::size_t tail_index_entries{64};
};
struct StateFileInfo {
  std::uint64_t generation{0}, snapshot_keys{0}, tail_frames{0}, sequence{0}, bytes{0};
  unsigned active_file{0}; bool maintaining{false}, write_pending{false};
  std::uint64_t content_fingerprint{0}; // Validated snapshot and ordered tail digests.
};
struct StateV3Memory {
  std::size_t owner{0}, impl{0}, reads{0}, validation{0}, maintenance{0}, write_copy{0};
  std::size_t read_cache_metadata{0}, read_cache_data{0};
  std::size_t tail_index_values{0}, tail_index_order{0};
};
struct StateV3Diagnostics {
  std::uint64_t batch_begins{0},batch_members{0},batch_sort_steps{0},batch_pivots{0},batch_stream_records{0},batch_tail_frames{0},batch_restarts{0};
  std::size_t batch_peak_bytes{0};
  std::uint64_t lookup_begins{0}, tail_frame_fetches{0}, tail_frame_validations{0}, tail_frame_decodes{0};
  std::uint64_t tail_key_comparisons{0}, tail_index_build_frames{0}, tail_index_comparisons{0}, tail_index_moves{0};
  std::uint64_t tail_index_lookups{0}, tail_index_hits{0}, tail_index_misses{0}, tail_index_overflows{0};
  std::uint64_t tail_fallback_lookups{0}, tail_index_invalidations{0}, snapshot_decodes{0}, snapshot_key_comparisons{0}, confirmed_appends{0};
  std::size_t index_capacity{0}, index_covered{0};
  std::uint64_t index_generation{0}, index_sequence{0}; bool index_complete{false};
};
// Read-only, captured confirmed prefix. A physical generation change invalidates it.
struct StateExportCursor {
  std::shared_ptr<const StateIoIdentity> activation;
  std::uint64_t physical{0}, generation{0}, keys{0}, tails{0}, sequence{0}, index{0};
  unsigned file{0}; std::array<std::uint8_t,64> bytes{}; std::size_t pos{0};
};
struct StateExportStep { StateStep step; StableLexemeKey key; bool item{false}, complete{false}; };
class StateV3 {
 public:
  StateV3(StateFilePort&, StateV3Config);
  ~StateV3(); // Releases memory only. Explicit close owns OS close operations.
  StateV3(const StateV3&)=delete; StateV3& operator=(const StateV3&)=delete;
  StateStatus begin_open(bool create_if_both_missing=false);
  StateStep advance_open(const StateBudget&);
  StateStatus status() const;
  bool ready() const;
  bool storage_current() const;
  StateIoTicket begin_lookup(StateReadRole, const StableLexemeKey&);
  StateStep advance_lookup(const StateIoTicket&, const StateBudget&);
  StateIoTicket begin_batch(StateBatch&);
  StateStep advance_batch(const StateIoTicket&,StateBatch&,const StateBudget&);
  void cancel_read(const StateIoTicket&); // Ownership, not availability.
  bool read_is_owned(const StateIoTicket&) const;
  bool begin_export(StateExportCursor&) const;
  StateExportStep advance_export(StateExportCursor&,const StateBudget&);
  StateStatus begin_write(const StableLexemeKey&, std::uint32_t absolute);
  StateStep advance_write(const StateBudget&);
  StateStatus begin_maintenance();
  StateStep advance_maintenance(const StateBudget&);
  StateStep advance_close(const StateBudget&);
  StateFileInfo info() const;
  std::shared_ptr<const StateIoIdentity> activation() const;
  RegistryNamespaceId registry_namespace() const;
  StateV3Memory memory_usage() const;
  StateV3Diagnostics diagnostics() const;
 private:
  StateStep advance_lookup_one(const StateIoTicket&, const StateBudget&);
  struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace hanime
