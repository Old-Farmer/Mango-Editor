#pragma once

#include <cstddef>
#include <optional>

namespace mango {

class Buffer;
struct Cursor;

struct CursorState {
    size_t line = 0;
    size_t byte_offset = 0;
    std::optional<size_t> b_view_col_want_;
    size_t character_in_line_ =
        0;  // for status line to show the state, no need to restore
};

// BufferView is a class tha represent a view of buffer, i.e. Which part of a
// buffer is showed in the screen.
struct BufferView {
    BufferView() {}

    // Wrap or no wrap: which line of buffer on the first frame row
    size_t line = 0;
    // No wrap: which col of buffer view on the first frame col
    size_t col = 0;
    // Wrap: When in wrap, a buffer line may be rendered in multi rows, dividing
    // a line into multi sublines. This member represents which sub line of
    // buffer on the first screen row.
    size_t subline = 0;

    std::optional<CursorState> cursor_state{std::in_place};

    void SaveCursorState(Cursor* cursor);
    void RestoreCursorState(Cursor* cursor, Buffer* buffer);
};

}  // namespace mango