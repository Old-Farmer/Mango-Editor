#pragma once

#include "buffer.h"
#include "cursor.h"
#include "frame.h"
#include "options.h"
#include "term.h"

namespace mango {

class Window {
   public:
    Window(Buffer* buffer, Cursor* cursor, Options* options) noexcept;
    ~Window() = default;
    MANGO_DELETE_COPY(Window);
    MANGO_DEFAULT_MOVE(Window);

    int id() { return id_; }

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

    // window list op
    static Window CreateListHead() noexcept;
    void AppendToList(Window*& tail) noexcept;
    void RemoveFromList() noexcept;

   private:
    static int64_t AllocId() noexcept;
    Window() {} // only for list head

   public:
    Frame frame_;

    Window* next_ = nullptr;
    Window* prev_ = nullptr;

   private:
    int64_t id_ = AllocId();
    static int64_t cur_window_id_;
};

}  // namespace mango
