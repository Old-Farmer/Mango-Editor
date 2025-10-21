#pragma once

#include <vector>

namespace mango {

enum class MouseState : int {
    kPressed,
    kReleased,
};

enum class BufferState : int {
    kModified = 0,
    kNotModified,
    kCannotRead,
    kHaveNotRead,
    kCannotCreate,
    kReadOnly
};

constexpr const char* BufferStateString[] = {
    "[Modified]",    "", "[Can't Read]", "[Haven't Read]",
    "[Can' Create]", "", "[Read Only]"};

enum class Mode : int {
    kEdit = 0,
    kPeelCommand,  // user is inputting sth
    kPeelShow,     // peel shows some multirow output
    kFind,

    _COUNT,  // not mode, just for count
};

inline bool IsPeel(Mode mode) {
    return mode == Mode::kPeelCommand || mode == Mode::kPeelShow;
}

extern const std::vector<Mode> kDefaultsModes;
extern const std::vector<Mode> kAllModes;

}  // namespace mango