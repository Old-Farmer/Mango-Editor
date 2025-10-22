#include "window.h"

#include "buffer.h"
#include "cursor.h"

namespace mango {
Window::Window(Buffer* buffer, Cursor* cursor, Options* options) noexcept
    : frame_(buffer, cursor, options), cursor_(cursor) {}

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
    frame_.DeleteCharacterBeforeCursor();
}

void Window::AddStringAtCursor(std::string str) {
    frame_.AddStringAtCursor(std::move(str));
}

void Window::TabAtCursor() { frame_.TabAtCursor(); }

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
        size_t line = search_result_[mid].line;
        size_t byte_offset = search_result_[mid].byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (static_cast<size_t>(mid) == search_result_.size() - 1) {
                mid = 0;
            } else {
                mid++;
            }
            cursor_->line = search_result_[mid].line;
            cursor_->byte_offset = search_result_[mid].byte_offset;
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
        cursor_->line = search_result_[0].line;
        cursor_->byte_offset = search_result_[0].byte_offset;
        cursor_->DontHoldColWant();
        return {1, search_result_.size()};
    }
    cursor_->line = search_result_[left].line;
    cursor_->byte_offset = search_result_[left].byte_offset;
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
        size_t line = search_result_[mid].line;
        size_t byte_offset = search_result_[mid].byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (mid == 0) {
                mid = search_result_.size() - 1;
            } else {
                mid--;
            }
            cursor_->line = search_result_[mid].line;
            cursor_->byte_offset = search_result_[mid].byte_offset;
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
        cursor_->line = search_result_[last].line;
        cursor_->byte_offset = search_result_[last].byte_offset;
        cursor_->DontHoldColWant();
        return {last, search_result_.size()};
    }
    cursor_->line = search_result_[left - 1].line;
    cursor_->byte_offset = search_result_[left - 1].byte_offset;
    cursor_->DontHoldColWant();
    return {static_cast<size_t>(left), search_result_.size()};
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango