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

size_t DrawLine(Terminal& term, std::string_view line, const Pos& pos,
                size_t view_col, size_t width, size_t screen_row,
                size_t screen_col,
                const std::vector<const std::vector<Highlight>*>* highlights,
                Terminal::AttrPair fallback_attr, int tabstop) {
    std::vector<int64_t> highlights_i;
    if (highlights) {
        highlights_i.resize(highlights->size());
        for (size_t i = 0; i < highlights->size(); i++) {
            highlights_i[i] = LocateInPos(*(*highlights)[i], pos);
        }
    }

    Character character;
    size_t cur_view_col = 0;
    size_t offset = pos.byte_offset;
    while (offset < line.size()) {
        int character_width;
        int byte_len;
        Result res = ThisCharacter(line, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        character_width = character.Width();
        if (cur_view_col < view_col) {
            ;
        } else if (cur_view_col >= view_col &&
                   cur_view_col +
                           (character_width <= 0 ? 1 : character_width) <=
                       width + view_col) {
            // Decide attr
            Terminal::AttrPair attr = fallback_attr;
            if (highlights) {
                for (size_t i = 0; i < highlights->size(); i++) {
                    int64_t highlight_i = highlights_i[i];
                    const auto& highlight = *(*highlights)[i];
                    if (highlight_i != static_cast<int64_t>(highlight.size()) &&
                        highlight[highlight_i].range.PosInMe(
                            {pos.line, offset})) {
                        attr = highlight[highlight_i].attr;
                        break;
                    }
                }
            }
            if (character_width <= 0) {
                // we meet unprintable or zero-width character
                char c;
                if (character.Ascii(c) && c == '\t') {
                    int space_count = tabstop - cur_view_col % tabstop;
                    while (space_count > 0 &&
                           cur_view_col + 1 <= width + view_col) {
                        int cur_screen_col =
                            cur_view_col - view_col + screen_col;
                        Codepoint space = kSpaceChar;
                        // Just in view, render the character
                        Result ret = term.SetCell(cur_screen_col, screen_row,
                                                  &space, 1, attr);
                        MGO_ASSERT(ret == kOk);
                        space_count--;
                        cur_view_col++;
                    }
                    character_width = 0;
                } else {
                    MGO_LOG_INFO(
                        "Meet non-printable or wcwidth == 0 character");
                    character.Clear();
                    character.Push(kReplacementChar);
                    character_width = kReplacementCharWidth;
                }
            }
            if (character_width > 0) {
                int cur_screen_col = cur_view_col - view_col + screen_col;
                Result ret = term.SetCell(cur_screen_col, screen_row,
                                          character.Codepoints(),
                                          character.CodePointCount(), attr);
                MGO_ASSERT(ret == kOk);
            }
        } else {
            break;
        }
        offset += byte_len;
        cur_view_col += character_width;

        // Try goto next syntax highlight range
        if (highlights) {
            for (size_t i = 0; i < highlights->size(); i++) {
                const auto& highlight = *(*highlights)[i];
                while (highlights_i[i] <
                           static_cast<int64_t>(highlight.size()) &&
                       highlight[highlights_i[i]].range.PosAfterMe(
                           {pos.line, offset})) {
                    highlights_i[i]++;
                }
            }
        }
    }
    return offset;
}

}  // namespace mango