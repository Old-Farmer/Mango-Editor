#pragma once
#include <string_view>

namespace mango {

// A filetype is represented as an immutable string

std::string_view DecideFiletype(std::string_view file_name);

}  // namespace mango