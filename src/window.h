#pragma once

#include "buffer.h"
#include "cursor.h"
#include "term.h"
namespace mango {

class Window {
   public:
    Window();
    Window(Buffer* buffer);
    ~Window() = default;
    MANGO_DEFAULT_COPY(Window);
    MANGO_DEFAULT_MOVE(Window);

    int id() { return id_; }

    void Draw();

    void MakeCursorVisible();

    void SetCursorByBViewCol(int64_t b_view_col);

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
    void AddCharacterAtCursor(const std::string& character);

   private:
    static int64_t AllocId() noexcept;

   public:
    int width_ = 0;
    int height_ = 0;
    int row_ = 0;  // window top left corner x related to the whole screen
    int col_ = 0;  // window top left corner y related to the whole screen
    // (0, 0) ------------------------------------ row
    // |
    // |
    // |
    // col
    Buffer* buffer_ = nullptr;  // associated buffer
    Cursor* cursor_ = nullptr;
    // If we put a buffer in an infinite window, (b_view_x_, b_view_y_) means
    // the top left corner to show the buffer if the buffer top left coner is
    // (0, 0).
    int64_t b_view_row_ = 0;
    int64_t b_view_col_ = 0;
    bool wrap_ = false;


   private:
    Terminal* term_ = &Terminal::GetInstance();
    int64_t id_;

    static int64_t cur_window_id_;
};

}  // namespace mango
