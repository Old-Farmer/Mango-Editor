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
    kEdit = 0,     // == kVimInsert
    kPeelCommand,  // == kVimCommand, user is inputting sth
    kPeelShow,     // == kVimCommandShow, peel shows some multirow output

    // Vi specific mode
    kVimNormal,
    kVimVisual,
    kVimVisualLine,
    kVimVisualBlock,
    kVimOperatorPending,

    kNone,    // Used for some situations that don't care about mode.
    _kCount,  // not mode, just for count
};

constexpr std::string_view kViModeString[] = {
    "INSERT",   "COMMAND",  "COMMAND",    "NORMAL", "VISUAL",
    "VISUAL-L", "VISUAL-B", "OP-PEND", "",       "",
};

#define MGO_VIM_MODE_WIDTH "8" // WIDTH for showing vim mode

inline bool IsPeel(Mode mode) {
    return mode == Mode::kPeelCommand || mode == Mode::kPeelShow;
}

#define MGO_DEFAULT_MODES Mode::kEdit
#define MGO_ALL_MODES Mode::kEdit, Mode::kPeelCommand, Mode::kPeelShow

#define MGO_VIM_VISUAL_MODES \
    Mode::kVimVisual, Mode::kVimVisualLine, Mode::kVimVisualBlock
#define MGO_VIM_DEFAULT_MODES \
    Mode::kVimNormal, MGO_VIM_VISUAL_MODES, Mode::kVimOperatorPending
#define MGO_VIM_ALL_MODES                                                 \
    Mode::kVimNormal, Mode::kEdit, Mode::kVimVisual, Mode::kVimVisualLine,  \
        Mode::kVimVisualBlock, Mode::kVimOperatorPending, Mode::kPeelShow, \
        Mode::kPeelCommand

}  // namespace mango
