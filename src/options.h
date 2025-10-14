#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "term.h"

namespace mango {

enum class Action : int { kQuit = 0 };

// Use many magic numbers here. Just for convinience.
struct Options {
    int poll_event_timeout_ms = -1;
    int64_t scroll_rows_per_mouse_wheel_scroll = 3;
    std::vector<const char*> begin_files;

    // // Now only Support single Keys
    // std::unordered_map<Terminal::KeyInfo, Action> keymaps;

    // NOTE: Change carefully
    // See Terminall::Init
    // TODO: better color control
    std::vector<Terminal::AttrPair> attr_table = {
        {Terminal::kDefault, Terminal::kDefault}, // normal
        {Terminal::kBlack, Terminal::kWhite}, // selection
        {Terminal::kYellow, Terminal::kDefault}, // key word
        {Terminal::kGreen, Terminal::kDefault}, // string
        {Terminal::kCyan, Terminal::kDefault}, // comment
    };

    int tabstop = 4;
    bool tabspace = true;
};

}  // namespace mango
