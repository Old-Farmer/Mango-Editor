#pragma once

#include <string_view>
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

constexpr std::string_view kBufferStateString[] = {
    "[M]",
    "",
    "[Can't Read]",
    "[Haven't Read]",
    "[Can't Create]",
    "[RdOnly]",
    "[CodingInvalid]",
};

enum class Mode : int {
    kEdit = 0,     // == kViInsert
    kPeelCommand,  // == kViCommand, user is inputting sth
    kPeelShow,     // == kViCommandShow, peel shows some multirow output

    // Vi specific mode
    kViNormal,
    kViVisual,
    kViVisualLine,
    kViVisualBlock,
    kViOperatorPending,

    kNone,    // Used for some situations that don't care about mode.
    _kCount,  // not mode, just for count
};

constexpr std::string_view kViModeString[] = {
    "INSERT",   "COMMAND",  "COMMAND",    "NORMAL", "VISUAL",
    "VISUAL-L", "VISUAL-B", "OP-PEND", "",       "",
};

#define MGO_VI_MODE_WIDTH "8" // WIDTH for showing vi mode

inline bool IsPeel(Mode mode) {
    return mode == Mode::kPeelCommand || mode == Mode::kPeelShow;
}

#define MGO_DEFAULT_MODES Mode::kEdit
#define MGO_ALL_MODES Mode::kEdit, Mode::kPeelCommand, Mode::kPeelShow

#define MGO_VI_VISUAL_MODES \
    Mode::kViVisual, Mode::kViVisualLine, Mode::kViVisualBlock
#define MGO_VI_DEFAULT_MODES \
    Mode::kViNormal, MGO_VI_VISUAL_MODES, Mode::kViOperatorPending
#define MGO_VI_ALL_MODES                                                 \
    Mode::kViNormal, Mode::kEdit, Mode::kViVisual, Mode::kViVisualLine,  \
        Mode::kViVisualBlock, Mode::kViOperatorPending, Mode::kPeelShow, \
        Mode::kPeelCommand

}  // namespace mango
