#pragma once

#include <cstddef>
#include <optional>

namespace mango {

class Window;

struct Cursor {
    // row and col in screen
    // this is synced with line & byte_offset after Preprocess
    int s_row = -1;
    int s_col = -1;

    // this is synced with line & byte_offset after Preprocess
    size_t character_in_line = 0;

    // line and byte_offset of the buffer of the specific frame
    size_t line = 0;
    size_t byte_offset = 0;

    // when cursor move up/down or scroll rows make cursor move,
    // b_view_col should be the same.
    // Editing or cursor move left/right/jump lose this effect
    std::optional<size_t> b_view_col_want;

    Window* in_window;  // nullptr means in MangoPeel
    Window* restore_from_peel = nullptr;

    // TODO: other info

    void DontHoldColWant() { b_view_col_want.reset(); }
};

}  // namespace mango
