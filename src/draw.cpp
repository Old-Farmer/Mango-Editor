#include "draw.h"

namespace mango {

// Locate a pos in ranges, select a range when a pos just locate in, or just
// after the pos.
static inline int64_t LocateInPos(const std::vector<Highlight>& highlight,
                                  const Pos& pos) {
    int64_t left = 0, right = highlight.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        if (highlight[mid].range.PosInMe(pos)) {
            return mid;
        } else if (highlight[mid].range.PosBeforeMe(pos)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return left;
}

// TODO: when wrap, do not break word. same as ArrangeLine and
// Frame::SetCursorByViewCol.
size_t DrawLine(Terminal& term, std::string_view line, const Pos& begin_pos,
                size_t begin_view_col, size_t width, size_t screen_row,
                size_t screen_col,
                const std::vector<const std::vector<Highlight>*>* highlights,
                Terminal::AttrPair fallback_attr, int tabstop, bool wrap) {
    std::vector<int64_t> highlights_i;
    if (highlights) {
        highlights_i.resize(highlights->size());
        for (size_t i = 0; i < highlights->size(); i++) {
            highlights_i[i] = LocateInPos(*(*highlights)[i], begin_pos);
        }
    }

    Character character;
    size_t view_col = 0;
    size_t byte_offset = begin_pos.byte_offset;
    while (byte_offset < line.size()) {
        int character_width;
        int byte_len;
        bool is_tab = false;
        Result res = ThisCharacter(line, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        character_width = character.Width();
        if (character_width == 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                is_tab = true;
                character_width = tabstop - view_col % tabstop;
            } else {
                character.Clear();
                character.Push(kReplacementChar);
                character_width = kReplacementCharWidth;
            }
        }
        if (view_col >= begin_view_col &&
            view_col + character_width <= width + begin_view_col) {
            // When wrap, we leave a cell for cursor.
            if (wrap && byte_offset + byte_len == line.size() &&
                view_col + character_width == width) {
                break;
            }
            // Decide attr
            Terminal::AttrPair attr = fallback_attr;
            if (highlights) {
                for (size_t i = 0; i < highlights->size(); i++) {
                    int64_t highlight_i = highlights_i[i];
                    const auto& highlight = *(*highlights)[i];
                    if (highlight_i != static_cast<int64_t>(highlight.size()) &&
                        highlight[highlight_i].range.PosInMe(
                            {begin_pos.line, byte_offset})) {
                        attr = highlight[highlight_i].attr;
                        break;
                    }
                }
            }

            int cur_screen_col = view_col - begin_view_col + screen_col;
            if (!is_tab) {
                term.SetCell(cur_screen_col, screen_row, character.Codepoints(),
                             character.CodePointCount(), attr);
            } else {
                Codepoint space = kSpaceChar;
                for (int i = 0; i < character_width; i++) {
                    term.SetCell(cur_screen_col, screen_row, &space, 1, attr);
                    cur_screen_col++;
                }
            }
        } else {
            break;
        }
        byte_offset += byte_len;
        view_col += character_width;

        // Try goto next syntax highlight range
        if (highlights) {
            for (size_t i = 0; i < highlights->size(); i++) {
                const auto& highlight = *(*highlights)[i];
                while (highlights_i[i] <
                           static_cast<int64_t>(highlight.size()) &&
                       highlight[highlights_i[i]].range.PosAfterMe(
                           {begin_pos.line, byte_offset})) {
                    highlights_i[i]++;
                }
            }
        }
    }
    return byte_offset;
}

size_t ArrangeLine(std::string_view line, size_t begin_byte_offset,
                   size_t begin_view_col, size_t width, int tabstop, bool wrap,
                   size_t* end_view_col, size_t* target_byte_offset,
                   bool* stop_at_target, size_t* character_cnt) {
    if (stop_at_target) {
        *stop_at_target = false;
    }
    if (character_cnt) {
        *character_cnt = 0;
    }

    Character character;
    size_t view_col = 0;
    size_t byte_offset = begin_byte_offset;
    while (byte_offset < line.size()) {
        int character_width;
        int byte_len;
        Result res = ThisCharacter(line, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        character_width = character.Width();
        if (character_width == 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                character_width = tabstop - view_col % tabstop;
            } else {
                character_width = kReplacementCharWidth;
            }
        }
        if (view_col >= begin_view_col &&
            view_col + character_width <= width + begin_view_col) {
            if (wrap && byte_offset + byte_len == line.size() &&
                view_col + character_width == width) {
                break;
            }
        } else {
            break;
        }
        // character at target byte offset can be rendered.
        if (target_byte_offset && *target_byte_offset == byte_offset) {
            if (stop_at_target) {
                *stop_at_target = true;
            }
            break;
        }
        if (character_cnt) {
            (*character_cnt)++;
        }
        byte_offset += byte_len;
        view_col += character_width;
    }

    if (target_byte_offset && *target_byte_offset == byte_offset &&
        byte_offset == line.size()) {
        if (stop_at_target) {
            *stop_at_target = true;
        }
    }
    if (end_view_col) {
        *end_view_col = view_col;
    }
    return byte_offset;
}

size_t ScreenRows(std::string_view line, size_t width, int tabstop) {
    size_t byte_offset = 0;
    size_t row_cnt = 0;
    do {
        byte_offset = ArrangeLine(line, byte_offset, 0, width, tabstop, true);
        row_cnt++;
    } while (byte_offset < line.size());
    return row_cnt;
}

}  // namespace mango