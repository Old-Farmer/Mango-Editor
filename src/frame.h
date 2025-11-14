#pragma once
#include <cstdint>

#include "buffer.h"
#include "options.h"
#include "selection.h"
#include "term.h"
#include "utils.h"

namespace mango {

class Buffer;
struct Cursor;
class SyntaxParser;
struct Pos;
class ClipBoard;

// Frame is a class that offers some basic ui interface
// A Frame must associated with a buffer from rendering
class Frame {
   public:
    Frame() = default;
    Frame(Buffer* buffer, Cursor* cursor, Opts* opts,
          SyntaxParser* parser, ClipBoard* clipboard) noexcept;
    ~Frame() = default;
    MGO_DELETE_COPY(Frame);
    MGO_DEFAULT_MOVE(Frame);

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
    void CursorGoNextWordEnd(bool one_more_character);
    void CursorGoPrevWord();

    void SelectAll();

    void DeleteAtCursor();
    void DeleteWordBeforeCursor();
    // if cursor_pos != nullptr then cursor will set to cursor_pos
    void AddStringAtCursor(std::string str, const Pos* cursor_pos = nullptr);
    void TabAtCursor();
    void Redo();
    void Undo();

    void Copy();
    void Paste();
    void Cut();

    void DeleteCharacterBeforeCursor();
    void DeleteSelection();

    void AddStringAtCursorNoSelection(std::string str,
                                      const Pos* cursor_pos = nullptr);
    void ReplaceSelection(std::string str, const Pos* cursor_pos = nullptr);

   private:
    size_t CalcLineNumberWidth();
    void DrawLineNumber();

    void UpdateSyntax();
    void SelectionFollowCursor();
    void SelectionCancell() { selection_.active = false; }
    void AfterModify(const Pos& cursor_pos);

    template<typename T>
    T GetOpt(OptKey key) {
        if (opts_->GetScope(key) == OptScope::kGlobal) {
            return opts_->global_opts_->GetOpt<T>(key);
        }
        if (opts_->GetScope(key) == OptScope::kBuffer) {
            return buffer_->opts().GetOpt<T>(key);
        }
        return opts_->GetOpt<T>(key);
    }

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
    LineNumberType line_number_;

    Selection selection_;
    ClipBoard* clipboard_;

   private:
    SyntaxParser* parser_;
    Opts* opts_;
    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
