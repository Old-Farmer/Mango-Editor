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

    // line and byte_offset of the buffer of the specific window
    int64_t line = 0;
    int64_t byte_offset = 0;

    // when go up and go down using cursor, b_view_col should be
    // the same
    // not used now
    // TODO: support this
    int64_t b_view_col_want = -1;

    Window* in_window;

    // TODO: other info
};

}  // namespace mango
