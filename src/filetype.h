#pragma once
#include <string_view>
#include <vector>
#include "utils.h"

namespace mango {

// A filetype is represented as an immutable string

zstring_view DecideFiletype(std::string_view file_name);

std::vector<zstring_view> AllFiletypes();

bool IsFiletype(zstring_view filetype);

zstring_view FiletypeStrRep(zstring_view filetype);

}  // namespace mango