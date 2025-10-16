#pragma once
#include <string>

namespace mango {

enum class Filetype {
    kText,
    kC,
};

Filetype DecideFiletype(std::string& path);

}  // namespace mango