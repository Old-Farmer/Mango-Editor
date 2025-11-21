#pragma once

#include <cstddef>
#include <optional>

#include "pos.h"

namespace mango {

class Window;
class Selection;
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
    size_t line = 0;
    size_t byte_offset = 0;

    // when cursor move up/down or scroll rows make cursor move,
    // b_view_col should be the same.
    // Editing or cursor move left/right/jump lose this effect
    std::optional<size_t> b_view_col_want;

    Window* in_window;  // nullptr means in MangoPeel
    MangoPeel* peel;
    Window* restore_from_peel = nullptr;


    // TODO: other info

    void DontHoldColWant() { b_view_col_want.reset(); }
    void SetPos(const Pos& pos) {
        line = pos.line;
        byte_offset = pos.byte_offset;
    }
    Pos ToPos() { return {line, byte_offset}; }
};

}  // namespace mango
