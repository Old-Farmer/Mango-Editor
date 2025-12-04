#include "buffer_view.h"

#include "buffer.h"
#include "cursor.h"

namespace mango {

void BufferView::SaveCursorState(Cursor* cursor) {
    cursor_state->line = cursor->line;
    cursor_state->byte_offset = cursor->byte_offset;
    cursor_state->character_in_line_ = cursor->character_in_line;
}

void BufferView::RestoreCursorState(Cursor* cursor, Buffer* buffer) {
    MGO_ASSERT(buffer);
    if (buffer->state() == BufferState::kHaveNotRead) {
        cursor->line = 0;
        cursor->byte_offset = 0;
        cursor->b_view_col_want.reset();
        cursor_state.reset();
        return;
    }

    // We check and set the cursor to a valid pos
    cursor->line = std::min(cursor_state->line, buffer->LineCnt() - 1);
    cursor->byte_offset = std::min(cursor_state->byte_offset,
                                   buffer->GetLine(cursor->line).size());
    cursor->b_view_col_want.reset();
    cursor_state.reset();
}

}  // namespace mango