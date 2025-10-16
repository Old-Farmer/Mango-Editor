#include "filetype.h"

#include <unordered_map>

namespace mango {

Filetype DecideFiletype(std::string& path) {
    std::string::size_type pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return Filetype::kText;
    }

    // TODO: regex and better
    std::string_view suffix(path.c_str() + pos + 1, path.size() - pos - 1);
    if (suffix == "c") {
        return Filetype::kC;
    }
    return Filetype::kText;
}

}  // namespace mango