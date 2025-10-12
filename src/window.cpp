#include "window.h"

#include <algorithm>

#include "coding.h"

namespace mango {
Window::Window() : id_(AllocId()) {}

Window::Window(Buffer* buffer) : buffer_(buffer), id_(AllocId()) {}

void Window::Draw() {
    assert(buffer_ != nullptr);
    assert(buffer_->IsReadAll());
    // bool need_to_render_cursor = cursor_->in_window == this;
    if (wrap_) {
        // TODO: wrap the content
        assert(false);
    } else {
        std::vector<Line>& lines = buffer_->lines();
        std::vector<uint32_t> codepoints;
        for (int win_r = 0; win_r < height_; win_r++) {
            int screen_r = win_r + row_;
            int b_view_r = win_r + b_view_row_;

            if (lines.size() <= b_view_r) {
                uint32_t codepoint = '~';
                term_->SetCell(col_, screen_r, &codepoint, 1);
                continue;
            }

            const std::string& cur_line = lines[b_view_r].line;
            std::vector<uint32_t> character;
            int cur_b_view_c = 0;
            int offset = 0;
            while (offset < cur_line.size()) {
                int character_width;
                int byte_len;
                Result res = NextCharacterInUtf8(cur_line, offset, character,
                                                 byte_len, character_width);
                if (res != kOk) {
                    break;
                }
                if (cur_b_view_c < b_view_col_) {
                    ;
                } else if (cur_b_view_c >= b_view_col_ &&
                           cur_b_view_c + character_width <=
                               width_ + b_view_col_) {
                    int screen_c = cur_b_view_c - b_view_col_ + col_;
                    // Just in view, render the character
                    int ret = term_->SetCell(
                        screen_c, screen_r, character.data(), character.size());
                    if (ret == kTermOutOfBounds) {
                        // User resize the screen now, just skip the left cols
                        // in this row
                        break;
                    }

                    // // maybe this is the location to render the cursor.
                    // if (need_to_render_cursor) {
                    //     if (cursor_->line == b_view_r &&
                    //         cursor_->byte_offset == offset) {
                    //         ret = term_->SetCursor(screen_c, screen_r);
                    //         if (ret == kTermOutOfBounds) {
                    //             break;
                    //         }
                    //         cursor_->s_col = screen_c;
                    //         cursor_->s_row = screen_r;
                    //         need_to_render_cursor = false;
                    //     }
                    // }
                } else {
                    break;
                }
                offset += byte_len;
                cur_b_view_c += character_width;
            }

            // // maybe this is the location to render the cursor.
            // // !! end of line pos
            // if (need_to_render_cursor) {
            //     if (cursor_->line == b_view_r &&
            //         cursor_->byte_offset == offset) {
            //         int screen_c = cur_b_view_c - b_view_col_ + col_;
            //         int ret = term_->SetCursor(screen_c, screen_r);
            //         if (ret == kTermOutOfBounds) {
            //             break;
            //         }
            //         cursor_->s_col = screen_c;
            //         cursor_->s_row = screen_r;
            //         need_to_render_cursor = false;
            //     }
            // }

            // Line& cur_line = lines[b_view_r];
            // codepoints.clear();
            // cur_line.RenderLine(&codepoints);
            // for (int i = 0; i < cur_line.render_line.size() - 1; i++) {
            //     if (cur_line.render_line[i].col >= b_view_col_ &&
            //         cur_line.render_line[i + 1].col <= width_ + b_view_col_)
            //         { int ret = term_->SetCell(
            //             cur_line.render_line[i].col - b_view_col_ + col_,
            //             screen_r, &codepoints[i], 1);
            //         if (ret == kTermOutOfBounds) {
            //             // User resize the screen now, just skip the left
            //             cols
            //             // in this row
            //             break;
            //         }
            //     }
            // }
        }
    }
}

void Window::MakeCursorVisible() {
    // Calculate the cursor pos if we put the buffer from (0, 0)
    int row = cursor_->line;

    const std::string& cur_line = buffer_->lines()[cursor_->line].line;
    std::vector<uint32_t> character;
    int cur_b_view_c = 0;
    int offset = 0;
    while (offset < cur_line.size() && offset != cursor_->byte_offset) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        offset += byte_len;
        cur_b_view_c += character_width;
    }

    // adjust col of view
    if (cur_b_view_c < b_view_col_) {
        b_view_col_ = cur_b_view_c;
    } else if (cur_b_view_c - b_view_col_ >= width_) {
        b_view_col_ = cur_b_view_c + 2 - width_;
    }

    // adjust row of view
    if (row < b_view_row_) {
        b_view_row_ = row;
    } else if (row - b_view_row_ >= height_) {
        b_view_row_ = row + 2 - height_;
    }

    cursor_->s_row = row - b_view_row_ + row_;
    cursor_->s_col = cur_b_view_c - b_view_col_ + col_;
}

void Window::SetCursorByBViewCol(int b_view_col) {
    int cur_b_view_row = cursor_->line;
    int target_b_view_col = b_view_col;
    const std::string& cur_line = buffer_->lines()[cur_b_view_row].line;
    std::vector<uint32_t> character;
    int cur_b_view_c = 0;
    int offset = 0;
    while (offset < cur_line.size()) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        if (cur_b_view_c <= target_b_view_col &&
            target_b_view_col < cur_b_view_c + character_width) {
            cursor_->line = cur_b_view_row;
            cursor_->byte_offset = offset;
            return;
        }
        offset += byte_len;
        cur_b_view_c += character_width;
    }
    cursor_->byte_offset = offset;
}

