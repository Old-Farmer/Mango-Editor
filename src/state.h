#pragma once

namespace mango {

enum class MouseState {
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

enum class EditorMode : int {
    kEdit,
    kPeel,
    kFind,
};

}  // namespace mango