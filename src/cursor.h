#pragma once

#include "term.h"

namespace mango {

class Window;

struct Cursor {
    // row and col in screen
    // this is synced with line & byte_offset after Preprocess
    int s_row = -1;
    int s_col = -1;

    // this is synced with line & byte_offset after Preprocess
    int64_t character_in_line = 0;

    // line and byte_offset of the buffer of the specific frame
    int64_t line = 0;
    int64_t byte_offset = 0;

    // when cursor move up/down or scroll rows make cursor move,
    // b_view_col should be the same.
    // Editing or cursor move left/right/jump lose this effect
    int64_t b_view_col_want = -1;

    Window* in_window; // nullptr means in MangoPeel

    // TODO: other info
};

}  // namespace mango
