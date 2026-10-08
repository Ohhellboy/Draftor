#include "file_manager_model.hpp"

#include <algorithm>

namespace hanime::documents {

Status FileManagerModel::enter(const FileManagerView view) {
  if (view == FileManagerView::Closed) { cancel(); return Status::Ok; }
  if(state_.view==FileManagerView::Closed){state_.return_id=session_.currentId();state_.return_session=session_.sessionSerial();}
  session_.cancelRestore(&state_.restore_token);
  state_.view = view; state_.page.offset = 0U; state_.selected = 0U; state_.pending = PendingAction::None;
  return refresh();
}
Status FileManagerModel::refresh() {
  if (state_.view == FileManagerView::Closed) return Status::Conflict;
  const DocumentArea area = state_.view == FileManagerView::Trash ? DocumentArea::Trash : DocumentArea::Normal;
  cancelConfirmation();
  Status status;
  do {
    status = catalog_.list(area, state_.page.offset, kMaxListPage, &state_.page);
    if (status != Status::Ok && status != Status::OkDegraded) { state_.page.items.clear(); break; }
    if (!state_.page.items.empty() || state_.page.offset == 0) break;
    state_.page.offset = state_.page.offset > kMaxListPage ? state_.page.offset-kMaxListPage : 0;
  } while (true);
  if (state_.page.items.empty()) state_.selected = 0U;
  else state_.selected = std::min(state_.selected, state_.page.items.size() - 1U);
  state_.last_status = status; return status;
}
Status FileManagerModel::escape() {
  if(state_.pending!=PendingAction::None){cancelConfirmation();return Status::Ok;}
  if(state_.view==FileManagerView::Trash||state_.view==FileManagerView::TrashedReturn)return enter(FileManagerView::Normal);
  if(state_.view==FileManagerView::Normal&&!state_.return_id.empty()&&
     (state_.return_id!=session_.currentId()||state_.return_session!=session_.sessionSerial())){
    DocumentMetadata target;const auto got=catalog_.get(state_.return_id,&target);
    if(got!=Status::Ok&&got!=Status::OkDegraded){state_.last_status=got;return got;}
    if(target.area==DocumentArea::Trash){state_.view=FileManagerView::TrashedReturn;return Status::Ok;}
  }
  cancel();return Status::Ok; // never opens a row or installs stale text
}
void FileManagerModel::cancel() noexcept {
  session_.cancelRestore(&state_.restore_token);
  state_ = {};
}
void FileManagerModel::moveSelection(const int delta) noexcept {
  cancelConfirmation();
  if (state_.page.items.empty()) return;
  const int size = static_cast<int>(state_.page.items.size());
  int next = static_cast<int>(state_.selected) + delta; if (next < 0) next = 0; if (next >= size) next = size - 1;
  state_.selected = static_cast<std::size_t>(next);
}
Status FileManagerModel::nextPage() {
  if (!state_.page.has_more) {
    state_.last_status = Status::LastPage;
    return state_.last_status;
  }
  state_.page.offset += state_.page.items.size(); state_.selected = 0U; return refresh();
}
Status FileManagerModel::previousPage() {
  if (state_.page.offset == 0U) {
    state_.last_status = Status::FirstPage;
    return state_.last_status;
  }
  state_.page.offset = state_.page.offset > kMaxListPage ? state_.page.offset - kMaxListPage : 0U; state_.selected = 0U; return refresh();
}
const DocumentMetadata* FileManagerModel::selectedMetadata() const noexcept {
  return state_.selected < state_.page.items.size() ? &state_.page.items[state_.selected] : nullptr;
}
OperationResult FileManagerModel::activateSelected(const std::uint32_t now_ms, const bool refresh_after) {
  const DocumentMetadata* selected = selectedMetadata(); if (!selected) return {Status::NotFound, "selection"};
  OperationResult result = state_.view == FileManagerView::Trash
      ? session_.restoreFromTrash(selected->id) : session_.open(selected->id, now_ms);
  state_.last_status = result.status; if ((result || result.status == Status::Partial) && refresh_after) (void)refresh(); return result;
}
Status FileManagerModel::renameSelected(const std::string_view name) {
  const DocumentMetadata* selected = selectedMetadata(); if (!selected) return Status::NotFound;
  const Status status = session_.rename(selected->id, name); state_.last_status = status; if (status == Status::Ok) (void)refresh(); return status;
}
Status FileManagerModel::requestTrashSelected() {
  if (state_.view != FileManagerView::Normal || !selectedMetadata()) return Status::NotFound;
  session_.cancelRestore(&state_.restore_token);
  state_.trash_id = selectedMetadata()->id; state_.trash_session = session_.sessionSerial();
  state_.pending = PendingAction::TrashSelected; state_.last_status = Status::ConfirmationRequired; return state_.last_status;
}
Status FileManagerModel::requestBodyRestore(const RestoreCommand command) {
  session_.cancelRestore(&state_.restore_token);
  const Status status = session_.prepareRestore(command, &state_.restore_token);
  state_.pending = status == Status::ConfirmationRequired ? PendingAction::RestoreBody : PendingAction::None;
  if (state_.pending == PendingAction::None) {
    session_.cancelRestore(&state_.restore_token);
  }
  state_.last_status = status; return status;
}
OperationResult FileManagerModel::confirm(const std::uint32_t now_ms) {
  OperationResult result{Status::Conflict, "pending"};
  if (state_.pending == PendingAction::TrashSelected) {
    const DocumentMetadata* selected = selectedMetadata();
    result = selected && selected->id == state_.trash_id && state_.trash_session == session_.sessionSerial() && catalog_.validateSession() == Status::Ok
        ? session_.moveToTrash(state_.trash_id, now_ms) : OperationResult{Status::StaleToken, "trash_target"};
  } else if (state_.pending == PendingAction::RestoreBody) {
    result = session_.confirmRestore(&state_.restore_token, now_ms);
  }
  state_.pending = PendingAction::None;
  session_.cancelRestore(&state_.restore_token);
  state_.last_status = result.status;
  if (result || result.status == Status::Partial) (void)refresh();
  return result;
}
void FileManagerModel::cancelConfirmation() noexcept {
  state_.pending = PendingAction::None;
  state_.trash_id = {}; state_.trash_session = 0;
  session_.cancelRestore(&state_.restore_token);
  state_.last_status = Status::Ok;
}
OperationResult FileManagerModel::create(const std::string_view name, const std::uint32_t now_ms) {
  OperationResult result = session_.create(name, now_ms); state_.last_status = result.status; if (result) (void)refresh(); return result;
}

}  // namespace hanime::documents
