#pragma once

#include "buffer.h"
#include "frame.h"

namespace mango {

// Mango peel is an user interaction area.
// It fix at the bottome of the screan.
// It usually shows in one row, when multiple row contents needed, it will
// extend it row numbers.
class MangoPeel {
   public:
    MangoPeel(Cursor* cursor, Options* options);
    ~MangoPeel() = default;
    MANGO_DELETE_COPY(MangoPeel);
    MANGO_DEFAULT_MOVE(MangoPeel);

    void Draw();

    void MakeCursorVisible();

    // return real b_view_col
    int64_t SetCursorByBViewCol(int64_t b_view_col);

    void SetCursorHint(int s_row, int s_col);

    void ScrollRows(int64_t count);
    void ScrollCols(int64_t count);

    void CursorGoRight();
    void CursorGoLeft();
    void CursorGoUp();
    void CursorGoDown();
    void CursorGoHome();
    void CursorGoEnd();

    void DeleteCharacterBeforeCursor();
    void AddStringAtCursor(const std::string& str);
    void TabAtCursor();
    // Result DoCommand();
    // Result Hide();

   public:
    Buffer buffer_;  // Unlike window, Peel owns her nofilebacked buffer
    Frame frame_;
};

}  // namespace mango
