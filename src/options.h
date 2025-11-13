#pragma once

#include <vector>

#include "term.h"

namespace mango {

enum CharacterType : int {
    kNormal = 0,
    kReverse,
    kSelection,
    kMenu,
    kLineNumber,

    kKeyword,
    kTypeBuiltin,
    kOperator,
    kString,
    kComment,
    kNumber,
    kConstant,
    kFunction,
    kType,
    kVariable,
    kDelimiter,
    kProperty,
    kLabel,

    __kCharacterTypeCount,
};

enum class LineNumberType {
    kNone,
    kAboslute,
    // kRelative, // Not suppot now
};

// Use many magic numbers here. Just for convinience.
struct Options {
    Options();

    int poll_event_timeout_ms = -1;
    int64_t scroll_rows_per_mouse_wheel_scroll = 3;
    int escape_timeout_ms = 50;

    int cursor_start_holding_interval_ms = 500;

    std::vector<Terminal::AttrPair> attr_table;

    int tabstop = 4;
    bool tabspace = true;

    bool auto_indent = true;

    bool auto_pair = true;

    LineNumberType line_number = LineNumberType::kAboslute;

    size_t status_line_left_indent = 2;
    size_t status_line_right_indent = 2;
    size_t status_line_sep_width = 2;

    size_t cmp_menu_max_height = 8;
    size_t cmp_menu_max_width = 20;

    // NOTE: must greater than 0
    size_t buffer_hitstory_max_item_cnt = 50;
};

// Some options only useful when the program just starts
struct InitOptions {
    std::vector<const char*> begin_files;
};


}  // namespace mango
