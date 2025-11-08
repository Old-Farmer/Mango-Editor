#pragma once

#include <chrono>
#include <cstddef>
#include <optional>

namespace mango {

class Window;
class Selection;

enum class CursorState {
    kReleased,
    kLeftNotReleased,
    kRightNotReleased,
    kMiddleNotReleased,
    kLeftHolding,
};

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

    // Cursor states
    CursorState state_ = CursorState::kReleased;
    // Two quickly left(right) clicks at same and not released, a
    // cursor starts holding: e.g. left release left, interval very short,
    // holding start pos: second left pos; Or two continous left(right) clicks,
    // holding start pos: first click pos.
    //  If release, holding end.
    //  if next left(right) is different from start pos, selection start
    std::chrono::steady_clock::time_point last_click_time_ =
        std::chrono::steady_clock::now();  // only record when kReleased to
                                           // kXXNotReleased

    // TODO: other info

    void DontHoldColWant() { b_view_col_want.reset(); }
};

}  // namespace mango
