#include "window.h"

#include <algorithm>

#include "coding.h"
#include "gsl/gsl"

namespace mango {
Window::Window(Buffer* buffer, Cursor* cursor, Options* options)
    : buffer_(buffer),
      cursor_(cursor),
      id_(AllocId()),
      attr_table(&options->attr_table) {}

void Window::Draw() {
    assert(buffer_ != nullptr);
    assert(buffer_->IsReadAll());
    if (wrap_) {
        // TODO: wrap the content
        assert(false);
    } else {
        std::vector<Line>& lines = buffer_->lines();
        std::vector<uint32_t> codepoints;
        for (int win_r = 0; win_r < height_; win_r++) {
            int screen_r = win_r + row_;
            int64_t b_view_r = win_r + b_view_line_;

            if (lines.size() <= b_view_r) {
                uint32_t codepoint = '~';
                term_->SetCell(col_, screen_r, &codepoint, 1,
                               (*attr_table)[kNormal]);
                continue;
            }

            const std::string& cur_line = lines[b_view_r].line;
            std::vector<uint32_t> character;
            int64_t cur_b_view_c = 0;
            int64_t offset = 0;
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
                    int ret = term_->SetCell(screen_c, screen_r,
                                             character.data(), character.size(),
                                             (*attr_table)[kNormal]);
                    if (ret == kTermOutOfBounds) {
                        // User resize the screen now, just skip the left cols
                        // in this row
                        break;
                    }
                } else {
                    break;
                }
                offset += byte_len;
                cur_b_view_c += character_width;
            }
        }
    }
}

void Window::MakeCursorVisible() {
    // Calculate the cursor pos if we put the buffer from (0, 0)
    int row = cursor_->line;

    const std::string& cur_line = buffer_->lines()[cursor_->line].line;
    std::vector<uint32_t> character;
    int64_t cur_b_view_c = 0;
    int64_t offset = 0;
    cursor_->character_in_line = 0;
    while (offset < cur_line.size() && offset != cursor_->byte_offset) {
        int character_width;
        int byte_len;
        Result res = NextCharacterInUtf8(cur_line, offset, character, byte_len,
                                         character_width);
        assert(res == kOk);
        offset += byte_len;
        cur_b_view_c += character_width;
        cursor_->character_in_line++;
    }

    // some opretions makes want change, reset it
    if (cursor_->b_view_col_want == -1) {
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
        b_view_line_ = row + 2 - height_;
    }

    cursor_->s_row = row - b_view_line_ + row_;
    cursor_->s_col = cur_b_view_c - b_view_col_ + col_;
}

int64_t Window::SetCursorByBViewCol(int64_t b_view_col) {
    int64_t cur_b_view_row = cursor_->line;
    int64_t target_b_view_col = b_view_col;
    const std::string& cur_line = buffer_->lines()[cur_b_view_row].line;
    std::vector<uint32_t> character;
    int64_t cur_b_view_c = 0;
    int64_t offset = 0;
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
            return cur_b_view_c;
        }
        offset += byte_len;
        cur_b_view_c += character_width;
    }
    cursor_->byte_offset = offset;
    return cur_b_view_c;
}

void Window::SetCursorHint(int s_row, int s_col) {
    assert(buffer_);
    assert(s_row >= row_ && s_row < row_ + height_);
    assert(s_col >= col_ && s_col < col_ + width_);

    auto _ = gsl::finally([this] { cursor_->b_view_col_want = -1; });

    int64_t cur_b_view_row = s_row - row_ + b_view_line_;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->lines().size()) {
        cursor_->line = buffer_->lines().size() - 1;
        cursor_->byte_offset = buffer_->lines().back().line.size();
        return;
    }

    cursor_->line = cur_b_view_row;

    // not empty line
    // search througn line
    int64_t target_b_view_col = s_col - col_ + b_view_col_;
    SetCursorByBViewCol(target_b_view_col);
}

void Window::ScrollRows(int64_t count) {
    assert(buffer_);
    if (count > 0) {
        b_view_line_ =
            std::min<int64_t>(b_view_line_ + count, buffer_->lines().size() - 1);
    } else {
        b_view_line_ = std::max<int64_t>(b_view_line_ + count, 0);
    }

    if (cursor_->in_window != this) {
        return;
    }

    if (cursor_->line < b_view_line_) {
        cursor_->line = b_view_line_;
    } else if (cursor_->line >= b_view_line_ + height_) {
        cursor_->line = b_view_line_ + height_ - 1;
    }
    SetCursorByBViewCol(cursor_->b_view_col_want);
}

void Window::ScrollCols(int64_t count) {}

void Window::CursorGoRight() {
    assert(buffer_);
    auto _ = gsl::finally([this] { cursor_->b_view_col_want = -1; });

    // end
    if (buffer_->lines()[cursor_->line].line.size() == cursor_->byte_offset) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = NextCharacterInUtf8(buffer_->lines()[cursor_->line].line,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    assert(ret == kOk);
    cursor_->byte_offset += len;
}

void Window::CursorGoLeft() {
    assert(buffer_);
    auto _ = gsl::finally([this] { cursor_->b_view_col_want = -1; });

    // home
    if (cursor_->byte_offset == 0) {
        return;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = PrevCharacterInUtf8(buffer_->lines()[cursor_->line].line,
                                     cursor_->byte_offset, charater, len,
                                     character_width);
    assert(ret == kOk);
    cursor_->byte_offset -= len;
}

void Window::CursorGoUp() {
    assert(buffer_);

    // first line
    if (cursor_->line == 0) {
        return;
    }

    cursor_->line--;
    SetCursorByBViewCol(cursor_->b_view_col_want);
}

void Window::CursorGoDown() {
    assert(buffer_);

    // last line
    if (buffer_->lines().size() - 1 == cursor_->line) {
        return;
    }

    cursor_->line++;
    SetCursorByBViewCol(cursor_->b_view_col_want);
}

void Window::CursorGoHome() {
    assert(buffer_);
    cursor_->byte_offset = 0;
    cursor_->b_view_col_want = -1;
}

void Window::CursorGoEnd() {
    assert(buffer_);
    cursor_->byte_offset = buffer_->lines()[cursor_->line].line.size();
    cursor_->b_view_col_want = -1;
}

void Window::DeleteCharacterBeforeCursor() {
    if (!buffer_->IsReadAll()) {
        return;
    }

    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }

        // merge two lines
        int64_t prev_line_size =
            buffer_->lines()[cursor_->line - 1].line.size();
        buffer_->lines()[cursor_->line - 1].line.append(
            buffer_->lines()[cursor_->line].line);
        buffer_->lines().erase(buffer_->lines().begin() + cursor_->line);
        cursor_->line = cursor_->line - 1;
        cursor_->byte_offset = prev_line_size;
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
    buffer_->state() = BufferState::kModified;
    cursor_->b_view_col_want = -1;
}

void Window::AddCharacterAtCursor(const std::string& character) {
    if (!buffer_->IsReadAll()) {
        return;
    }

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
    buffer_->state() = BufferState::kModified;
    cursor_->b_view_col_want = -1;
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango