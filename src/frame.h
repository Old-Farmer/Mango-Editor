#pragma once
#include <cstdint>

#include "buffer.h"
#include "buffer_view.h"
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
    Frame(Cursor* cursor, Opts* opts, SyntaxParser* parser,
          ClipBoard* clipboard) noexcept;
    ~Frame() = default;
    MGO_DELETE_COPY(Frame);
    MGO_DEFAULT_MOVE(Frame);

    void Draw();

    bool In(size_t s_col, size_t s_row);

    // Adjust view to a valid view.
    // Now we use this to make sure that the current buffer view is valid.
    // This garrentee will make lots of method implementations easier, like
    // Scroll, MakeCursorVisible.
    // TODO: maybe we don't neet make sure valid every time In some
    // circumstance?
    void MakeSureViewValid();

    void MakeCursorVisible();

    void SetCursorHint(size_t s_row, size_t s_col);

    // count > 0 for down
    // < 0 for up
    void ScrollRows(int64_t count);
    void ScrollCols(int64_t count);

    void CursorGoRight();
    void CursorGoLeft();
    void CursorGoUp(size_t count);
    void CursorGoDown(size_t count);
    void CursorGoHome();
    void CursorGoEnd();
    void CursorGoWordEnd(bool one_more_character);
    void CursorGoWordBegin();
    void CursorGoLine(size_t line);

    void SelectAll();

    Result DeleteAtCursor();
    Result DeleteWordBeforeCursor();
    // if cursor_pos != nullptr then cursor will set to cursor_pos
    Result AddStringAtCursor(std::string str, const Pos* cursor_pos = nullptr);
    // kOk for truely done
    Result Replace(const Range& range, std::string str,
                   const Pos* cursor_pos = nullptr);
    Result TabAtCursor();
    Result Redo();
    Result Undo();

    void Copy();
    Result Paste();
    void Cut();

    Result DeleteCharacterBeforeCursor();
    Result DeleteSelection();

    Result AddStringAtCursorNoSelection(std::string str,
                                        const Pos* cursor_pos = nullptr);
    Result ReplaceSelection(std::string str, const Pos* cursor_pos = nullptr);

   private:
    size_t SidebarWidth();
    void DrawSidebar(int s_row, size_t absolute_line, size_t sidebar_width);
    Range CalcWrapRange(size_t content_width);

    // return real b_view_col
    size_t SetCursorByBViewCol(size_t b_view_col_from_byte_offset,
                               size_t byte_offset, size_t content_width,
                               bool wrap);

    void SetCursorHintNoWrap(size_t s_row, size_t s_col, size_t sidebar_width);
    void SetCursorHintWrap(size_t s_row, size_t s_col, size_t sidebar_width);

    void MakeCursorVisibleNotWrap();
    void MakeCursorVisibleWrap();
    void MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(
        size_t content_width);
    void MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(
        size_t content_width);

    void CursorGoUpWrap(size_t count, size_t content_width);
    void CursorGoUpNoWrap(size_t count, size_t content_width);
    void CursorGoDownWrap(size_t count, size_t content_width);
    void CursorGoDownNoWrap(size_t count, size_t content_width);

    void ScrollRowsWrap(int64_t count, size_t content_width);
    void ScrollRowsNoWrap(int64_t count, size_t content_width);

    bool SizeValid(size_t sidebar_width);

    void UpdateSyntax();
    void SelectionFollowCursor();
    void SelectionCancell() { selection_.active = false; }
    void AfterModify(const Pos& cursor_pos);

    template <typename T>
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

    // // Wrap or no wrap: which line of buffer on the first screen row
    // size_t b_view_line_ = 0;
    // // No wrap: which col of buffer view on the first screen col
    // size_t b_view_col_ = 0;
    // // Wrap: When in wrap, a buffer line may be rendered in multi rows,
    // dividing
    // // a line into multi sublines. This member represents which sub line of
    // // buffer on the first screen row.
    // size_t b_view_subline_ = 0;

    BufferView* b_view_;

    Selection selection_;
    ClipBoard* clipboard_;

    bool make_cursor_visible_ =
        true;  // TODO: don't call xxx = true in every call.

   private:
    SyntaxParser* parser_;
    Opts* opts_;
    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
