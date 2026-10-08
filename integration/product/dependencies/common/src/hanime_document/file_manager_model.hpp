#pragma once

#include "document_session.hpp"

#include <cstddef>
#include <string_view>

namespace hanime::documents {

enum class FileManagerView : std::uint8_t { Closed, Normal, Trash, TrashedReturn };
enum class PendingAction : std::uint8_t { None, TrashSelected, RestoreBody };

struct FileManagerState {
  FileManagerView view = FileManagerView::Closed;
  ListPage page{};
  std::size_t selected = 0;
  PendingAction pending = PendingAction::None;
  RestoreToken restore_token{};
  DocumentId trash_id{};
  std::uint64_t trash_session = 0;
  Status last_status = Status::Ok;
  DocumentId return_id{};
  std::uint64_t return_session=0;
};

class FileManagerModel {
 public:
  FileManagerModel(DocumentCatalog& catalog, DocumentSession& session) noexcept
      : catalog_(catalog), session_(session) {}

  Status enter(FileManagerView view);
  Status refresh();
  void cancel() noexcept;
  Status escape();
  void moveSelection(int delta) noexcept;
  Status nextPage();
  Status previousPage();
  // A host that closes the menu after success can skip the unused rescan.
  OperationResult activateSelected(std::uint32_t now_ms, bool refresh_after = true);
  Status renameSelected(std::string_view name);
  Status requestTrashSelected();
  OperationResult confirm(std::uint32_t now_ms);
  void cancelConfirmation() noexcept;
  OperationResult create(std::string_view name, std::uint32_t now_ms);
  Status requestBodyRestore(RestoreCommand command);

  const FileManagerState& state() const noexcept { return state_; }

 private:
  const DocumentMetadata* selectedMetadata() const noexcept;
  DocumentCatalog& catalog_;
  DocumentSession& session_;
  FileManagerState state_{};
};

}  // namespace hanime::documents
