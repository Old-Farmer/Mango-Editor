#include "filetype.h"

#include <string>
#include <unordered_map>

#include "utils.h"

namespace mango {

constexpr zstring_view kDefaultFileType = "text";

// clang-format off
static const std::unordered_map<zstring_view, zstring_view> kSuffixToFiletype = {
    {"c", "c"},
    {"cpp", "cpp"},
    {"cc", "cpp"},
    {"cxx", "cpp"},
    {"h", "cpp"},
    {"hpp", "cpp"},
    {"hh", "cpp"},
    {"hxx", "cpp"},
    {"rs", "rust"},
    {"go", "go"},
    {"java", "java"},
    {"kt", "kotlin"},
    {"cs", "csharp"},
    {"py", "python"},
    {"lua", "lua"},
    {"js", "javascript"},
    {"ts", "typescript"},
    {"bash", "bash"},
    {"sh", "shell"},
    {"txt", kDefaultFileType},
    {"json", "json"},
    {"toml", "toml"},
    {"yaml", "yaml"},
    {"md", "markdown"},
    {"cmake", "cmake"},
};

static const std::unordered_map<zstring_view, zstring_view> kFileTypesToStrRep = {
    {"", "None"},
    {"c", "C"},
    {"cpp","C++"},
    {"go","Go"},
    {"java","Java"},
    {"kotlin","Kotlin"},
    {"csharp","C♯"},
    {"python","Python"},
    {"lua","Lua"},
    {"javascript","Javascript"},
    {"typescript","Typescript"},
    {"bash","Bash"},
    {"shell","Shell"},
    {kDefaultFileType,"txt"},
    {"json","JSON"},
    {"toml","TOML"},
    {"yaml","YAML"},
    {"markdown","Markdown"},
    {"cmake","CMake"},
    {"makefile","Makefil"}
};

// file name is prior to suffix
static const std::unordered_map<zstring_view, zstring_view> kNameToFiletype = {
    {"CMakeLists.txt", "cmake"},
    {"Makefile", "makefile"},
};
// clang-format on

zstring_view DecideFiletype(std::string_view file_name) {
    auto iter = kNameToFiletype.find(file_name);
    if (iter != kNameToFiletype.end()) {
        return iter->second;
    }

    std::string::size_type pos = file_name.find_last_of('.');
    if (pos == std::string::npos) {
        return kDefaultFileType;
    }

    // TODO: regex and better
    std::string_view suffix(file_name.data() + pos + 1,
                            file_name.size() - pos - 1);
    auto iter2 = kSuffixToFiletype.find(suffix);
    if (iter2 == kSuffixToFiletype.end()) {
        return kDefaultFileType;
    }
    return iter2->second;
}

std::vector<zstring_view> AllFiletypes() {
    std::vector<zstring_view> filetypes(kSuffixToFiletype.size());
    size_t i = 0;
    for (auto [_, filetype] : kSuffixToFiletype) {
        filetypes[i++] = filetype;
    }
    return filetypes;
}

zstring_view IsFiletype(zstring_view filetype) {
    auto iter = kFileTypesToStrRep.find(filetype);
    if (iter == kFileTypesToStrRep.end()) {
        return {};
    }
    return iter->first;
}

zstring_view FiletypeStrRep(zstring_view filetype) {
    return kFileTypesToStrRep.at(filetype);
}

}  // namespace mango