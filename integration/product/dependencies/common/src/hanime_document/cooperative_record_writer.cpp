#include "../../core_memory.hpp"
#include "cooperative_record_writer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace hanime::documents {

void CooperativeHeaderReader::start(StoragePort& storage, draftor::memory::String path,
                                    const RecordKind kind,
                                    const std::size_t max_payload,
                                    const MediumSession session) {
  storage_ = &storage;
  path_ = std::move(path);
  kind_ = kind;
  max_payload_ = max_payload;
  session_ = session;
  state_ = State::Open;
  status_ = Status::NotFound;
  read_status_ = Status::NotFound;
  after_close_status_ = Status::Ok;
  header_ = {};
  wanted_ = 0U;
  at_ = 0U;
  last_step_storage_ = false;
}

bool CooperativeHeaderReader::active() const noexcept {
  return state_ != State::Idle && state_ != State::Done;
}

void CooperativeHeaderReader::finish(const Status status) noexcept {
  status_ = status;
  state_ = State::Done;
}

bool CooperativeHeaderReader::service() {
  if (!active() || storage_ == nullptr) return false;
  last_step_storage_ = false;
  const MediumSession now = storage_->mediumSession();
  if (!now.available || now.mount_epoch != session_.mount_epoch) {
    finish(now.mount_epoch != session_.mount_epoch ? Status::SessionChanged
                                                   : Status::MediaUnavailable);
    read_status_ = status_;
    return true;
  }

  CooperativeIoResult result{};
  last_step_storage_ = state_ != State::Decode;
  switch (state_) {
    case State::Open:
      result = storage_->cooperativeOpen(path_, CooperativeOpenMode::Read);
      if (result.status == Status::Ok) {
        read_status_ = Status::Ok;
        state_ = State::IsDirectory;
      } else {
        read_status_ = result.status;
        finish(result.status);
      }
      break;
    case State::OpenFailedExists:
      result = storage_->cooperativeExists(path_);
      if (result.status != Status::Ok) {
        read_status_ = result.status;
      } else {
        read_status_ = result.flag ? Status::IoError : Status::NotFound;
      }
      finish(read_status_);
      break;
    case State::IsDirectory:
      result = storage_->cooperativeIsDirectory();
      if (result.status != Status::Ok) after_close_status_ = result.status;
      else if (result.flag) after_close_status_ = Status::IoError;
      state_ = result.status == Status::Ok && !result.flag
          ? State::Size : State::Close;
      break;
    case State::Size:
      result = storage_->cooperativeSize();
      if (result.status != Status::Ok) {
        after_close_status_ = result.status;
        state_ = State::Close;
      } else {
        wanted_ = std::min(result.value, bytes_.size());
        state_ = wanted_ == 0U ? State::Close : State::Read;
      }
      break;
    case State::Read: {
      const std::size_t count = wanted_ - at_;
      result = storage_->cooperativeRead(bytes_.data() + at_, count);
      if (result.status != Status::Ok || result.count == 0U ||
          result.count > count) {
        after_close_status_ = result.status == Status::Ok
            ? Status::IoError : result.status;
        state_ = State::Close;
      } else {
        at_ += result.count;
        if (at_ == wanted_) state_ = State::Close;
      }
      break;
    }
    case State::Close:
      result = storage_->cooperativeClose();
      if (result.status != Status::Ok) after_close_status_ = result.status;
      state_ = after_close_status_ == Status::Ok ? State::Decode : State::Done;
      if (state_ == State::Done) status_ = after_close_status_;
      break;
    case State::Decode:
      status_ = RecordStore::decodeHeader(
          std::string_view(reinterpret_cast<const char*>(bytes_.data()), at_),
          kind_, max_payload_, &header_);
      state_ = State::Done;
      break;
    case State::Idle:
    case State::Done:
      return false;
  }
  return true;
}

