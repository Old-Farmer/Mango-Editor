#pragma once
#include <cstdint>

#include "term.h"
#include "utils.h"

namespace mango {

class Buffer;
class Cursor;
class Options;

// Frame is a class that offers some basic ui interface
// A Frame must associated with a buffer from rendering
class Frame {
   public:
    Frame() {}
    Frame(Buffer* buffer, Cursor* cursor, Options* options) noexcept;
    ~Frame() = default;
    MANGO_DELETE_COPY(Frame);
    MANGO_DEFAULT_MOVE(Frame);

    void Draw();

    bool In(size_t s_col, size_t s_row);

    void MakeCursorVisible();

    // return real b_view_col
    size_t SetCursorByBViewCol(size_t b_view_col);

    void SetCursorHint(size_t s_row, size_t s_col);

    // count > 0 for down
    // < 0 for up
    void ScrollRows(int64_t count, bool cursor_in_frame);
    void ScrollCols(int64_t count);

    void CursorGoRight();
    void CursorGoLeft();
    void CursorGoUp();
    void CursorGoDown();
    void CursorGoHome();
    void CursorGoEnd();

    void DeleteCharacterBeforeCursor();
    // Only single "\n" or a no "\n" string is permitted
    void AddStringAtCursor(std::string str);
    void TabAtCursor();

   public:
    size_t width_ = 0;
    size_t height_ = 0;
    size_t row_ = 0;  // window top left corner x related to the whole screen
    size_t col_ = 0;  // window top left corner y related to the whole screen
    // (0, 0) ------------------------------------ row
    // |
    // |
    // |
    // col
    Buffer* buffer_ = nullptr;  // associated buffer
    Cursor* cursor_ = nullptr;
    // when no wrap, If we put a buffer in an infinite window, (b_view_line_,
    // b_view_row_) means the top left corner to show the buffer if the buffer
    // top left coner is (0, 0).
    // When wrap, b_view_col_ is not used
    size_t b_view_line_ = 0;
    size_t b_view_col_ = 0;
    bool wrap_ = false;

   private:
    Options* options_;
    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
