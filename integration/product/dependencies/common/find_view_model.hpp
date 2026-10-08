#pragma once

#include "find_types.hpp"

#include <string_view>

namespace hanime::find::display {

struct FindBarText {
  std::string_view message;
  std::string_view case_rule_label;
  std::string_view case_shortcut;
};

FindBarText findBarText(const ViewModel& view) noexcept;

}  // namespace hanime::find::display
