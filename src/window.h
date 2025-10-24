#pragma once

#include "buffer.h"
#include "frame.h"

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
    size_t SetCursorByBViewCol(size_t b_view_col);

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
    void Redo();
    void Undo();

    void NextBuffer();
    void PrevBuffer();

    // attach will first detach
    void AttachBuffer(Buffer* buffer);
    // dangerous op, must attach buffer before preprocess & draw
    void DetachBuffer();

    // Search relevant
    struct SearchState {
        size_t i = 0; // from 1 instead of zero
        size_t total = 0;
    };
    //
    void BuildSearchContext(std::string pattern);
    void DestorySearchContext();
    const std::string& GetSearchPattern() {return search_pattern_;}
    SearchState CursorGoNextSearchResult();
    SearchState CursorGoPrevSearchResult();

   private:
    static int64_t AllocId() noexcept;
    Window() {}  // only for list head

   public:
    Frame frame_;

   private:
    Cursor* cursor_;
    Options* options_;

    std::vector<Range> search_result_;
    std::string search_pattern_;
    int64_t search_buffer_version_ = -1;

    int64_t id_ = AllocId();
    static int64_t cur_window_id_;
};

}  // namespace mango
