#include "find_view_model.hpp"

#include "find_bar_layout.hpp"

namespace hanime::find::display {

FindBarText findBarText(const ViewModel& view) noexcept {
  std::string_view message;
  switch (view.status) {
    case Status::Closed: message = {}; break;
    case Status::Editing: message = "TYPE"; break;
    case Status::Searching: message = "FIND"; break;
    case Status::Match:
      if (view.wrapped) {
        message = view.direction == Direction::Forward
            ? "WRAP>" : "<WRAP";
      } else {
        message = "MATCH";
      }
      break;
    case Status::NotFound: message = "MISS"; break;
    case Status::EmptyQuery: message = "EMPTY"; break;
    case Status::QueryTooLong: message = "LONG"; break;
    case Status::CancelledOrStale: message = "STALE"; break;
    case Status::AdapterFailure: message = "ERROR"; break;
  }
  return {message, bar_layout::caseRuleLabel(view.case_sensitive),
          bar_layout::kCaseShortcut};
}

}  // namespace hanime::find::display
