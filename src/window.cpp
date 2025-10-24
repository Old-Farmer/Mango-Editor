#include "window.h"

#include "buffer.h"
#include "coding.h"
#include "cursor.h"
#include "options.h"

namespace mango {
Window::Window(Buffer* buffer, Cursor* cursor, Options* options) noexcept
    : frame_(buffer, cursor, options), cursor_(cursor), options_(options) {}

void Window::Draw() { frame_.Draw(); }

void Window::MakeCursorVisible() {
    assert(cursor_->in_window == this);
    frame_.MakeCursorVisible();
}

size_t Window::SetCursorByBViewCol(size_t b_view_col) {
    return frame_.SetCursorByBViewCol(b_view_col);
}

void Window::SetCursorHint(size_t s_row, size_t s_col) {
    frame_.SetCursorHint(s_row, s_col);
}

void Window::ScrollRows(int64_t count) {
    frame_.ScrollRows(count, cursor_->in_window == this);
}

void Window::ScrollCols(int64_t count) { frame_.ScrollCols(count); }

void Window::CursorGoRight() { frame_.CursorGoRight(); }

void Window::CursorGoLeft() { frame_.CursorGoLeft(); }

void Window::CursorGoUp() { frame_.CursorGoUp(); }

void Window::CursorGoDown() { frame_.CursorGoDown(); }

void Window::CursorGoHome() { frame_.CursorGoHome(); }

void Window::CursorGoEnd() { frame_.CursorGoEnd(); }

void Window::DeleteCharacterBeforeCursor() {
    Buffer* buffer = frame_.buffer_;
    Range range;
    if (cursor_->byte_offset == 0) {  // first byte
        if (cursor_->line == 0) {
            return;
        }
        range = {{cursor_->line - 1,
                  buffer->lines()[cursor_->line - 1].line_str.size()},
                 {cursor_->line, 0}};
    } else {
        if (options_->tabspace) {
            bool all_space = true;
            const std::string& line = buffer->lines()[cursor_->line].line_str;
            for (size_t byte_offset = 0; byte_offset < cursor_->byte_offset;
                 byte_offset++) {
                if (line[byte_offset] != kSpaceChar) {
                    all_space = false;
                    break;
                }
            }
            if (!all_space) {
                range = {{cursor_->line, cursor_->byte_offset - 1},
                         {cursor_->line, cursor_->byte_offset}};
            } else {
                range = {{cursor_->line, (cursor_->byte_offset - 1) / 4 * 4},
                         {cursor_->line, cursor_->byte_offset}};
            }
        } else {
            range = {{cursor_->line, cursor_->byte_offset - 1},
                     {cursor_->line, cursor_->byte_offset}};
        }
    }
    Pos pos;
    if (buffer->Delete(range, pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
}

void Window::AddStringAtCursor(std::string str) {
    if (options_->auto_indent && str == kNewLine &&
        frame_.buffer_->lines()[cursor_->line].line_str.size() ==
            cursor_->byte_offset) {
        // Want to create a new line ?
        // TODO: auto indent
        frame_.AddStringAtCursor(std::move(str));
    } else {
        frame_.AddStringAtCursor(std::move(str));
    }
}

void Window::TabAtCursor() { frame_.TabAtCursor(); }

void Window::Redo() { frame_.Redo(); }
void Window::Undo() { frame_.Undo(); }

void Window::NextBuffer() {
    if (frame_.buffer_->IsLastBuffer()) {
        return;
    }
    Buffer* next = frame_.buffer_->next_;
    DetachBuffer();
    frame_.buffer_ = next;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::PrevBuffer() {
    if (frame_.buffer_->IsFirstBuffer()) {
        return;
    }
    Buffer* prev = frame_.buffer_->prev_;
    DetachBuffer();
    frame_.buffer_ = prev;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::AttachBuffer(Buffer* buffer) {
    if (frame_.buffer_) {
        DetachBuffer();
    }
    frame_.buffer_ = buffer;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::DetachBuffer() {
    if (frame_.buffer_) {
        frame_.buffer_->SaveCursorState(*cursor_);
        frame_.buffer_ = nullptr;
        DestorySearchContext();
    }
}

void Window::BuildSearchContext(std::string pattern) {
    search_pattern_ = std::move(pattern);
    search_result_ = frame_.buffer_->Search(search_pattern_);
    search_buffer_version_ = frame_.buffer_->version();
}

void Window::DestorySearchContext() {
    search_pattern_.clear();
    search_result_.clear();
    search_buffer_version_ = -1;
}

Window::SearchState Window::CursorGoNextSearchResult() {
    if (search_buffer_version_ == -1) {
        return {};
    }

    if (frame_.buffer_->version() != search_buffer_version_) {
        // The buffer has changed, we do search again;
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
    }

    if (search_result_.size() == 0) {
        return {};
    }

    int64_t left = 0, right = search_result_.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        size_t line = search_result_[mid].begin.line;
        size_t byte_offset = search_result_[mid].begin.byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (static_cast<size_t>(mid) == search_result_.size() - 1) {
                mid = 0;
            } else {
                mid++;
            }
            cursor_->line = search_result_[mid].begin.line;
            cursor_->byte_offset = search_result_[mid].begin.byte_offset;
            cursor_->DontHoldColWant();
            return {static_cast<size_t>(mid + 1), search_result_.size()};
        } else if (cursor_->line < line ||
                   (cursor_->line == line &&
                    cursor_->byte_offset < byte_offset)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    if (static_cast<size_t>(left) == search_result_.size()) {
        cursor_->line = search_result_[0].begin.line;
        cursor_->byte_offset = search_result_[0].begin.byte_offset;
        cursor_->DontHoldColWant();
        return {1, search_result_.size()};
    }
    cursor_->line = search_result_[left].begin.line;
    cursor_->byte_offset = search_result_[left].begin.byte_offset;
    cursor_->DontHoldColWant();
    return {static_cast<size_t>(left + 1), search_result_.size()};
}

Window::SearchState Window::CursorGoPrevSearchResult() {
    if (search_buffer_version_ == -1) {
        return {};
    }

    if (frame_.buffer_->version() != search_buffer_version_) {
        // The buffer has changed, we do search again;
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
    }

    if (search_result_.size() == 0) {
        return {};
    }

    int64_t left = 0, right = search_result_.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        size_t line = search_result_[mid].begin.line;
        size_t byte_offset = search_result_[mid].begin.byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (mid == 0) {
                mid = search_result_.size() - 1;
            } else {
                mid--;
            }
            cursor_->line = search_result_[mid].begin.line;
            cursor_->byte_offset = search_result_[mid].begin.byte_offset;
            cursor_->DontHoldColWant();
            return {static_cast<size_t>(mid + 1), search_result_.size()};
        } else if (cursor_->line < line ||
                   (cursor_->line == line &&
                    cursor_->byte_offset < byte_offset)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    if (static_cast<size_t>(left) == 0) {
        size_t last = search_result_.size() - 1;
        cursor_->line = search_result_[last].begin.line;
        cursor_->byte_offset = search_result_[last].begin.byte_offset;
        cursor_->DontHoldColWant();
        return {last, search_result_.size()};
    }
    cursor_->line = search_result_[left - 1].begin.line;
    cursor_->byte_offset = search_result_[left - 1].begin.byte_offset;
    cursor_->DontHoldColWant();
    return {static_cast<size_t>(left), search_result_.size()};
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango