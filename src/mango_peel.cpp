#include "mango_peel.h"

#include "clipboard.h"
#include "cursor.h"
#include "options.h"

namespace mango {

MangoPeel::MangoPeel(Cursor* cursor, GlobalOpts* global_opts,
                     ClipBoard* clipboard)
    : buffer_(global_opts),
      opts_(global_opts),
      frame_(&buffer_, cursor, &opts_, nullptr, clipboard) {
    buffer_.Load();

    opts_.SetOpt(kOptLineNumber, static_cast<int64_t>(LineNumberType::kNone));
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

void MangoPeel::CursorGoNextWordEnd(bool one_more_character) {
    frame_.CursorGoWordEnd(one_more_character);
}

void MangoPeel::CursorGoPrevWord() { frame_.CursorGoWordBegin(); }

void MangoPeel::DeleteCharacterBeforeCursor() { frame_.DeleteAtCursor(); }

void MangoPeel::DeleteWordBeforeCursor() { frame_.DeleteWordBeforeCursor(); }

void MangoPeel::AddStringAtCursor(std::string str) {
    frame_.AddStringAtCursor(std::move(str));
}

void MangoPeel::TabAtCursor() { frame_.TabAtCursor(); }

void MangoPeel::Copy() { frame_.Copy(); }
void MangoPeel::Paste() {
    MGO_ASSERT(!frame_.selection_.active);
    bool lines;
    std::string content = frame_.clipboard_->GetContent(lines);
    if (content.empty()) {
        return;
    }

    for (char& c : content) {
        if (c == '\n') {
            c = ' ';
        }
    }
    frame_.AddStringAtCursor(std::move(content));
}

void MangoPeel::SetContent(std::string content) {
    buffer_.Clear();
    Pos pos;
    buffer_.Add({0, 0}, std::move(content), nullptr, false, pos);
}

const std::string& MangoPeel::GetContent() { return buffer_.GetLine(0); }

}  // namespace mango