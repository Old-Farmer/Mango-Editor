#pragma once
#include <cstdint>

#include "buffer.h"
#include "buffer_view.h"
#include "options.h"
#include "search.h"
#include "selection.h"
#include "term.h"
#include "utils.h"

namespace mango {

class Buffer;
struct Cursor;
class SyntaxParser;
struct Pos;
class ClipBoard;

// TextArea is a class that offers some basic ui interface
// A Frame must associated with a buffer from rendering
class TextArea {
   public:
    TextArea() = default;
    TextArea(Cursor* cursor, Opts* opts, SyntaxParser* parser,
             ClipBoard* clipboard) noexcept;
    ~TextArea() = default;
    MGO_DELETE_COPY(TextArea);
    MGO_DEFAULT_MOVE(TextArea);

    // if search_context != nullptr, frame will draw the search highlight no
    // matter what kHighlighOnSearch is. So caller should be careful.
    void Draw(BufferSearchContext* search_context);

    bool In(size_t s_col, size_t s_row);

    // Adjust view to a valid view.
    // Now we use this to make sure that the current buffer view is valid.
    // This garrentee will make lots of method implementations easier, like
    // Scroll, MakeCursorVisible.
    // TODO: maybe we don't neet make sure valid every time In some
    // circumstance?
    void MakeSureViewValid();

    void MakeSureBColViewWantReady(CursorState& state);

    void MakeCursorVisible();

    void SetCursorHint(size_t s_row, size_t s_col);

    // NOTE: all count shouldn't be 0, otherwise behavior is undefined.

    // count > 0 for down
    // < 0 for up
    void ScrollRows(int64_t count);
    void ScrollCols(int64_t count);

    bool CursorGoRightState(size_t count, CursorState& state);
    bool CursorGoLeftState(size_t count, CursorState& state);
    bool CursorGoUpState(size_t count, CursorState& state);
    bool CursorGoDownState(size_t count, CursorState& state);
    bool CursorGoHomeState(CursorState& state);
    bool CursorGoFirstNonBlankState(CursorState& state);
    bool CursorGoEndState(CursorState& state);
    bool CursorGoNextWordEndState(size_t count, CursorState& state);
    bool CursorGoPrevWordBeginState(size_t count, CursorState& state);
    bool CursorGoNextWordBeginState(size_t count, CursorState& state);
    bool CursorGoLineState(size_t line, CursorState& state);

    void CursorGoRight(size_t count);
    void CursorGoLeft(size_t count);
    void CursorGoUp(size_t count);
    void CursorGoDown(size_t count);
    void CursorGoHome();
    void CursorGoFirstNonBlank();
    void CursorGoEnd();
    void CursorGoNextWordEnd(size_t count);
    void CursorGoPrevWordBegin(size_t count);
    void CursorGoNextWordBegin(size_t count);
    void CursorGoLine(size_t line);

    void StartSelection(Pos anchor);
    void StartLineSelection(Pos anchor);
    void StopSelection() { selection_.reset(); }
    bool IsSelectionActive() { return selection_.get() != nullptr; }
    void SelectionFollowCursor();

    // Shouldn't be called in vim mode
    void SelectAll();

    Result DeleteAtCursor();
    Result DeleteWordBeforeCursor();
    // if cursor_pos != nullptr then cursor will set to cursor_pos
    Result AddStringAtCursor(std::string_view str,
                             const Pos* cursor_pos = nullptr);
    // if cursor_pos != nullptr then cursor will set to cursor_pos
    // Selection shouldn't be active.
    Result AddStringAtPos(Pos pos, std::string_view str,
                          const Pos* cursor_pos = nullptr);
    // kOk for truely done
    Result Replace(const Range& range, std::string_view str,
                   const Pos* cursor_pos = nullptr);
    Result TabAtCursor();
    Result Redo();
    Result Undo();

    void Copy();
    Result Paste(size_t count);
    void Cut();

    void CopyRange(Range range, bool lines);
    void CutRange(Range range, bool lines);

    Result DeleteCharacterBeforeCursor();
    Result DeleteSelection();

    Result AddStringAtCursorNoSelection(std::string_view str,
                                        const Pos* cursor_pos = nullptr);
    Result ReplaceSelection(std::string_view str,
                            const Pos* cursor_pos = nullptr);

    // Search relevant
    BufferSearchState CursorGoSearchResultState(BufferSearchContext& context,
                                                bool next, size_t count,
                                                bool keep_current_if_one,
                                                CursorState& state);
    // Just move buffer view without touch cursor
    bool BufferViewGoSearchResult(BufferSearchContext& context, bool next,
                                  size_t count, bool keep_current_if_one,
                                  CursorState& state);

    void AfterModify(const Pos& cursor_pos);

   private:
    size_t SidebarWidth();
    void DrawSidebar(int s_row, size_t absolute_line, size_t sidebar_width);
    Range CalcWrapRange(size_t content_width);

    // return byte_offset
    size_t CalcByteOffsetByBViewCol(std::string_view line,
                                    size_t b_view_col_from_byte_offset,
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

    bool CursorGoUpStateWrap(size_t count, size_t content_width,
                             CursorState& state);
    bool CursorGoUpStateNoWrap(size_t count, size_t content_width,
                               CursorState& state);
    bool CursorGoDownStateWrap(size_t count, size_t content_width,
                               CursorState& state);
    bool CursorGoDownStateNoWrap(size_t count, size_t content_width,
                                 CursorState& state);

    void ScrollRowsWrap(int64_t count, size_t content_width);
    void ScrollRowsNoWrap(int64_t count, size_t content_width);

    bool SizeValid(size_t sidebar_width);

    void UpdateSyntax();

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

    BufferView* b_view_;

    std::unique_ptr<Selection> selection_;
    ClipBoard* clipboard_;

   private:
    SyntaxParser* parser_;
    Opts* opts_;
    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
