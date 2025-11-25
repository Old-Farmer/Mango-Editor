#pragma once

#include <vector>

namespace mango {

enum class MouseState {
    kReleased,
    kLeftNotReleased,
    kRightNotReleased,
    kMiddleNotReleased,
    kLeftHolding,
};

enum class BufferState : int {
    kModified = 0,
    kNotModified = 1,
    kCannotRead = 2,
    kHaveNotRead = 3,
    kCannotCreate = 4,
    kReadOnly = 5,
    kCodingInvalid = 6,
};

constexpr const char* BufferStateString[] = {
    "[Modified]",       "",
    "[Can't Read]",     "[Haven't Read]",
    "[Can' Create]",    "[Read Only]",
    "[Coding Invalid]",
};

enum class Mode : int {
    kEdit = 0,
    kPeelCommand,  // user is inputting sth
    kPeelShow,     // peel shows some multirow output

    kViNormal,
    kViVisual,
    kViVisualLine,
    kViVisualBlock,
    kOperatorPending,

    kNone,
    _kCount,  // not mode, just for count
};

inline bool IsPeel(Mode mode) {
    return mode == Mode::kPeelCommand || mode == Mode::kPeelShow;
}

extern const std::vector<Mode> kDefaultsModes;
extern const std::vector<Mode> kAllModes;

}  // namespace mango