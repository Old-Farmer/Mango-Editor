#include "window.h"

namespace mango {
Window::Window(Buffer* buffer, Cursor* cursor, Options* options) noexcept
    : frame_(buffer, cursor, options) {}

void Window::Draw() { frame_.Draw(); }

void Window::MakeCursorVisible() {
    assert(frame_.cursor_->in_window == this);
    frame_.MakeCursorVisible();
}

int64_t Window::SetCursorByBViewCol(int64_t b_view_col) {
    return frame_.SetCursorByBViewCol(b_view_col);
}

void Window::SetCursorHint(int s_row, int s_col) {
    frame_.SetCursorHint(s_row, s_col);
}

void Window::ScrollRows(int64_t count) {
    frame_.ScrollRows(count, frame_.cursor_->in_window == this);
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

void Window::AddCharacterAtCursor(const std::string& character) {
    frame_.AddCharacterAtCursor(character);
}

Window Window::CreateListHead() noexcept { return Window(); }

void Window::AppendToList(Window*& tail) noexcept {
    assert(tail->next_ == nullptr);

    tail->next_ = this;
    prev_ = tail;
    next_ = nullptr;

    tail = this;
}

void Window::RemoveFromList() noexcept {
    if (next_ == nullptr) {
        prev_->next_ = next_;
        prev_ = nullptr;
        return;
    }
    // Must have a dummy head
    next_->prev_ = prev_;
    next_ = nullptr;
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango