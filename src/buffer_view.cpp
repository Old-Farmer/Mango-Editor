#include "buffer_view.h"

#include "buffer.h"
#include "cursor.h"

namespace mango {

void BufferView::SaveCursorState(Cursor* cursor) {
    cursor_state.pos = cursor->pos;
    cursor_state.character_in_line_ = cursor->character_in_line;
    cursor_state.b_view_col_want_ = cursor->b_view_col_want;
    cursor_state_valid = true;
}

void BufferView::RestoreCursorState(Cursor* cursor, Buffer* buffer) {
    MGO_ASSERT(buffer);
    MGO_ASSERT(cursor_state_valid);
    if (buffer->state() == BufferState::kHaveNotRead) {
        cursor->pos = {0, 0};
        cursor->b_view_col_want.reset();
        cursor_state_valid = false;
        return;
    }

    // We check and set the cursor to a valid pos
    cursor->pos.line = std::min(cursor_state.pos.line, buffer->LineCnt() - 1);
    cursor->pos.byte_offset = std::min(
        cursor_state.pos.byte_offset, buffer->GetLine(cursor->pos.line).size());
    cursor->b_view_col_want = cursor_state.b_view_col_want_;
    cursor_state_valid = false;
}

}  // namespace mango
