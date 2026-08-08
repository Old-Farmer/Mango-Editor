#pragma once
#include <cstdint>

#include "buffer.h"
#include "buffer_view.h"
#include "options.h"
#include "search.h"
#include "selection.h"
#include "syntax.h"
#include "term.h"
#include "utils.h"

namespace charxed {

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
    CHX_DELETE_COPY(TextArea);
    CHX_DEFAULT_MOVE(TextArea);

    // if search_context != nullptr, frame will draw the search highlight no
    // matter what kHighlighOnSearch is. So caller should be careful.
    void Draw(BufferSearchReplaceContext* search_context);

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
    bool FindNextCharacterAndCursorGoInCurrentLineState(const Character& c,
                                                        CursorState& state);
    bool FindPrevCharacterAndCursorGoInCurrentLineState(const Character& c,
                                                        CursorState& state);
    bool CursorGoBracketState(CursorState& state);

    using CursorGoState = bool (TextArea::*)(CursorState&);
    using CursorGoStateWithCount = bool (TextArea::*)(size_t, CursorState&);

    // Cursor movement wrappers of CursorGoxxxState, no count and count version.
    // f is CursorGoxxxState, character wise.
    template <CursorGoState f>
    void CursorGo() {
        b_view_->make_cursor_visible = true;
        CursorState state(cursor_);
        if ((this->*f)(state)) {
            state.SetCursor(cursor_);
            SelectionFollowCursor();
        }
    }
    template <CursorGoStateWithCount f>
    void CursorGoWithCount(size_t count) {
        b_view_->make_cursor_visible = true;
        CursorState state(cursor_);
        if ((this->*f)(count, state)) {
            state.SetCursor(cursor_);
            SelectionFollowCursor();
        }
    }

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
    void CursorGoBracket();
    void FindNextCharacterAndCursorGoInCurrentLine(const Character& c);
    void FindPrevCharacterAndCursorGoInCurrentLine(const Character& c);

    void StartSelection(Pos anchor);
    void StartLineSelection(Pos anchor);
    void StopSelection() { selection_.reset(); }
    bool IsSelectionActive() { return selection_.get() != nullptr; }
    void SelectionFollowCursor();

    // Selection wrappers of CursorGoxxxState, no count and count version.
    // f is CursorGoxxxState, character wise.
    template <CursorGoState f>
    void SelectTo(bool inclusive_end = false) {
        b_view_->make_cursor_visible = true;
        CursorState state(cursor_);
        if (!(this->*f)(state)) {
            return;
        }
        selection_ = std::make_unique<NormalSelection>(cursor_->pos, state.pos,
                                                       inclusive_end);
    }
    template <CursorGoStateWithCount f>
    void SelectToWithCount(size_t count) {
        b_view_->make_cursor_visible = true;
        CursorState state(cursor_);
        if (!(this->*f)(count, state)) {
            return;
        }
        selection_ = std::make_unique<NormalSelection>(cursor_->pos, state.pos);
    }

    void SelectAll();
    // TODO:
    // bool SelectWords(size_t count);
    // bool SelectInnerWords(size_t count);
    bool SelectWord();
    void SelectLinesToLine(size_t line);
    // Include the current line
    void SelectNextLines(size_t count);
    void SelectPrevLines(size_t count);

    // inner means we don't need pair open & close character, just need the
    // content inside the pair.
    // return true if we can find a pair,
    // else return false.
    bool SelectPair(char open, bool inner);

    Result DeleteAtCursor();
    // Must !selection_->IsSelectionActive()
    Result DeleteWordBeforeCursor();
    // if cursor_pos != nullptr then cursor will set to cursor_pos
    Result AddStringAtCursorNoSelection(std::string_view str,
                                        const Pos* cursor_pos = nullptr);
    // str is new content, lines means whether is line semantic
    Result ReplaceSelection(std::string_view str, bool lines);

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

    // Operations on selection will stop cancel selection after finishing,
    // include Copy, Paste, Cut, Indent, etc.

    void Copy();
    Result Paste(size_t count, bool after_cursor);
    void Cut();

    Result IndentSelection(size_t count);
    Result UnindentSelection(size_t count);

    void CopyRange(Range range, bool lines);
    void CutRange(Range range, bool lines);

    Result DeleteCharacterBeforeCursor();
    Result DeleteCharacterFromCursor(size_t count);
    Result DeleteSelection();

    // inclusice end_line
    Result IndentLines(size_t count, size_t begin_line, size_t end_line);
    Result UnindentLines(size_t count, size_t begin_line, size_t end_line);

    // Search relevant
    SearchState CursorGoSearchResultState(BufferSearchReplaceContext& context,
                                          bool next, size_t count,
                                          bool keep_current_if_one,
                                          CursorState& state);
    // Just move buffer view without touch cursor
    bool BufferViewGoSearchResult(BufferSearchReplaceContext& context,
                                  bool next, size_t count,
                                  bool keep_current_if_one);

    void AfterModify(const Pos& cursor_pos);

   private:
    std::vector<Highlight> PrepareSearchHighlight(
        BufferSearchReplaceContext* search_context);
    std::vector<Highlight> PrepareSelectionHighlight();
    std::tuple<std::vector<Highlight>, std::vector<int64_t>>
    PrepareTrailingBlankHighlight(const Range& render_range);

    struct DrawContext {
        size_t sidebar_width;
        size_t content_s_col;
        size_t content_width;
        bool need_hl_cursor_line;
        size_t cursor_line;
        std::vector<const std::vector<Highlight>*> highlights;
        Range render_range;
        std::vector<Highlight> selection_hl;
        std::vector<int64_t> trailing_white_begin_pre_line;
    };

    void DrawWarp(const DrawContext& context);
    void DrawNoWarp(const DrawContext& context);

    size_t SidebarWidth();
    void DrawSidebar(int s_row, size_t absolute_line, size_t sidebar_width);
    Range CalcWrapRange(size_t content_width);

    // return byte_offset
    size_t CalcByteOffsetByBViewCol(std::string_view line,
                                    size_t b_view_col_from_byte_offset,
                                    size_t byte_offset, size_t content_width);
    // return byte_offset
    TextTree::Iterator CalcByteOffsetByBViewCol(
        const TextTree::TextView& line, size_t b_view_col_from_byte_offset,
        TextTree::Iterator iter, size_t content_width);

    void SetCursorHintNoWrap(size_t s_row, size_t s_col, size_t sidebar_width);
    void SetCursorHintWrap(size_t s_row, size_t s_col, size_t sidebar_width);

    void MakeCursorVisibleNotWrap(size_t top_scroll_off,
                                  size_t bottom_scroll_off);
    void MakeCursorVisibleWrap(size_t top_scroll_off, size_t bottom_scroll_off);
    void MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(
        size_t top_scroll_off, size_t content_width);
    // Make Cursor Visible and Calculate how many screen lines b view should
    // move when cursor is in the render range to satisfy the scroll off
    // setting. b_view->make_cursor_visible must be true.
    // return scroll line, < 0 means b_view go up, > 0 means b_view go
    // down.
    int64_t MakeCursorVisibleWrapWhenCursorInRenderRange(
        size_t row, size_t top_scroll_off, size_t bottom_scroll_off,
        size_t content_width);
    void MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(
        size_t bottom_scroll_off, size_t content_width);

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

}  // namespace charxed
