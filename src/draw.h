#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "syntax.h"

namespace mango {

// Draw a line on the terminal.
// in highlights, index 0 means highest priority.
// return the not drawn start byte_offset of the line.
size_t DrawLine(Terminal& term, std::string_view line, const Pos& begin_pos,
                size_t begin_view_col, size_t width, size_t screen_row,
                size_t screen_col,
                const std::vector<const std::vector<Highlight>*>* highlights,
                Terminal::AttrPair fallback_attr, int tabstop, bool wrap);

// Nearly Same as the above, but not draw at terminal.
// If target_byte_offset != nullptr, if corresponding character can be drawed in
// the row, this func will stop, and stop_at_target will set to true. If
// target_byte_offset == line.size(), and all character can be drawed,
// stop_at_target will also set to true.
size_t ArrangeLine(std::string_view line, size_t begin_byte_offset,
                   size_t begin_view_col, size_t width, int tabstop, bool wrap,
                   size_t* end_view_col = nullptr,
                   size_t* target_byte_offset = nullptr,
                   bool* stop_at_target = nullptr,
                   size_t* character_cnt = nullptr);

// Return screen row cnt of a line for wrap.
size_t ScreenRows(std::string_view line, size_t width, int tabstop);

}  // namespace mango