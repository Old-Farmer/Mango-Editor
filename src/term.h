// A wrapper of termbox2

#pragma once

#include <vector>

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
        kWhite = TB_WHITE
    };

    enum Effect : Attr {
        kBold = TB_BOLD,
        kUnderline = TB_UNDERLINE,
        kReverse = TB_REVERSE,
        kItalic = TB_ITALIC,
        kBlink = TB_BLINK,
        kHiBlack = TB_HI_BLACK,
        kBright = TB_BRIGHT,
        kDim = TB_DIM,
    };

    // throws TermException
    // or
    // return
    // kOk for ok
    // kTermOutOfBounds for out of bounds
    Result SetCell(int col, int row, uint32_t* codepoint, size_t n_codepoint,
                   const AttrPair& attr) {
        int ret =
            tb_set_cell_ex(col, row, codepoint, n_codepoint, attr.fg, attr.bg);
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

   public:
    // throws TermException
    bool Poll(int timeout_ms);

    using Event = tb_event;

    const Event& GetEvent() { return event_; }

    static bool EventIsResize(const Event& e) {
        return e.type == TB_EVENT_RESIZE;
    }

    static bool EventIsKey(const Event& e) { return e.type == TB_EVENT_KEY; }

    static bool EventIsMouse(const Event& e) {
        return e.type == TB_EVENT_MOUSE;
    }

    enum class EventType : uint8_t {
        kResize = TB_EVENT_RESIZE,
        kKey = TB_EVENT_KEY,
        kMouse = TB_EVENT_MOUSE,
        kBracketedPasteOpen,
        kBracketedPasteClose,
    };

    static EventType WhatEvent(const Event& e) {
        return static_cast<EventType>(e.type);
    }

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
        uint32_t codepoint;
        SpecialKey special_key;
        Mod mod;

        bool IsSpecialKey() const noexcept { return codepoint == 0; }

        static constexpr KeyInfo CreateSpecialKey(
            SpecialKey key, Mod mod = static_cast<Mod>(0)) {
            return {0, key, mod};
        }

        static constexpr KeyInfo CreateNormalKey(
            uint32_t codepoint, Mod mod = static_cast<Mod>(0)) {
            MGO_ASSERT(codepoint != 0);
            return {codepoint, {}, mod};
        }

        size_t ToNumber() const {
            return (static_cast<size_t>(codepoint) << 24) |
                   (static_cast<size_t>(special_key) << 8) |
                   (static_cast<size_t>(mod));
        }
    };

    static ResizeInfo EventResizeInfo(const Event& e) noexcept {
        return {e.w, e.h};
    }
    static MouseInfo EventMouseInfo(const Event& e) noexcept {
        return {e.x, e.y, static_cast<MouseKey>(e.key)};
    }
    static KeyInfo EventKeyInfo(const Event& e) noexcept {
        return {e.ch, static_cast<SpecialKey>(e.key), static_cast<Mod>(e.mod)};
    }

    // Utilities funtions

    // -1 for not printable
    // 0 for combining character
    // 1 for 1 col
    // 2 for 2 col
    static int WCWidth(uint32_t ch) noexcept { return tb_wcwidth(ch); }

    static size_t StringWidth(const std::string& str);

   private:
    Event event_;

    // When parsing escape key seq, some events occurs and interrupt it, we
    // should kept it in left_events and report them.
    std::vector<Event> left_events_;
    KeyseqManager* esc_keyseq_manager_ = nullptr;
    Mode mode_ = Mode::kNone;  // const

    bool init_ = false;

    GlobalOpts* global_opts_;
};

}  // namespace mango