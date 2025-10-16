#pragma once

#include <cstdint>
#include <vector>
namespace mango {

enum class MouseState : int {
    kPressed,
    kReleased,
};

enum class BufferState : int {
    kModified = 0,
    kNotModified = 1,
    kCannotRead = 2,
    kHaveNotRead = 3,
    kCannotCreate = 4,
};

constexpr const char* BufferStateString[] = {
    "[Modified]", "", "[Can't Read]", "[Haven't Read]", "[Can' Create]",
};

enum class Mode : int {
    kEdit = 0,
    kPeelCommand,  // user is inputting sth
    kPeelShow,     // peel shows some multirow output
    kFind,

    _COUNT, // not mode, just for count
};

extern const std::vector<Mode> kDefaultsModes;

}  // namespace mango