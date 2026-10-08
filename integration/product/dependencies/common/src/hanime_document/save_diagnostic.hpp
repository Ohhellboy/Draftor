#pragma once

// Diagnostic-only observer. No storage, editor mutation, allocation or output.
#include "editor_port.hpp"
#include <array>
#include <cstdio>
#include <type_traits>
#include <utility>

#ifndef HANIME_SAVE_SUBSTAGE_DIAGNOSTIC
#define HANIME_SAVE_SUBSTAGE_DIAGNOSTIC 1
#endif

namespace hanime::documents::save_diag {
constexpr bool kEnabled = HANIME_SAVE_SUBSTAGE_DIAGNOSTIC != 0;
enum class Op : std::uint8_t {
  Capture, BodySave, MetaLoad, GenerationPeek, Utf8, BodyCrc, PayloadCrc,
  HeaderCrc, PositionLoad, PairPeek, PairLoad, PairCommit, PrefixRead,
  FullRead, WriteParts, VerifyParts, DirectoryPlan, Exists, Mkdir, Remove,
  Open, IsDirectory, Size, Write, Flush, Close, Read, Compare, Eof,
  BodyAck, PositionSave, ReadOpen, ReadClose, WriteOpen, WriteClose,
  VerifyOpen, VerifyRead, VerifyClose, DirectoryOpen, DirectoryClose,
  ServiceStep, Count
};
inline const char* name(Op op) {
  constexpr const char* names[] = {
    "Capture","BodySave","MetaLoad","GenerationPeek","Utf8","BodyCrc","PayloadCrc",
    "HeaderCrc","PositionLoad","PairPeek","PairLoad","PairCommit","PrefixRead",
    "FullRead","WriteParts","VerifyParts","DirectoryPlan","Exists","Mkdir","Remove",
    "Open","IsDirectory","Size","Write","Flush","Close","Read","Compare","Eof",
    "BodyAck","PositionSave","ReadOpen","ReadClose","WriteOpen","WriteClose",
    "VerifyOpen","VerifyRead","VerifyClose","DirectoryOpen","DirectoryClose",
    "ServiceStep"
  };
  return names[static_cast<unsigned>(op)];
}
constexpr std::size_t kOps = static_cast<std::size_t>(Op::Count);
constexpr std::size_t kTransactions = 4, kLongest = 8;
enum class Kind : std::uint8_t { BodyAuto, PositionOnly };
enum class Trigger : std::uint8_t { Idle, HardDeadline, PositionIdle };
enum class End : std::uint8_t { None, UserStop, BufferFull };
struct Stat {
  std::uint64_t inclusive = 0, exclusive = 0, bytes = 0;
  std::uint32_t count = 0, maximum = 0, failures = 0, unknown = 0;
};
struct Slow {
  Op op{};
  std::uint8_t phase = 0;
  std::uint32_t start = 0, end = 0, duration = 0, bytes = 0;
  std::int8_t outcome = -1;
};
struct Transaction {
  Kind kind{}; Trigger trigger{};
  std::uint32_t sequence = 0, start = 0, end = 0, duration = 0;
  std::uint64_t covered = 0, revision = 0, completion_revision = 0;
  std::uint64_t body_revision = 0, completion_body_revision = 0;
  std::uint64_t document_session = 0, mount = 0;
  std::uint64_t position_revision = 0, completion_position_revision = 0;
  DocumentId document{};
  std::uint32_t bytes = 0, cursor = 0, viewport = 0, layout = 0;
  std::uint8_t affinity = 0;
  bool captured = false, complete = false, body_committed = false;
  bool body_dirty = false, position_dirty = false, identity_same = false;
  // -1 means the position phase did not occur, not successful by default.
  std::int16_t position_status = -1;
  Status result = Status::Busy;
  std::array<std::array<Stat, kOps>, 2> stats{};
  std::array<Slow, kLongest> longest{};
  std::uint32_t slow_count = 0, scopes = 0;
  std::uint32_t service_steps = 0, pause_count = 0;
  std::uint32_t pause_input = 0, pause_composition = 0, pause_storage = 0;
  std::uint32_t storage_steps = 0, cpu_steps = 0, write_chunks = 0;
  std::uint32_t max_io_request = 0;
};
#if HANIME_SAVE_SUBSTAGE_DIAGNOSTIC
class Scope;
class Recorder {
 public:
  using Clock = std::uint32_t (*)();
  Clock clock = nullptr;
  bool active() const { return accepting_; }
  bool pending() const { return pending_; }
  bool inTransaction() const { return current_ != nullptr; }
  bool canStart() const { return kEnabled && !accepting_ && !pending_ && !current_; }
  bool start(std::uint32_t session) {
    if (!canStart()) return false;
    records_ = {}; count_ = 0; dropped_ = 0; session_ = session;
    accepting_ = true; reason_ = End::None; output_record_ = output_row_ = 0;
    return true;
  }
  bool stop() {
    if (!accepting_) return false;
    accepting_ = false; reason_ = End::UserStop; pending_ = true;
    return true;
  }
  bool begin(Kind kind, Trigger trigger, const DocumentId& id,
             std::uint64_t serial, std::uint64_t mount) {
    if (!kEnabled || !accepting_ || !clock) return false;
    if (current_ || count_ == kTransactions) { ++dropped_; return false; }
    current_ = &records_[count_];
    current_->kind = kind; current_->trigger = trigger;
    current_->sequence = count_ + 1; current_->start = clock();
    current_->document = id; current_->document_session = serial; current_->mount = mount;
    phase_ = kind == Kind::PositionOnly ? 1 : 0;
    return true;
  }
  void capture(const EditorView& view, Status status) {
    if (!current_ || status != Status::Ok) return;
    current_->captured = true; current_->bytes = view.text.size();
    current_->revision = view.revision; current_->cursor = view.cursor_byte;
    current_->body_revision = view.body_revision;
    current_->viewport = view.viewport.byte_offset;
    current_->affinity = static_cast<std::uint8_t>(view.viewport.affinity);
    current_->layout = view.viewport.layout_revision;
  }
  void position(Status status) {
    if (current_) current_->position_status = static_cast<std::int16_t>(status);
  }
  void cooperative(std::size_t service_steps, std::size_t pause_count,
                   std::size_t pause_input, std::size_t pause_composition,
                   std::size_t pause_storage,
                   std::size_t storage_steps, std::size_t cpu_steps,
                   std::size_t write_chunks, std::size_t max_io_request) {
    if (!current_) return;
    current_->service_steps = static_cast<std::uint32_t>(service_steps);
    current_->pause_count = static_cast<std::uint32_t>(pause_count);
    current_->pause_input = static_cast<std::uint32_t>(pause_input);
    current_->pause_composition = static_cast<std::uint32_t>(pause_composition);
    current_->pause_storage = static_cast<std::uint32_t>(pause_storage);
    current_->storage_steps = static_cast<std::uint32_t>(storage_steps);
    current_->cpu_steps = static_cast<std::uint32_t>(cpu_steps);
    current_->write_chunks = static_cast<std::uint32_t>(write_chunks);
    current_->max_io_request = static_cast<std::uint32_t>(max_io_request);
  }
  void finish(const OperationResult& result, std::uint64_t revision,
              std::uint64_t body_revision,
              bool same, bool body_dirty, bool position_dirty,
              std::uint64_t position_revision = 0U,
              std::uint64_t completion_position_revision = 0U) {
    if (!current_) return;
    current_->end = clock(); current_->duration = current_->end - current_->start;
    current_->result = result.status; current_->body_committed = result.body_committed;
    current_->completion_revision = revision; current_->identity_same = same;
    current_->completion_body_revision = body_revision;
    current_->body_dirty = body_dirty; current_->position_dirty = position_dirty;
    current_->position_revision = position_revision;
    current_->completion_position_revision = completion_position_revision;
    current_->complete = true; current_ = nullptr; ++count_;
    if (count_ == kTransactions && accepting_) {
      accepting_ = false; pending_ = true; reason_ = End::BufferFull;
    }
  }
  std::uint32_t session() const { return session_; }
  std::size_t count() const { return count_; }
  const Transaction& at(std::size_t n) const { return records_[n]; }
  // Formatting happens only after capture stops and the caller grants idle output.
  bool nextLine(char* out, std::size_t size, bool idle);
 private:
  friend class Scope;
  friend class Phase;
  std::array<Transaction, kTransactions> records_{};
  Transaction* current_ = nullptr;
  Scope* top_ = nullptr;
  std::size_t count_ = 0, output_record_ = 0, output_row_ = 0;
  std::uint32_t session_ = 0, dropped_ = 0;
  std::uint8_t phase_ = 0;
  bool accepting_ = false, pending_ = false;
  End reason_ = End::None;
};
inline Recorder recorder;
class Phase {
 public:
  explicit Phase(std::uint8_t p) : old_(recorder.phase_) { recorder.phase_ = p; }
  ~Phase() { recorder.phase_ = old_; }
 private: std::uint8_t old_;
};
class Scope {
 public:
  Scope(Op op, std::size_t bytes = 0) : op_(op), bytes_(bytes) {
    if (!kEnabled || !recorder.current_) return;
    parent_ = recorder.top_; recorder.top_ = this;
    phase_ = recorder.phase_; active_ = true; start_ = recorder.clock();
  }
  Scope(const Scope&) = delete;
  ~Scope() {
    if (!active_) return;
    const auto end = recorder.clock();
    const std::uint32_t elapsed = end - start_;
    recorder.top_ = parent_;
    if (parent_) parent_->children_ += elapsed;
    else recorder.current_->covered += elapsed;
    auto& t = *recorder.current_;
    auto& s = t.stats[phase_][static_cast<unsigned>(op_)];
    ++t.scopes; ++s.count; s.inclusive += elapsed; s.bytes += bytes_;
    s.exclusive += elapsed >= children_ ? elapsed - children_ : 0;
    if (elapsed > s.maximum) s.maximum = elapsed;
    s.failures += outcome_ == 0; s.unknown += outcome_ < 0;
    // Keep the longest actual primitive/CPU calls, not enclosing save functions.
    const bool leaf = (op_ >= Op::Exists && op_ <= Op::Eof) ||
                      op_ == Op::Utf8 || op_ == Op::BodyCrc ||
                      op_ == Op::PayloadCrc || op_ == Op::HeaderCrc ||
                      op_ == Op::Capture || op_ == Op::BodyAck ||
                      op_ >= Op::ReadOpen || op_ == Op::ServiceStep;
    if (leaf) {
      Slow value{op_,phase_,start_,end,elapsed,bytes_,outcome_};
      std::size_t at = 0;
      while (at < t.slow_count && t.longest[at].duration >= elapsed) ++at;
      if (at < kLongest) {
        const auto last = t.slow_count < kLongest ? t.slow_count++ : kLongest - 1;
        for (auto i = last; i > at; --i) t.longest[i] = t.longest[i-1];
        t.longest[at] = value;
      }
    }
  }
  void result(std::int8_t result) { outcome_ = result; }
 private:
  Op op_; std::uint32_t bytes_, start_ = 0;
  std::uint64_t children_ = 0;
  Scope* parent_ = nullptr;
  std::uint8_t phase_ = 0;
  std::int8_t outcome_ = -1;
  bool active_ = false;
};
#else
// OFF has no clock, transaction array, phase or writable global state. In
// particular, Phase must not retain the ON recorder in the target image.
class Recorder {
 public:
  static constexpr bool active() { return false; }
  static constexpr bool pending() { return false; }
  static constexpr bool inTransaction() { return false; }
  static constexpr bool canStart() { return false; }
  static constexpr bool start(std::uint32_t) { return false; }
  static constexpr bool stop() { return false; }
  static constexpr bool begin(Kind, Trigger, const DocumentId&,
                              std::uint64_t, std::uint64_t) { return false; }
  static void capture(const EditorView&, Status) {}
  static void position(Status) {}
  static void cooperative(std::size_t, std::size_t, std::size_t, std::size_t,
                          std::size_t, std::size_t, std::size_t, std::size_t,
                          std::size_t) {}
  static void finish(const OperationResult&, std::uint64_t, std::uint64_t,
                     bool, bool, bool, std::uint64_t = 0U,
                     std::uint64_t = 0U) {}
  static constexpr std::uint32_t session() { return 0; }
  static constexpr std::size_t count() { return 0; }
  static constexpr bool nextLine(char*, std::size_t, bool) { return false; }
};
inline constexpr Recorder recorder{};
class Phase {
 public:
  explicit Phase(std::uint8_t) {}
  ~Phase() {}
};
class Scope {
 public:
  explicit Scope(Op, std::size_t = 0) {}
  Scope(const Scope&) = delete;
  ~Scope() {}
  void result(std::int8_t) {}
};
static_assert(std::is_empty_v<Recorder> && std::is_empty_v<Phase> &&
              std::is_empty_v<Scope>, "disabled save observer must be stateless");
#endif
inline std::int8_t statusOutcome(Status s) {
  return s == Status::Ok || s == Status::OkDegraded ? 1 : 0;
}
template<class F, class Check>
auto measured(Op op, std::size_t bytes, F&& f, Check&& check) {
  Scope span(op, bytes);
  auto result = f(); span.result(check(result)); return result;
}
template<class F> auto measuredStatus(Op op, F&& f) {
  return measured(op, 0, std::forward<F>(f), [](Status s){ return statusOutcome(s); });
}
template<class F> auto measuredResult(Op op, F&& f) {
  return measured(op, 0, std::forward<F>(f), [](const auto& r){ return statusOutcome(r.status); });
}
template<class F> void measuredVoid(Op op, F&& f) { Scope span(op); f(); }
#if HANIME_SAVE_SUBSTAGE_DIAGNOSTIC
inline bool Recorder::nextLine(char* out, std::size_t size, bool idle) {
  if (!idle || !pending_ || accepting_ || current_ || !out || size < 512) return false;
  using U = unsigned long long;
  if (output_record_ < count_) {
    const auto& t = records_[output_record_];
    const auto row = output_row_++;
    if (row == 0) {
      std::snprintf(out,size,"[SAVE_TX] session=%u tx=%u kind=%s trigger=%s start_us=%u end_us=%u total_us=%u\n",
        unsigned(session_),unsigned(t.sequence),t.kind==Kind::BodyAuto?"BodyAuto":"PositionOnly",
        t.trigger==Trigger::Idle?"Idle":t.trigger==Trigger::HardDeadline?"HardDeadline":"PositionIdle",
        unsigned(t.start),unsigned(t.end),unsigned(t.duration));
      return true;
    }
    if (row == 1) {
      std::snprintf(out,size,"[SAVE_RESULT] session=%u tx=%u result=%u complete=%u body_committed=%u position_status=%d dirty_body=%u dirty_position=%u identity_same=%u\n",
        unsigned(session_),unsigned(t.sequence),unsigned(t.result),unsigned(t.complete),
        unsigned(t.body_committed),int(t.position_status),unsigned(t.body_dirty),
        unsigned(t.position_dirty),unsigned(t.identity_same));
      return true;
    }
    if (row == 2) {
      std::snprintf(out,size,"[SAVE_CONTEXT] session=%u tx=%u captured=%u bytes=%u body_rev=%llu completion_rev=%llu text_rev=%llu completion_text_rev=%llu doc_session=%llu mount=%llu\n",
        unsigned(session_),unsigned(t.sequence),unsigned(t.captured),unsigned(t.bytes),
        U(t.revision),U(t.completion_revision),U(t.body_revision),
        U(t.completion_body_revision),U(t.document_session),U(t.mount));
      return true;
    }
    if (row == 3) {
      std::snprintf(out,size,"[SAVE_POSITION] session=%u tx=%u cursor=%u viewport=%u affinity=%u layout=%u position_version=%llu completion_position_version=%llu\n",
        unsigned(session_),unsigned(t.sequence),unsigned(t.cursor),unsigned(t.viewport),
        unsigned(t.affinity),unsigned(t.layout),U(t.position_revision),
        U(t.completion_position_revision));
      return true;
    }
    if (row == 4) {
      const auto remainder = t.duration >= t.covered ? t.duration - t.covered : 0;
      std::snprintf(out,size,"[SAVE_ACCOUNTING] session=%u tx=%u root_covered_us=%llu remainder_us=%llu scopes=%u\n",
        unsigned(session_),unsigned(t.sequence),U(t.covered),U(remainder),unsigned(t.scopes));
      return true;
    }
    if (row == 5) {
      const auto& a=t.document.bytes;
      std::snprintf(out,size,"[SAVE_ID] session=%u tx=%u document=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
        unsigned(session_),unsigned(t.sequence),a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10],a[11],a[12],a[13],a[14],a[15]);
      return true;
    }
    if (row == 6) {
      std::snprintf(out,size,"[SAVE_COOPERATIVE] session=%u tx=%u service_steps=%u pauses=%u pause_input=%u pause_composition=%u pause_storage=%u storage_steps=%u cpu_steps=%u write_chunks=%u max_io_request=%u\n",
        unsigned(session_),unsigned(t.sequence),unsigned(t.service_steps),
        unsigned(t.pause_count),unsigned(t.pause_input),
        unsigned(t.pause_composition),unsigned(t.pause_storage),
        unsigned(t.storage_steps),unsigned(t.cpu_steps),
        unsigned(t.write_chunks),unsigned(t.max_io_request));
      return true;
    }
    if (row < 7 + 2*kOps) {
      const auto index = row - 7, phase = index / kOps, op = index % kOps;
      const auto& s = t.stats[phase][op];
      std::snprintf(out,size,"[SAVE_OP] session=%u tx=%u phase=%s op=%s n=%u incl_us=%llu self_us=%llu max_us=%u failures=%u unknown=%u bytes=%llu\n",
        unsigned(session_),unsigned(t.sequence),phase?"POSITION":"BODY",name(static_cast<Op>(op)),
        unsigned(s.count),U(s.inclusive),U(s.exclusive),unsigned(s.maximum),unsigned(s.failures),unsigned(s.unknown),U(s.bytes));
      return true;
    }
    const auto index = row - 7 - 2*kOps;
    if (index < t.slow_count) {
      const auto& s=t.longest[index];
      std::snprintf(out,size,"[SAVE_SLOW] session=%u tx=%u rank=%u phase=%s op=%s start_us=%u end_us=%u duration_us=%u bytes=%u outcome=%d\n",
        unsigned(session_),unsigned(t.sequence),unsigned(index+1),s.phase?"POSITION":"BODY",name(s.op),
        unsigned(s.start),unsigned(s.end),unsigned(s.duration),unsigned(s.bytes),int(s.outcome));
      return true;
    }
    ++output_record_; output_row_=0;
    return nextLine(out,size,idle);
  }
  std::snprintf(out,size,"[SAVE_SUMMARY] session=%u count=%u complete=%u incomplete=0 dropped=%u overwritten=0 termination=%s output=COMPLETE\n",
    unsigned(session_),unsigned(count_),unsigned(count_),unsigned(dropped_),reason_==End::BufferFull?"BUFFER_FULL":"USER_STOP");
  pending_=false;
  return true;
}
#endif
} // namespace hanime::documents::save_diag
