#pragma once

#include <chrono>

#include "state.h"

namespace mango {

struct Mouse {
    MouseState state = MouseState::kReleased;
    // Two quickly left(right) clicks at same and not released, a
    // cursor starts holding: e.g. left release left, interval very short,
    // holding start pos: second left pos; Or two continous left(right) clicks,
    // holding start pos: first click pos.
    //  If release, holding end.
    //  if next left(right) is different from start pos, selection start
    std::chrono::steady_clock::time_point last_click_time =
        std::chrono::steady_clock::now();  // only record when kReleased to
                                           // kXXNotReleased
};

}  // namespace mango
