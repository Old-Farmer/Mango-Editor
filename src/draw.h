#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "syntax.h"

namespace charxed {

// return the not drawn start byte_offset of the line, other is the same as the
// other version of DrawLine.
std::tuple<size_t, size_t> DrawLine(
    Terminal& term, std::string_view line, const Pos& begin_pos,
    size_t begin_view_col, size_t width, size_t screen_row, size_t screen_col,
    const std::vector<const std::vector<Highlight>*>* highlights,
    Theme scheme, const Terminal::AttrPair& fallback_attr,
    int64_t trailing_white_begin, int tabstop, bool wrap, bool full_line);

// Draw a line on the terminal.
// in highlights, index 0 means highest priority.
// if full_line == true, this function will draw the screen line at every cell
// wherever there as to put a character.
// return the not drawn iter of this line and the end_view_col
std::tuple<TextTree::Iterator, size_t> DrawLine(
    Terminal& term, size_t line, const TextTree::TextView& line_view,
    TextTree::Iterator iter, size_t begin_view_col, size_t width,
    size_t screen_row, size_t screen_col,
    const std::vector<const std::vector<Highlight>*>* highlights,
    Theme scheme, const Terminal::AttrPair& fallback_attr,
    int64_t trailing_white_begin, int tabstop, bool wrap, bool full_line);

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

TextTree::Iterator ArrangeLine(const TextTree::TextView& line,
                               TextTree::Iterator iter, size_t begin_view_col,
                               size_t width, int tabstop, bool wrap,
                               size_t* end_view_col = nullptr,
                               size_t* target_byte_offset = nullptr,
                               bool* stop_at_target = nullptr,
                               size_t* character_cnt = nullptr);

// Return screen row cnt of a line for wrap.
size_t ScreenRows(std::string_view line, size_t width, int tabstop);

size_t ScreenRows(const TextTree::TextView& line, size_t width, int tabstop);

}  // namespace charxed
