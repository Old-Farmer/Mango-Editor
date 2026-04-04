#pragma once

#include <cstddef>
#include <optional>

#include "pos.h"

namespace mango {

class Window;
struct Selection;
class GlobalOpts;
class Opts;
class MangoPeel;

struct Cursor {
    // row and col in screen
    // this is synced with line & byte_offset after Preprocess
    int s_row = -1;
    int s_col = -1;

    // this is synced with line & byte_offset after Preprocess
    size_t character_in_line = 0;

    // line and byte_offset of the buffer of the specific frame
    Pos pos = {0, 0};

    // when cursor move up/down or scroll rows make cursor move,
    // b_view_col should be the same.
    // Editing or cursor move left/right/jump lose this effect
    // NOTE: wrap, no wrap have different meanings.
    std::optional<size_t> b_view_col_want;

    // The cursor must be visible, means we must adjust the view to make the
    // cursor visible.
    bool must_visible = true;

    Window* in_window;  // nullptr means in MangoPeel
    Window* restore_from_peel = nullptr;

    // TODO: other info

    void DontHoldColWant() { b_view_col_want.reset(); }
    // void SetPos(Pos pos) {
    //     line = pos.line;
    //     byte_offset = pos.byte_offset;
    // }
    // Pos ToPos() { return {line, byte_offset}; }

    void SetScreenPos(int screen_col, int screen_row) {
        s_col = screen_col;
        s_row = screen_row;
    }
};

// A core state of cursor
struct CursorState {
    Pos pos;
    std::optional<size_t> b_view_col_want;

    CursorState(Cursor* cursor)
        : pos(cursor->pos), b_view_col_want(cursor->b_view_col_want) {}

    void SetCursor(Cursor* cursor) {
        cursor->pos = pos;
        cursor->b_view_col_want = b_view_col_want;
    }
    void DontHoldColWant() { b_view_col_want.reset(); }
};

}  // namespace mango
