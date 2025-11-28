#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "syntax.h"

namespace mango {

// Draw a line on the terminal.
// in highlights, index 0 means highest priority.
size_t DrawLine(Terminal& term, std::string_view line, const Pos& pos,
                size_t view_col, size_t width, size_t screen_row,
                size_t screen_col,
                const std::vector<const std::vector<Highlight>*>* highlights,
                Terminal::AttrPair fallback_attr, int tabstop);

}  // namespace mango