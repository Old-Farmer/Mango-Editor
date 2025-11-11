#include "mango_peel.h"

#include "cursor.h"
#include "options.h"

namespace mango {

MangoPeel::MangoPeel(Cursor* cursor, Options* options)
    : frame_(&buffer_, cursor, options, nullptr), buffer_(options) {
    buffer_.Load();
}

void MangoPeel::Draw() { frame_.Draw(); }

void MangoPeel::MakeCursorVisible() {
    MGO_ASSERT(frame_.cursor_->in_window == nullptr);
    frame_.MakeCursorVisible();
}

int64_t MangoPeel::SetCursorByBViewCol(size_t b_view_col) {
    return frame_.SetCursorByBViewCol(b_view_col);
}

void MangoPeel::SetCursorHint(size_t s_row, size_t s_col) {
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

void MangoPeel::CursorGoNextWord() { frame_.CursorGoNextWord(); }

void MangoPeel::CursorGoPrevWord() { frame_.CursorGoPrevWord(); }

void MangoPeel::DeleteCharacterBeforeCursor() { frame_.DeleteAtCursor(); }

void MangoPeel::DeleteWordBeforeCursor() { frame_.DeleteWordBeforeCursor(); }

void MangoPeel::AddStringAtCursor(std::string str) {
    frame_.AddStringAtCursor(std::move(str));
}

void MangoPeel::TabAtCursor() { frame_.TabAtCursor(); }

void MangoPeel::SetContent(std::string content) {
    buffer_.Clear();
    Pos pos;
    buffer_.Add({0, 0}, std::move(content), false, pos);
}

const std::string& MangoPeel::GetContent() { return buffer_.GetLine(0); }

}  // namespace mango