void Window::SetCursorHint(int s_row, int s_col) {
    assert(buffer_);
    assert(s_row >= row_ && s_row < row_ + height_);
    assert(s_col >= col_ && s_col < col_ + width_);

    int cur_b_view_row = s_row - row_ + b_view_row_;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->lines().size()) {
        cursor_->line = buffer_->lines().size() - 1;
        cursor_->byte_offset = buffer_->lines().back().line.size();
        return;
    }

    cursor_->line = cur_b_view_row;

    // not empty line
    // search througn line
    int target_b_view_col = s_col - col_ + b_view_col_;
    SetCursorByBViewCol(target_b_view_col);
}

// void Window::SetCursorHint(Cursor& cursor, int s_row, int s_col) {
//     assert(buffer_);
//     assert(s_row >= row_ && s_row < row_ + height_);
//     assert(s_col >= col_ && s_col < col_ + width_);

//     int cur_b_view_row = s_row - row_ + b_view_row_;
//     if (cur_b_view_row >= buffer_->lines().size()) {
//         cursor.row = s_row;
//         cursor.col =
//             std::min(col_, s_col);  // empty line, locate in the first col
//         return;
//     }

//     const std::vector<RenderChar>& cur_render_line =
//         buffer_->lines()[s_row - row_ + b_view_row_].render_line;

//     int cur_b_view_col = b_view_col_ + s_col - col_;
//     int l = 0, r = cur_render_line.size() - 1;
//     while (l <= r) {
//         int mid = l + (r - l) / 2;
//         if (cur_render_line[mid].col == cur_b_view_col) {
//             l = mid;
//             break;
//         } else if (cur_render_line[mid].col < cur_b_view_col) {
//             l = mid + 1;
//         } else {
//             r = mid - 1;
//         }
//     }
//     if (l == cur_render_line.size()) {
//         l = l - 1;
//     }
//     cursor.col = cur_render_line[l].col - b_view_col_ + col_;
//     cursor.row = s_row;
// }

void Window::ScrollRows(int count) {
    assert(buffer_);
    if (count > 0) {
        b_view_row_ =
            std::min<int>(b_view_row_ + count, buffer_->lines().size() - 1);
    } else {
        b_view_row_ = std::max<int>(b_view_row_ + count, 0);
    }

    if (cursor_->line < b_view_row_) {
        SetCursorHint(row_, cursor_->s_col);
    } else if (cursor_->line >= b_view_row_ + height_) {
        SetCursorHint(row_ + height_ - 1, cursor_->s_col);
    }
}

void Window::ScrollCols(int count) {}

void Window::CursorGoRight() {
    assert(buffer_);

    // end
    if (buffer_->lines()[cursor_->line].line.size() == cursor_->byte_offset) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = NextCharacterInUtf8(buffer_->lines()[cursor_->line].line,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    if (ret != kOk) {
        assert(false);
        return;
    }
    cursor_->byte_offset += len;
}
void Window::CursorGoLeft() {
    assert(buffer_);

    // home
    if (cursor_->byte_offset == 0) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = PrevCharacterInUtf8(buffer_->lines()[cursor_->line].line,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    if (ret != kOk) {
        return;
    }
    cursor_->byte_offset -= len;
}
void Window::CursorGoUp() {
    assert(buffer_);

    // first line
    if (cursor_->line == 0) {
        return;
    }

    cursor_->line--;
    SetCursorByBViewCol(cursor_->s_col - col_ + b_view_col_);
}
void Window::CursorGoDown() {
    assert(buffer_);

    // last line
    if (buffer_->lines().size() - 1 == cursor_->line) {
        return;
    }

    cursor_->line++;
    SetCursorByBViewCol(cursor_->s_col - col_ + b_view_col_);
}

void Window::CursorGoHome() {
    assert(buffer_);
    cursor_->byte_offset = 0;
}
void Window::CursorGoEnd() {
    assert(buffer_);
    cursor_->byte_offset = buffer_->lines()[cursor_->line].line.size();
}

void Window::DeleteCharacterBeforeCursor() {
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }

        // merge two lines
        buffer_->lines()[cursor_->line - 1].line.append(
            buffer_->lines()[cursor_->line].line);
        buffer_->lines().erase(buffer_->lines().begin() + cursor_->line);
        cursor_->line = cursor_->line - 1;
        cursor_->byte_offset = buffer_->lines()[cursor_->line].line.size();
    } else {
        std::vector<uint32_t> charater;
        int len, character_width;
        Result ret = PrevCharacterInUtf8(buffer_->lines()[cursor_->line].line,
                                         cursor_->byte_offset, charater, len,
                                         character_width);
        assert(ret == kOk);
        cursor_->byte_offset -= len;
        buffer_->lines()[cursor_->line].line.erase(cursor_->byte_offset, len);
    }
}

void Window::AddCharacterAtCursor(const std::string& character) {
    if (character == kNewLine) {
        auto& lines = buffer_->lines();
        std::string new_line = lines[cursor_->line].line.substr(
            cursor_->byte_offset,
            lines[cursor_->line].line.size() - cursor_->byte_offset);
        buffer_->lines().insert(buffer_->lines().begin() + cursor_->line + 1,
                                std::move(new_line));
        lines[cursor_->line].line.resize(cursor_->byte_offset);

        cursor_->line++;
        cursor_->byte_offset = 0;
    } else {
        buffer_->lines()[cursor_->line].line.insert(cursor_->byte_offset,
                                                    character);
        cursor_->byte_offset += character.size();
    }
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango