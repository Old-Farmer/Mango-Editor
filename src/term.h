// A wrapper of termbox2

#pragma once

#include <vector>

#include "character.h"
#include "exception.h"
#include "logging.h"
#include "result.h"
#include "state.h"
#include "termbox2.h"
#include "utils.h"

namespace mango {

inline void PrintTermError(std::string_view context, int error) {
    PrintError(context, tb_strerror(error));
}

inline void PrintTermErrorExit(std::string_view context, int error) {
    PrintError(context, tb_strerror(error));
    exit(EXIT_FAILURE);
}

inline void PrintTermErrorExit(int error) {
    PrintError({}, tb_strerror(error));
    exit(EXIT_FAILURE);
}

class GlobalOpts;
class KeyseqManager;

class Terminal {
   private:
    Terminal() = default;
    ~Terminal();

   public:
    MGO_DELETE_COPY(Terminal);
    MGO_DELETE_MOVE(Terminal);

    static Terminal& GetInstance() {
        static Terminal term;
        return term;
    }

    // throws TermException
    void Init(GlobalOpts* global_opts);

    // throws TermException
    void Shutdown();

   private:
    void InitEscKeyseq(KeyseqManager& m);

   public:
    using Attr = uintattr_t;

    struct AttrPair {
        Attr fg;
        Attr bg;

        AttrPair() = default;
        AttrPair(Attr _fg, Attr _bg) : fg(_fg), bg(_bg) {}
    };

    enum Color : Attr {
        kDefault = TB_DEFAULT,
        kBlack = TB_BLACK,
        kRed = TB_RED,
        kGreen = TB_GREEN,
        kYellow = TB_YELLOW,
        kBlue = TB_BLUE,
        kMagenta = TB_MAGENTA,
        kCyan = TB_CYAN,
        kWhite = TB_WHITE,
        kHiBlack = TB_HI_BLACK  // for truecolor
    };

    enum Effect : Attr {
        kBold = TB_BOLD,
        kUnderline = TB_UNDERLINE,
        kReverse = TB_REVERSE,
        kItalic = TB_ITALIC,
        kBlink = TB_BLINK,
        kBright = TB_BRIGHT,
        kDim = TB_DIM,
        kStrikeOut = TB_STRIKEOUT,
        kUnderline2 = TB_UNDERLINE_2,
        kOverline = TB_OVERLINE,
        kInvisible = TB_INVISIBLE,
    };

    // throws TermException
    void SetClearAttr(const AttrPair& attr);

    // throws TermException
    // or
    // return
    // kOk for ok
    // kTermOutOfBounds for out of bounds
    Result SetCell(int col, int row, const Codepoint* codepoint,
                   size_t n_codepoint, const AttrPair& attr) {
        int ret = tb_set_cell_ex(
            col, row,
            const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(codepoint)),
            n_codepoint, attr.fg, attr.bg);
        if (ret == TB_OK) {
            return kOk;
        } else if (ret == TB_ERR_OUT_OF_BOUNDS) {
            return kTermOutOfBounds;
        } else {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return kOk;
    }

