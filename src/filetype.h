#pragma once
#include <string_view>
#include <vector>
#include "utils.h"

namespace mango {

// A filetype is represented as an immutable string

zstring_view DecideFiletype(std::string_view file_name);

std::vector<zstring_view> AllFiletypes();

}  // namespace mango