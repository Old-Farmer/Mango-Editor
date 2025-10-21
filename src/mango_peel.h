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
    int64_t SetCursorByBViewCol(size_t b_view_col);

    void SetCursorHint(size_t s_row, size_t s_col);

    void ScrollRows(int64_t count);
    void ScrollCols(int64_t count);

    void CursorGoRight();
    void CursorGoLeft();
    void CursorGoUp();
    void CursorGoDown();
    void CursorGoHome();
    void CursorGoEnd();

    void DeleteCharacterBeforeCursor();
    void AddStringAtCursor(std::string str);
    void TabAtCursor();

    void SetContent(std::string content);
    const std::string& GetContent();
    // Result DoCommand();
    // Result Hide();

   public:
    Frame frame_;

   private:
    Buffer buffer_;  // Unlike window, Peel owns her nofilebacked buffer
};

}  // namespace mango