    // throws TermException
    // or
    // return
    // kOk for ok
    // kTermOutOfBounds for start out of bounds
    Result Print(int col, int row, const AttrPair& attr, const char* str) {
        int ret = tb_print(col, row, attr.fg, attr.bg, str);
        if (ret == TB_OK) {
            return kOk;
        } else if (ret == TB_ERR_OUT_OF_BOUNDS) {
            return kTermOutOfBounds;
        } else {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    // or
    // return
    // kOk for ok
    // kTermOutOfBounds for out of bounds
    Result SetCursor(int col, int row) {
        int ret = tb_set_cursor(col, row);
        if (ret == TB_OK) {
            return kOk;
        } else if (ret == TB_ERR_OUT_OF_BOUNDS) {
            return kTermOutOfBounds;
        } else {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return kOk;
    }

    // throws TermException
    void HideCursor() {
        int ret = tb_hide_cursor();
        if (ret != TB_OK) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    size_t Height() {
        int ret = tb_height();
        if (ret < 0) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return ret;
    }

    // throws TermException
    size_t Width() {
        int ret = tb_width();
        if (ret < 0) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return ret;
    }

    // throws TermException
    void Clear() {
        int ret = tb_clear();
        if (ret != TB_OK) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    void Present() {
        int ret = tb_present();
        if (ret != TB_OK) {
            MGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

   private:
    // throws TermException
    bool PollInner(int timeout_ms);

    void HandleEsc();

   public:
    // throws TermException
    // timeout == -1 means infinite blocking
    bool Poll(int timeout_ms);

    bool EventIsResize() { return event_.type == TB_EVENT_RESIZE; }

    bool EventIsKey() { return event_.type == TB_EVENT_KEY; }

    bool EventIsMouse() { return event_.type == TB_EVENT_MOUSE; }

    enum class EventType : uint8_t {
        kResize = TB_EVENT_RESIZE,
        kKey = TB_EVENT_KEY,
        kMouse = TB_EVENT_MOUSE,
        kBracketedPasteOpen,
        kBracketedPasteClose,
    };

    EventType WhatEvent() { return static_cast<EventType>(event_.type); }

    struct ResizeInfo {
        int width;
        int height;
    };

    enum class MouseKey : uint16_t {
        kLeft = TB_KEY_MOUSE_LEFT,
        kRight = TB_KEY_MOUSE_RIGHT,
        kMiddle = TB_KEY_MOUSE_MIDDLE,
        kRelease = TB_KEY_MOUSE_RELEASE,
        kWheelUp = TB_KEY_MOUSE_WHEEL_UP,
        kWheelDown = TB_KEY_MOUSE_WHEEL_DOWN,
    };

    struct MouseInfo {
        int col;
        int row;
        MouseKey t;
    };

    enum class SpecialKey : uint16_t {
        kCtrlTilde = TB_KEY_CTRL_TILDE,
        kCtrl2 = TB_KEY_CTRL_2,
        kCtrlA = TB_KEY_CTRL_A,
        kCtrlB = TB_KEY_CTRL_B,
        kCtrlC = TB_KEY_CTRL_C,
        kCtrlD = TB_KEY_CTRL_D,
        kCtrlE = TB_KEY_CTRL_E,
        kCtrlF = TB_KEY_CTRL_F,
        kCtrlG = TB_KEY_CTRL_G,
        kBackspace = TB_KEY_BACKSPACE,
        kCtrlH = TB_KEY_CTRL_H,
        kTab = TB_KEY_TAB,
        kCtrlI = TB_KEY_CTRL_I,
        kCtrlJ = TB_KEY_CTRL_J,
        kCtrlK = TB_KEY_CTRL_K,
        kCtrlL = TB_KEY_CTRL_L,
        kEnter = TB_KEY_ENTER,
        kCtrlM = TB_KEY_CTRL_M,
        kCtrlN = TB_KEY_CTRL_N,
        kCtrlO = TB_KEY_CTRL_O,
        kCtrlP = TB_KEY_CTRL_P,
        kCtrlQ = TB_KEY_CTRL_Q,
        kCtrlR = TB_KEY_CTRL_R,
        kCtrlS = TB_KEY_CTRL_S,
        kCtrlT = TB_KEY_CTRL_T,
        kCtrlU = TB_KEY_CTRL_U,
        kCtrlV = TB_KEY_CTRL_V,
        kCtrlW = TB_KEY_CTRL_W,
        kCtrlX = TB_KEY_CTRL_X,
        kCtrlY = TB_KEY_CTRL_Y,
        kCtrlZ = TB_KEY_CTRL_Z,
        kEsc = TB_KEY_ESC,
        kCtrlLsqBracket = TB_KEY_CTRL_LSQ_BRACKET,
        kCtrl3 = TB_KEY_CTRL_3,
        kCtrl4 = TB_KEY_CTRL_4,
        kCtrlBackslash = TB_KEY_CTRL_BACKSLASH,
        kCtrl5 = TB_KEY_CTRL_5,
        kCtrlRsqBracket = TB_KEY_CTRL_RSQ_BRACKET,
        kCtrl6 = TB_KEY_CTRL_6,
        kCtrl7 = TB_KEY_CTRL_7,
        kCtrlSlash = TB_KEY_CTRL_SLASH,
        kCtrlUnderscore = TB_KEY_CTRL_UNDERSCORE,
        kSpace = TB_KEY_SPACE,
        kBackspace2 = TB_KEY_BACKSPACE2,
        kCtrl8 = TB_KEY_CTRL_8,
        kF1 = TB_KEY_F1,
        kF2 = TB_KEY_F2,
        kF3 = TB_KEY_F3,
        kF4 = TB_KEY_F4,
        kF5 = TB_KEY_F5,
        kF6 = TB_KEY_F6,
        kF7 = TB_KEY_F7,
        kF8 = TB_KEY_F8,
        kF9 = TB_KEY_F9,
        kF10 = TB_KEY_F10,
        kF11 = TB_KEY_F11,
        kF12 = TB_KEY_F12,
        kInsert = TB_KEY_INSERT,
        kDelete = TB_KEY_DELETE,
        kHome = TB_KEY_HOME,
        kEnd = TB_KEY_END,
        kPgup = TB_KEY_PGUP,
        kPgdn = TB_KEY_PGDN,
        kArrowUp = TB_KEY_ARROW_UP,
        kArrowDown = TB_KEY_ARROW_DOWN,
        kArrowLeft = TB_KEY_ARROW_LEFT,
        kArrowRight = TB_KEY_ARROW_RIGHT,
        kBackTab = TB_KEY_BACK_TAB,
    };

    // C++ prefer enum class, but we use enum here for convenient bit op
    enum Mod : uint8_t {
        kAlt = TB_MOD_ALT,
        kCtrl = TB_MOD_CTRL,
        kShift = TB_MOD_SHIFT,
        kMotion = TB_MOD_MOTION
    };

    // special key xor codepoint
    // and mod
    struct KeyInfo {
        Codepoint codepoint;
        SpecialKey special_key;
        Mod mod;

        bool IsSpecialKey() const noexcept { return codepoint == 0; }

        static constexpr KeyInfo CreateSpecialKey(
            SpecialKey key, Mod mod = static_cast<Mod>(0)) {
            return {0, key, mod};
        }

        static constexpr KeyInfo CreateNormalKey(
            Codepoint codepoint, Mod mod = static_cast<Mod>(0)) {
            MGO_ASSERT(codepoint != 0);
            return {codepoint, {}, mod};
        }

        size_t ToNumber() const {
            return (static_cast<size_t>(codepoint) << 24) |
                   (static_cast<size_t>(special_key) << 8) |
                   (static_cast<size_t>(mod));
        }
    };

    ResizeInfo EventResizeInfo() noexcept { return {event_.w, event_.h}; }
    MouseInfo EventMouseInfo() noexcept {
        return {event_.x, event_.y, static_cast<MouseKey>(event_.key)};
    }
    KeyInfo EventKeyInfo() noexcept {
        return {static_cast<Codepoint>(event_.ch),
                static_cast<SpecialKey>(event_.key),
                static_cast<Mod>(event_.mod)};
    }

    void PendCurrentEvent() {
        pendding_events_.insert(pendding_events_.begin(), event_);
    }

   private:
    tb_event event_;

    // When parsing escape key seq, some events occurs and interrupt it, we
    // should kept it in left_events and report them later.
    std::vector<tb_event> pendding_events_;
    KeyseqManager* esc_keyseq_manager_ = nullptr;
    Mode mode_ = Mode::kNone;  // const

    bool init_ = false;

    GlobalOpts* global_opts_;
};

}  // namespace mango