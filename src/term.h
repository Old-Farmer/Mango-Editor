// A simple wrapper of termbox2
// Maybe

#pragma once

#include <cassert>

#include "exception.h"
#include "logging.h"
#include "result.h"
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

class Terminal {
   private:
    Terminal();
    ~Terminal();

   public:
    MANGO_DELETE_COPY(Terminal);
    MANGO_DELETE_MOVE(Terminal);

    static Terminal& GetInstance() {
        static Terminal term;
        return term;
    }

    // throws TermException
    void Init();

    // throws TermException
    void Shutdown() {
        int ret = tb_shutdown();
        // assert(ret == TB_OK);
    }

    // throws TermException
    // or
    // return
    // kOk for ok
    // kTermOutOfBounds for out of bounds
    Result SetCell(int col, int row, uint32_t* codepoint, size_t n_codepoint) {
        int ret = tb_set_cell_ex(col, row, codepoint, n_codepoint, TB_DEFAULT,
                                 TB_DEFAULT);
        if (ret == TB_OK) {
            return kOk;
        } else if (ret == TB_ERR_OUT_OF_BOUNDS) {
            return kTermOutOfBounds;
        } else {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return kOk;
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
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return kOk;
    }

    // throws TermException
    void HideCursor() {
        int ret = tb_hide_cursor();
        if (ret != TB_OK) {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    int Height() {
        int ret = tb_height();
        if (ret < 0) {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return ret;
    }

    // throws TermException
    int Width() {
        int ret = tb_width();
        if (ret < 0) {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return ret;
    }

    // throws TermException
    void Clear() {
        int ret = tb_clear();
        if (ret != TB_OK) {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    void Present() {
        int ret = tb_present();
        if (ret != TB_OK) {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
    }

    // throws TermException
    bool Poll(int timeout_ms) {
        int ret;
        if (timeout_ms == -1) {
            ret = tb_poll_event(&event_);
        } else {
            ret = tb_peek_event(&event_, timeout_ms);
        }
        if (ret == TB_ERR_NO_EVENT) {
            return false;
        } else if (ret == TB_ERR_POLL && tb_last_errno() == EINTR) {
            return false;
        } else if (ret == TB_OK) {
            ;
        } else {
            MANGO_LOG_ERROR("%s", tb_strerror(ret));
            throw TermException("%s", tb_strerror(ret));
        }
        return true;
    }

    bool EventIsResize() { return event_.type == TB_EVENT_RESIZE; }

    bool EventIsKey() { return event_.type == TB_EVENT_KEY; }

    bool EventIsMouse() { return event_.type == TB_EVENT_MOUSE; }

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

    enum Mod {
        kAlt = TB_MOD_ALT,
        kCtrl = TB_MOD_CTRL,
        kShift = TB_MOD_SHIFT,
        kMotion = TB_MOD_MOTION
    };

    // special key xor codepoint
    // and mod
    // kCtrl and Shift only occur with kArrow...
    struct KeyInfo {
        SpecialKey special_key;
        uint32_t codepoint;
        Mod mod;

        bool IsSpecialKey() const noexcept { return codepoint == 0; }
    };

    ResizeInfo EventResizeInfo() const noexcept { return {event_.w, event_.h}; }
    MouseInfo EventMouseInfo() const noexcept {
        return {event_.x, event_.y, static_cast<MouseKey>(event_.key)};
    }
    KeyInfo EventKeyInfo() const noexcept {
        return {static_cast<SpecialKey>(event_.key), event_.ch,
                static_cast<Mod>(event_.mod)};
    }

    // Utilities funtions

    // -1 for not printable
    // 0 for combining character
    // 1 for 1 col
    // 2 for 2 col
    static int WCWidth(uint32_t ch) noexcept { return tb_wcwidth(ch); }

   private:
    tb_event event_;
};

}  // namespace mango