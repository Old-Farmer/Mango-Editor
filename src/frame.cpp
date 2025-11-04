#include "frame.h"

#include <stdint.h>

#include <gsl/util>

#include "buffer.h"
#include "coding.h"
#include "cursor.h"
#include "options.h"
#include "syntax.h"

namespace mango {

Frame::Frame(Buffer* buffer, Cursor* cursor, Options* options,
             SyntaxParser* parser) noexcept
    : buffer_(buffer), cursor_(cursor), parser_(parser), options_(options) {}

static int64_t LocateInPos(const std::vector<Highlight>& highlight,
                           const Pos& pos) {
    int64_t left = 0, right = highlight.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        if (highlight[mid].range.PosIn(pos)) {
            return mid;
        } else if (highlight[mid].range.PosBefore(pos)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return left;
}

void Frame::Draw() {
    assert(buffer_ != nullptr);
    assert(buffer_->IsLoad());

    int64_t highlight_i = -1;
    const std::vector<Highlight>* syntax_highlight = nullptr;
    if (parser_) {
        auto syntax_context = parser_->GetBufferSyntaxContext(buffer_);
        if (syntax_context) {
            syntax_highlight = &syntax_context->syntax_highlight;
        }
    }

    if (wrap_) {
        // TODO: wrap the content
        assert(false);
    } else {
        const std::vector<Line>& lines = buffer_->lines();
        std::vector<uint32_t> codepoints;
        for (size_t win_r = 0; win_r < height_; win_r++) {
            int screen_r = win_r + row_;
            size_t b_view_r = win_r + b_view_line_;

            if (lines.size() <= b_view_r) {
                uint32_t codepoint = kTildeChar;
                term_->SetCell(col_, screen_r, &codepoint, 1,
                               options_->attr_table[kNormal]);
                continue;
            }

            if (syntax_highlight) {
                if (highlight_i == -1) {
                    highlight_i = LocateInPos(*syntax_highlight, {b_view_r, 0});
                } else {
                    while (highlight_i <
                               static_cast<int>(syntax_highlight->size()) &&
                           (*syntax_highlight)[highlight_i].range.PosAfter(
                               {b_view_r, 0})) {
                        highlight_i++;
                    }
                }
            }
            const std::string& cur_line = lines[b_view_r].line_str;
            std::vector<uint32_t> character;
            size_t cur_b_view_c = 0;
            size_t offset = 0;
            while (offset < cur_line.size()) {
                int character_width;
                int byte_len;
                Result res = NextCharacterInUtf8(cur_line, offset, character,
                                                 byte_len, character_width);
                assert(res == kOk);
                if (cur_b_view_c < b_view_col_) {
                    ;
                } else if (cur_b_view_c >= b_view_col_ &&
                           cur_b_view_c + character_width <=
                               width_ + b_view_col_) {
                    if (character[0] == kTabChar) {
                        int space_count = options_->tabstop -
                                          cur_b_view_c % options_->tabstop;
                        while (space_count > 0 &&
                               cur_b_view_c + 1 <= width_ + b_view_col_) {
                            int screen_c = cur_b_view_c - b_view_col_ + col_;
                            uint32_t space = kSpaceChar;
                            // Just in view, render the character
                            int ret =
                                term_->SetCell(screen_c, screen_r, &space, 1,
                                               options_->attr_table[kNormal]);
                            if (ret == kTermOutOfBounds) {
                                // User resize the screen now, just skip the
                                // left cols in this row
                                break;
                            }
                            space_count--;
                            cur_b_view_c++;
                        }
                        character_width = 0;
                    } else {
                        int screen_c = cur_b_view_c - b_view_col_ + col_;

                        Terminal::AttrPair attr;
                        // Decide attr
                        if (syntax_highlight == nullptr ||
                            static_cast<int64_t>(syntax_highlight->size()) ==
                                highlight_i) {
                            attr = options_->attr_table[kNormal];
                        } else {
                            if ((*syntax_highlight)[highlight_i].range.PosIn(
                                    {b_view_r, offset})) {
                                attr = (*syntax_highlight)[highlight_i].attr;
                            } else {
                                attr = options_->attr_table[kNormal];
                            }
                        }

                        // Just in view, render the character
                        int ret =
                            term_->SetCell(screen_c, screen_r, character.data(),
                                           character.size(), attr);
                        if (ret == kTermOutOfBounds) {
                            // User resize the screen now, just skip the left
                            // cols in this row
                            break;
                        }
                    }
                } else {
                    break;
                }
                offset += byte_len;
                cur_b_view_c += character_width;

                // Try goto next syntax highlight range
                if (syntax_highlight) {
                    while (highlight_i <
                               static_cast<int64_t>(syntax_highlight->size()) &&
                           (*syntax_highlight)[highlight_i].range.PosAfter(
                               {b_view_r, offset})) {
                        highlight_i++;
                    }
                }
            }
        }
    }
}

bool Frame::In(size_t s_col, size_t s_row) {
    return s_col >= col_ && s_col < s_col + width_ && s_row >= row_ &&
           s_row < row_ + height_;
}

void Frame::MakeCursorVisible() {
    // Calculate the cursor pos if we put the buffer from (0, 0)
    size_t row = cursor_->line;

    const std::string& cur_line = buffer_->lines()[cursor_->line].line_str;
    std::vector<uint32_t> character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    cursor_->character_in_line = 0;
    while (offset < cursor_->byte_offset) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        offset += byte_len;
        if (character[0] == kTabChar) {
            cur_b_view_c +=
                options_->tabstop - cur_b_view_c % options_->tabstop;
        } else {
            cur_b_view_c += character_width;
        }
        cursor_->character_in_line++;
    }

    // some opretions makes want change, reset it
    if (!cursor_->b_view_col_want.has_value()) {
        cursor_->b_view_col_want = cur_b_view_c;
    }

    // adjust col of view
    if (cur_b_view_c < b_view_col_) {
        b_view_col_ = cur_b_view_c;
    } else if (cur_b_view_c - b_view_col_ >= width_) {
        b_view_col_ = cur_b_view_c + 2 - width_;
    }

    // adjust row of view
    if (row < b_view_line_) {
        b_view_line_ = row;
    } else if (row - b_view_line_ >= height_) {
        b_view_line_ = row + 1 - height_;
    }

    cursor_->s_row = row - b_view_line_ + row_;
    cursor_->s_col = cur_b_view_c - b_view_col_ + col_;
}

size_t Frame::SetCursorByBViewCol(size_t b_view_col) {
    size_t cur_b_view_line = cursor_->line;
    size_t target_b_view_col = b_view_col;
    const std::string& cur_line = buffer_->lines()[cur_b_view_line].line_str;
    std::vector<uint32_t> character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    while (offset < cur_line.size()) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        if (cur_b_view_c <= target_b_view_col &&
            target_b_view_col < cur_b_view_c + character_width) {
            cursor_->line = cur_b_view_line;
            cursor_->byte_offset = offset;
            return cur_b_view_c;
        }
        offset += byte_len;
        if (character[0] == kTabChar) {
            cur_b_view_c +=
                options_->tabstop - cur_b_view_c % options_->tabstop;
        } else {
            cur_b_view_c += character_width;
        }
    }
    cursor_->byte_offset = offset;
    return cur_b_view_c;
}

void Frame::SetCursorHint(size_t s_row, size_t s_col) {
    assert(buffer_);
    assert(In(s_col, s_row));

    auto _ = gsl::finally([this] { cursor_->DontHoldColWant(); });

    size_t cur_b_view_row = s_row - row_ + b_view_line_;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->lines().size()) {
        cursor_->line = buffer_->lines().size() - 1;
        cursor_->byte_offset = buffer_->lines().back().line_str.size();
        return;
    }

    cursor_->line = cur_b_view_row;

    // not empty line
    // search througn line
    size_t target_b_view_col = s_col - col_ + b_view_col_;
    SetCursorByBViewCol(target_b_view_col);
}

void Frame::ScrollRows(int64_t count, bool cursor_in_frame) {
    assert(buffer_);
    if (count > 0) {
        b_view_line_ =
            std::min(b_view_line_ + count, buffer_->lines().size() - 1);
    } else {
        b_view_line_ =
            std::max<int64_t>(static_cast<int64_t>(b_view_line_) + count,
                              0);  // cast is necessary here
    }

    if (!cursor_in_frame) {
        return;
    }

    if (cursor_->line < b_view_line_) {
        cursor_->line = b_view_line_;
    } else if (cursor_->line >= b_view_line_ + height_) {
        cursor_->line = b_view_line_ + height_ - 1;
    }
    assert(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
}

void Frame::ScrollCols(int64_t count) { (void)count; }

void Frame::CursorGoRight() {
    assert(buffer_);
    auto _ = gsl::finally([this] { cursor_->DontHoldColWant(); });

    // end
    if (buffer_->lines()[cursor_->line].line_str.size() ==
        cursor_->byte_offset) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = NextCharacterInUtf8(buffer_->lines()[cursor_->line].line_str,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    assert(ret == kOk);
    cursor_->byte_offset += len;
}

void Frame::CursorGoLeft() {
    assert(buffer_);
    auto _ = gsl::finally([this] { cursor_->DontHoldColWant(); });

    // home
    if (cursor_->byte_offset == 0) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = PrevCharacterInUtf8(buffer_->lines()[cursor_->line].line_str,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    assert(ret == kOk);
    cursor_->byte_offset -= len;
}

void Frame::CursorGoUp() {
    assert(buffer_);

    // first line
    if (cursor_->line == 0) {
        return;
    }

    cursor_->line--;
    assert(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
}

void Frame::CursorGoDown() {
    assert(buffer_);

    // last line
    if (buffer_->lines().size() - 1 == cursor_->line) {
        return;
    }

    cursor_->line++;
    assert(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
}

void Frame::CursorGoHome() {
    assert(buffer_);
    cursor_->byte_offset = 0;
    cursor_->DontHoldColWant();
}

void Frame::CursorGoEnd() {
    assert(buffer_);
    cursor_->byte_offset = buffer_->lines()[cursor_->line].line_str.size();
    cursor_->DontHoldColWant();
}

void Frame::DeleteCharacterBeforeCursor() {
    Range range;
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }
        range = {{cursor_->line - 1,
                  buffer_->lines()[cursor_->line - 1].line_str.size()},
                 {cursor_->line, 0}};
    } else {
        std::vector<uint32_t> charater;
        int len, character_width;
        Result ret = PrevCharacterInUtf8(
            buffer_->lines()[cursor_->line].line_str, cursor_->byte_offset,
            charater, len, character_width);
        assert(ret == kOk);
        range = {{cursor_->line, cursor_->byte_offset - len},
                 {cursor_->line, cursor_->byte_offset}};
    }
    Pos pos;
    if (buffer_->Delete(range, pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
    UpdateHighlight();
}

void Frame::AddStringAtCursor(std::string str, const Pos* cursor_pos) {
    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    if (buffer_->Add({cursor_->line, cursor_->byte_offset}, std::move(str),
                     cursor_pos != nullptr, pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
    UpdateHighlight();
}

void Frame::TabAtCursor() {
    if (!options_->tabspace) {
        AddStringAtCursor(kTab);
        return;
    }

    int64_t cur_b_view_row = cursor_->line;
    const std::string& cur_line = buffer_->lines()[cur_b_view_row].line_str;
    std::vector<uint32_t> character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    while (offset < cursor_->byte_offset) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        offset += byte_len;
        cur_b_view_c += character_width;
    }
    int need_space = options_->tabstop - cur_b_view_c % options_->tabstop;
    AddStringAtCursor(std::string(need_space, kSpaceChar));
}

void Frame::Redo() {
    Pos pos;
    if (buffer_->Redo(pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
    UpdateHighlight();
}

void Frame::Undo() {
    Pos pos;
    if (buffer_->Undo(pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
    parser_->SyntaxHighlightAfterEdit(buffer_);
    UpdateHighlight();
}

void Frame::UpdateHighlight() {
    if (parser_) parser_->SyntaxHighlightAfterEdit(buffer_);
}

}  // namespace mango