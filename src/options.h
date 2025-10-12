#pragma once

#include <string>
#include "term.h"
#include <unordered_map>
#include <vector>

namespace mango {

enum class Action : int {
    kQuit = 0
};

// Use many magic numbers here. Just for convinience.
struct Options {
    int poll_event_timeout_ms = -1;
    int scroll_rows_per_mouse_wheel_scroll = 3;
    std::vector<const char*> begin_files;
    // // Now only Support single Keys
    // std::unordered_map<Terminal::KeyInfo, Action> keymaps;
};

}  // namespace mango
