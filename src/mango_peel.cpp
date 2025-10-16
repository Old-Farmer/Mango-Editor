#include "mango_peel.h"

#include "cursor.h"
#include "options.h"

namespace mango {

MangoPeel::MangoPeel(Cursor* cursor, Options* options)
    : frame_(&buffer_, cursor, options) {
        buffer_.ReadAll();
    }

void MangoPeel::Draw() { frame_.Draw(); }

void MangoPeel::MakeCursorVisible() {
    assert(frame_.cursor_->in_window == nullptr);
    frame_.MakeCursorVisible();
}

int64_t MangoPeel::SetCursorByBViewCol(int64_t b_view_col) {
    return frame_.SetCursorByBViewCol(b_view_col);
}

void MangoPeel::SetCursorHint(int s_row, int s_col) {
    frame_.SetCursorHint(s_row, s_col);
}

void MangoPeel::ScrollRows(int64_t count) { frame_.ScrollRows(count, true); }

void MangoPeel::ScrollCols(int64_t count) { frame_.ScrollCols(count); }

void MangoPeel::CursorGoRight() { frame_.CursorGoRight(); }

void MangoPeel::CursorGoLeft() { frame_.CursorGoLeft(); }

void MangoPeel::CursorGoUp() { frame_.CursorGoUp(); }

void MangoPeel::CursorGoDown() { frame_.CursorGoDown(); }

void MangoPeel::CursorGoHome() { frame_.CursorGoHome(); }

void MangoPeel::CursorGoEnd() { frame_.CursorGoEnd(); }

void MangoPeel::DeleteCharacterBeforeCursor() {
    frame_.DeleteCharacterBeforeCursor();
}

void MangoPeel::AddStringAtCursor(const std::string& str) {
    frame_.AddStringAtCursor(str);
}

void MangoPeel::TabAtCursor() { frame_.TabAtCursor(); }

}  // namespace mango