Status CooperativeRecordWriter::start(
    draftor::memory::String base, const RecordKind kind,
    const std::uint64_t requested_generation,
    const std::string_view* payload_parts, const std::size_t part_count,
    const MediumSession session, const std::uint64_t trusted_generation) {
  if (active() || base.empty() || session.mount_epoch == 0U ||
      !session.available || (payload_parts == nullptr && part_count != 0U) ||
      part_count > payload_parts_.size()) {
    return Status::Conflict;
  }
  std::size_t total = 0U;
  for (std::size_t i = 0U; i < part_count; ++i) {
    if (payload_parts[i].size() >
        std::numeric_limits<std::uint32_t>::max() - total) {
      return Status::TooLarge;
    }
    total += payload_parts[i].size();
  }
  base_ = std::move(base);
  kind_ = kind;
  requested_generation_ = requested_generation;
  trusted_generation_ = trusted_generation;
  generation_ = 0U;
  session_ = session;
  payload_size_ = total;
  payload_part_count_ = part_count;
  for (std::size_t i = 0U; i < part_count; ++i) payload_parts_[i] = payload_parts[i];
  running_crc_ = 0xFFFFFFFFU;
  payload_crc_ = 0U;
  crc_part_ = crc_offset_ = io_part_ = io_offset_ = 0U;
  storage_steps_ = cpu_steps_ = write_chunks_ = max_io_request_ = 0U;
  slot_status_[0] = slot_status_[1] = Status::NotFound;
  slot_read_status_[0] = slot_read_status_[1] = Status::NotFound;
  slot_header_[0] = slot_header_[1] = {};
  status_ = Status::Ok;
  deferred_failure_ = Status::Ok;
  directory_planned_=false;
  target_path_.clear();
  parent_path_.clear();
  current_directory_.clear();
  header_.clear();
  state_ = State::PayloadCrc;
  return Status::Ok;
}

bool CooperativeRecordWriter::active() const noexcept {
  return state_ != State::Idle && state_ != State::Done;
}

void CooperativeRecordWriter::crcUpdate(std::uint32_t* crc,
                                        const std::uint8_t* bytes,
                                        const std::size_t count) noexcept {
  if (crc == nullptr || (bytes == nullptr && count != 0U)) return;
  for (std::size_t i = 0U; i < count; ++i) {
    *crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (*crc & 1U);
      *crc = (*crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
}

bool CooperativeRecordWriter::sessionMatches(
    const StoragePort& storage) const noexcept {
  const MediumSession current = storage.mediumSession();
  return current.available && current.mount_epoch == session_.mount_epoch;
}

void CooperativeRecordWriter::fail(const Status status,
                                   const bool file_open) noexcept {
  deferred_failure_ = status;
  if (file_open && status != Status::SessionChanged &&
      status != Status::MediaUnavailable) {
    state_ = State::FailureClose;
  } else {
    status_ = status;
    state_ = State::Done;
  }
}

void CooperativeRecordWriter::advancePart(std::size_t* part,
                                          std::size_t* offset) const noexcept {
  if (part == nullptr || offset == nullptr) return;
  while (*part < record_parts_.size() &&
         (*part > payload_part_count_ ||
          *offset >= record_parts_[*part].size())) {
    ++*part;
    *offset = 0U;
  }
}

draftor::memory::String CooperativeRecordWriter::parentPath() const {
  const std::size_t slash = target_path_.rfind('/');
  return slash == draftor::memory::String::npos || slash == 0U
      ? draftor::memory::String("/") : target_path_.substr(0U, slash);
}

bool CooperativeRecordWriter::selectNextDirectory() {
  if (directory_scan_ > parent_path_.size()) return false;
  const std::size_t slash = parent_path_.find('/', directory_scan_);
  const std::size_t end = slash == draftor::memory::String::npos
      ? parent_path_.size() : slash;
  current_directory_ = parent_path_.substr(0U, end);
  directory_scan_ = slash == draftor::memory::String::npos
      ? parent_path_.size() + 1U : slash + 1U;
  return !current_directory_.empty();
}

OperationResult CooperativeRecordWriter::working(
    const char* stage) const noexcept {
  return {Status::Busy, stage};
}

OperationResult CooperativeRecordWriter::service(StoragePort& storage) {
  if (!active()) return {status_, state_ == State::Done ? "complete" : "idle",
                         status_ == Status::Ok};
  if (!storage.cooperativeSupported()) {
    fail(Status::Conflict, false);
    return {status_, "cooperative_storage"};
  }
  if (!sessionMatches(storage)) {
    const MediumSession current = storage.mediumSession();
    fail(current.mount_epoch != session_.mount_epoch ? Status::SessionChanged
                                                      : Status::MediaUnavailable,
         false);
    return {status_, "mount_epoch"};
  }

  CooperativeIoResult io{};
  switch (state_) {
    case State::PayloadCrc: {
      while (crc_part_ < payload_part_count_ &&
             crc_offset_ >= payload_parts_[crc_part_].size()) {
        ++crc_part_;
        crc_offset_ = 0U;
      }
      if (crc_part_ == payload_part_count_) {
        payload_crc_ = ~running_crc_;
        state_ = State::BuildHeader;
      } else {
        const std::string_view part = payload_parts_[crc_part_];
        const std::size_t count = std::min(kChunkBytes,
                                           part.size() - crc_offset_);
        crcUpdate(&running_crc_, reinterpret_cast<const std::uint8_t*>(
                      part.data() + crc_offset_), count);
        crc_offset_ += count;
      }
      ++cpu_steps_;
      return working("payload_crc");
    }
    case State::BuildHeader:
      // Generation is finalized after the authenticated A/B header peek.
      state_ = State::PeekAStart;
      ++cpu_steps_;
      return working("header_deferred");
    case State::PeekAStart:
      header_reader_.start(storage, base_ + ".A", kind_,
                           std::numeric_limits<std::uint32_t>::max(), session_);
      state_ = State::PeekA;
      ++cpu_steps_;
      return working("peek_a_start");
    case State::PeekA:
      header_reader_.service();
      if (header_reader_.lastStepWasStorage()) ++storage_steps_;
      else ++cpu_steps_;
      if (!header_reader_.active()) {
        slot_status_[0] = header_reader_.status();
        slot_read_status_[0] = header_reader_.readStatus();
        slot_header_[0] = header_reader_.header();
        state_ = State::PeekBStart;
      }
      return working("peek_a");
    case State::PeekBStart:
      header_reader_.start(storage, base_ + ".B", kind_,
                           std::numeric_limits<std::uint32_t>::max(), session_);
      state_ = State::PeekB;
      ++cpu_steps_;
      return working("peek_b_start");
    case State::PeekB:
      header_reader_.service();
      if (header_reader_.lastStepWasStorage()) ++storage_steps_;
      else ++cpu_steps_;
      if (!header_reader_.active()) {
        slot_status_[1] = header_reader_.status();
        slot_read_status_[1] = header_reader_.readStatus();
        slot_header_[1] = header_reader_.header();
        state_ = State::SelectSlot;
      }
      return working("peek_b");
    case State::SelectSlot: {
      const bool valid[2] = {slot_status_[0] == Status::Ok,
                             slot_status_[1] == Status::Ok};
      std::uint8_t selected = 0U;
      bool have_selected = false;
      if (valid[0] || valid[1]) {
        const bool trusted[2] = {
            valid[0] && trusted_generation_ != 0U &&
                slot_header_[0].generation == trusted_generation_,
            valid[1] && trusted_generation_ != 0U &&
                slot_header_[1].generation == trusted_generation_};
        if (trusted_generation_ != 0U && !trusted[0] && !trusted[1]) {
          fail(Status::Conflict, false);
          return {status_, "trusted_slot"};
        }
        selected = trusted[0] || trusted[1]
            ? (trusted[0] ? 0U : 1U)
            : (valid[0] && valid[1]
                ? (slot_header_[0].generation >= slot_header_[1].generation
                       ? 0U : 1U)
                : (valid[0] ? 0U : 1U));
        have_selected = true;
      } else {
        Status failure = Status::Corrupt;
        if (slot_read_status_[0] == Status::MediaUnavailable ||
            slot_read_status_[1] == Status::MediaUnavailable) failure = Status::MediaUnavailable;
        else if (slot_read_status_[0] == Status::SessionChanged ||
                 slot_read_status_[1] == Status::SessionChanged) failure = Status::SessionChanged;
        else if (slot_read_status_[0] == Status::IoError ||
                 slot_read_status_[1] == Status::IoError) failure = Status::IoError;
        else if (slot_status_[0] == Status::UnsupportedVersion ||
                 slot_status_[1] == Status::UnsupportedVersion) failure = Status::UnsupportedVersion;
        else if (slot_status_[0] == Status::TooLarge ||
                 slot_status_[1] == Status::TooLarge) failure = Status::TooLarge;
        else if (slot_read_status_[0] == Status::NotFound &&
                 slot_read_status_[1] == Status::NotFound) failure = Status::NotFound;
        if (failure != Status::NotFound) {
          fail(failure, false);
          return {status_, "slot_select"};
        }
      }
      const std::uint64_t maximum = have_selected
          ? slot_header_[selected].generation : 0U;
      if (requested_generation_ == 0U) {
        if (maximum == std::numeric_limits<std::uint64_t>::max()) {
          fail(Status::Conflict, false);
          return {status_, "generation"};
        }
        generation_ = maximum + 1U;
      } else {
        if (requested_generation_ <= maximum) {
          fail(Status::Conflict, false);
          return {status_, "generation"};
        }
        generation_ = requested_generation_;
      }
      const std::uint8_t target = have_selected
          ? static_cast<std::uint8_t>(1U - selected) : 0U;
      target_path_ = base_ + (target == 0U ? ".A" : ".B");
      header_ = RecordStore::encodeHeader(kind_, generation_, payload_size_,
                                          payload_crc_);
      if (header_.empty()) {
        fail(Status::TooLarge, false);
        return {status_, "header"};
      }
      record_parts_[0] = header_;
      for (std::size_t i = 0U; i < payload_part_count_; ++i) {
        record_parts_[i + 1U] = payload_parts_[i];
      }
      expected_size_ = header_.size() + payload_size_;
      parent_path_ = parentPath();
      directory_scan_ = 1U;
      state_ = storage.writesTruncate() ? State::WriteOpen : State::DirectoryPlan;
      ++cpu_steps_;
      return working("slot_selected");
    }
    case State::DirectoryPlan:
      directory_planned_=true;
      if (selectNextDirectory()) state_ = State::DirectoryExists;
      else state_ = storage.writesTruncate() ? State::WriteOpen : State::TargetExists;
      ++cpu_steps_;
      return working("directory_plan");
    case State::DirectoryExists:
      io = storage.cooperativeExists(current_directory_);
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else if(io.flag&&io.value==1U)state_=State::DirectoryPlan;
      else if(io.flag&&io.value==2U)fail(Status::IoError,false);
      else state_ = io.flag ? State::DirectoryOpen : State::DirectoryMkdir;
      return working("directory_exists");
    case State::DirectoryOpen:
      io = storage.cooperativeOpen(current_directory_, CooperativeOpenMode::Read);
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else state_ = State::DirectoryIsDirectory;
      return working("directory_open");
    case State::DirectoryIsDirectory:
      io = storage.cooperativeIsDirectory();
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, true);
      else {
        if (!io.flag) deferred_failure_ = Status::IoError;
        state_ = State::DirectoryClose;
      }
      return working("directory_isdir");
    case State::DirectoryClose:
      io = storage.cooperativeClose();
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else if (deferred_failure_ != Status::Ok) fail(deferred_failure_, false);
      else state_ = State::DirectoryPlan;
      deferred_failure_ = Status::Ok;
      return working("directory_close");
    case State::DirectoryMkdir:
      io = storage.cooperativeMkdir(current_directory_);
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else state_ = State::DirectoryPlan;
      return working("directory_mkdir");
    case State::TargetExists:
      io = storage.cooperativeExists(target_path_);
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else state_ = io.flag ? State::TargetRemove : State::WriteOpen;
      return working("target_exists");
    case State::TargetRemove:
      io = storage.cooperativeRemove(target_path_);
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else state_ = State::WriteOpen;
      return working("target_remove");
    case State::WriteOpen:
      io = storage.cooperativeOpen(target_path_, CooperativeOpenMode::Write);
      ++storage_steps_;
      // Fast path matches synchronous writeParts: existing parents need no
      // directory probes. One failed open may create missing parents, once.
      if(io.status!=Status::Ok&&storage.writesTruncate()&&!directory_planned_){state_=State::DirectoryPlan;}
      else if (io.status != Status::Ok) fail(io.status, false);
      else {
        io_part_ = io_offset_ = 0U;
        state_ = State::Write;
      }
      return working("write_open");
    case State::Write: {
      advancePart(&io_part_, &io_offset_);
      if (io_part_ > payload_part_count_) {
        state_ = State::Flush;
        ++cpu_steps_;
        return working("write_complete");
      }
      const std::string_view part = record_parts_[io_part_];
      const std::size_t count = std::min(kChunkBytes,
                                         part.size() - io_offset_);
      io = storage.cooperativeWrite(
          reinterpret_cast<const std::uint8_t*>(part.data() + io_offset_),
          count);
      ++storage_steps_;
      ++write_chunks_;
      max_io_request_ = std::max(max_io_request_, count);
      if (io.status != Status::Ok || io.count != count) {
        fail(io.status == Status::Ok ? Status::IoError : io.status, true);
      } else {
        io_offset_ += count;
      }
      return working("write_chunk");
    }
    case State::Flush:
      io = storage.cooperativeFlush();
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, true);
      else state_ = State::WriteClose;
      return working("flush");
    case State::WriteClose:
      io = storage.cooperativeClose();
      ++storage_steps_;
      if (io.status != Status::Ok) fail(io.status, false);
      else state_ = State::VerifyOpen;
      return working("write_close");
    case State::VerifyOpen:
      io = storage.cooperativeOpen(target_path_, CooperativeOpenMode::Read);
      ++storage_steps_;
      if (io.status == Status::IoError) state_ = State::VerifyOpenFailedExists;
      else if (io.status != Status::Ok) fail(io.status, false);
      else state_ = State::VerifyIsDirectory;
      return working("verify_open");
    case State::VerifyOpenFailedExists:
      io = storage.cooperativeExists(target_path_);
      ++storage_steps_;
      fail(io.status != Status::Ok ? io.status
                                   : (io.flag ? Status::IoError
                                              : Status::NotFound), false);
      return {status_, "verify_exists"};
    case State::VerifyIsDirectory:
      io = storage.cooperativeIsDirectory();
      ++storage_steps_;
      if (io.status != Status::Ok || io.flag) {
        deferred_failure_ = io.status == Status::Ok ? Status::IoError : io.status;
        state_ = State::VerifyClose;
      } else state_ = State::VerifySize;
      return working("verify_isdir");
    case State::VerifySize:
      io = storage.cooperativeSize();
      ++storage_steps_;
      if (io.status != Status::Ok || io.value != expected_size_) {
        deferred_failure_ = io.status == Status::Ok ? Status::IoError : io.status;
        state_ = State::VerifyClose;
      } else {
        io_part_ = io_offset_ = 0U;
        state_ = State::VerifyRead;
      }
      return working("verify_size");
    case State::VerifyRead: {
      advancePart(&io_part_, &io_offset_);
      if (io_part_ > payload_part_count_) {
        state_ = State::VerifyEof;
        ++cpu_steps_;
        return working("verify_payload_complete");
      }
      const std::string_view part = record_parts_[io_part_];
      const std::size_t count = std::min(kChunkBytes,
                                         part.size() - io_offset_);
      io = storage.cooperativeRead(verify_buffer_.data(), count);
      ++storage_steps_;
      max_io_request_ = std::max(max_io_request_, count);
      if (io.status != Status::Ok || io.count == 0U || io.count > count ||
          std::memcmp(verify_buffer_.data(), part.data() + io_offset_,
                      io.count) != 0) {
        deferred_failure_ = io.status == Status::Ok ? Status::IoError : io.status;
        state_ = State::VerifyClose;
      } else {
        io_offset_ += io.count;
      }
      return working("verify_chunk");
    }
    case State::VerifyEof:
      io = storage.cooperativeRead(verify_buffer_.data(), 1U);
      ++storage_steps_;
      if (io.status != Status::Ok || io.count != 0U) {
        deferred_failure_ = io.status == Status::Ok ? Status::IoError : io.status;
      }
      state_ = State::VerifyClose;
      return working("verify_eof");
    case State::VerifyClose:
      io = storage.cooperativeClose();
      ++storage_steps_;
      if (io.status != Status::Ok) deferred_failure_ = io.status;
      state_ = deferred_failure_ == Status::Ok ? State::Finish : State::Done;
      status_ = deferred_failure_;
      return working("verify_close");
    case State::Finish:
      status_ = Status::Ok;
      state_ = State::Done;
      ++cpu_steps_;
      return {Status::Ok, "complete", true};
    case State::FailureClose:
      io = storage.cooperativeClose();
      ++storage_steps_;
      status_ = io.status == Status::Ok ? deferred_failure_ : io.status;
      state_ = State::Done;
      return {status_, "failure_close"};
    case State::Idle:
    case State::Done:
      break;
  }
  return {status_, state_ == State::Done ? "complete" : "working",
          status_ == Status::Ok && state_ == State::Done};
}

}  // namespace hanime::documents
