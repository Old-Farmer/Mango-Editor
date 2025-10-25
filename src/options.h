#pragma once

#include <vector>

#include "term.h"

namespace mango {

enum class Action : int { kQuit = 0 };

enum CharacterType : int {
    kNormal = 0,
    kReverse,
    kSelection,
    kKeyword,  // "keyword"
    kString,   // "string"
    kComment,  // "comment"
    kNumber,   // "number"
    kMenu,
};

// Some options only useful when the program just starts
struct InitOptions {
    std::vector<const char*> begin_files;
};

// Use many magic numbers here. Just for convinience.
struct Options {
    int poll_event_timeout_ms = -1;
    int64_t scroll_rows_per_mouse_wheel_scroll = 3;

    // NOTE: Change carefully
    // See Terminall::Init
    // TODO: better color control
    std::vector<Terminal::AttrPair> attr_table = {
        {Terminal::kDefault, Terminal::kDefault},  // normal
        {Terminal::kDefault | Terminal::kReverse,
         Terminal::kDefault | Terminal::kReverse},  // reverse
        {Terminal::kDefault | Terminal::kReverse,
         Terminal::kDefault | Terminal::kReverse},  // selection
        {Terminal::kYellow, Terminal::kDefault},    // key word
        {Terminal::kGreen, Terminal::kDefault},     // string
        {Terminal::kCyan, Terminal::kDefault},      // comment
        {Terminal::kBlue, Terminal::kDefault},      // number
        {Terminal::kDefault, Terminal::kMagenta},   // menu
    };

    int tabstop = 4;
    bool tabspace = true;

    bool auto_indent = true;

    bool auto_pair = true;

    size_t status_line_left_indent = 2;
    size_t status_line_right_indent = 2;

    size_t cmp_menu_max_height = 8;
    size_t cmp_menu_max_width = 20;

    // NOTE: must greater than 0
    size_t buffer_hitstory_max_item_cnt = 50;
};

}  // namespace mango
