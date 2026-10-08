#pragma once
#include "raw_edit_admission.hpp"

namespace ime_device {
RawAdmission admitWubiEdit(std::string_view raw, size_t begin, size_t end,
                           std::string_view text);
bool wubiFullCode(std::string_view raw);
} // namespace ime_device
