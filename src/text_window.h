#pragma once

#include "buffer.h"
#include "search.h"
#include "text_area.h"
#include "window.h"

namespace charxed {

class TextWindow : public Window {
   public:
    TextWindow(Cursor* cursor, GlobalOpts* global_opts, SyntaxParser* parser,
               ClipBoard* clipboard, BufferManager* buffer_manager) noexcept;
    virtual ~TextWindow() = default;
    CHX_DELETE_COPY(TextWindow);
    CHX_DEFAULT_MOVE(TextWindow);

    int id() { return id_; }

    void Draw(bool highlight_search) {
        area_.Draw(highlight_search && GetOpt<bool>(kOptHighlightOnSearch)
                       ? &b_search_context_
                       : nullptr);
    }

    void MakeCursorVisible() { area_.MakeCursorVisible(); }

    void SetCursorHint(size_t s_row, size_t s_col) override {
        area_.SetCursorHint(s_row, s_col);
    }

    Result DoubleClick() override {
        if (area_.SelectWord()) {
            return kSelectionStarted;
        }
        return kOk;
    }

    // NOTE: all count shouldn't be 0, otherwise behavior is undefined.

    void ScrollRows(int64_t count) override { area_.ScrollRows(count); }
    void ScrollCols(int64_t count) override { area_.ScrollCols(count); }

    void CursorGoRight(size_t count) { area_.CursorGoRight(count); }
    void CursorGoLeft(size_t count) { area_.CursorGoLeft(count); }
    void CursorGoUp(size_t count) override { area_.CursorGoUp(count); }
    void CursorGoDown(size_t count) override { area_.CursorGoDown(count); }
    void CursorGoHalfPageUp(size_t count) override {
        area_.CursorGoUp(count * area_.height_ / 2);
    }
    void CursorGoHalfPageDown(size_t count) override {
        area_.CursorGoDown(count * area_.height_ / 2);
    }
    void CursorGoPageUp(size_t count) override {
        area_.CursorGoUp(count * area_.height_);
    }
    void CursorGoPageDown(size_t count) override {
        area_.CursorGoDown(count * area_.height_);
    }
    void CursorGoHome() { area_.CursorGoHome(); }
    void CursorGoFirstNonBlank() { area_.CursorGoFirstNonBlank(); }
    void CursorGoEnd() { area_.CursorGoEnd(); }
    void CursorGoNextWordEnd(size_t count) { area_.CursorGoNextWordEnd(count); }
    void CursorGoWordBegin(size_t count) { area_.CursorGoPrevWordBegin(count); }
    void CursorGoNextWordBegin(size_t count) {
        area_.CursorGoNextWordBegin(count);
    }
    void CursorGoBracket() { area_.CursorGoBracket(); }
    void CursorGoLine(size_t line);
    void FindNextCharacterAndCursorGoInCurrentLine(const Character& c) {
        area_.FindNextCharacterAndCursorGoInCurrentLine(c);
    }
    void FindPrevCharacterAndCursorGoInCurrentLine(const Character& c) {
        area_.FindPrevCharacterAndCursorGoInCurrentLine(c);
    }

    void SelectAll() { area_.SelectAll(); }

    bool IsSelectionActive() override { return area_.IsSelectionActive(); };
    bool StartSelection(Pos anchor) override {
        area_.StartSelection(anchor);
        return true;
    };
    void StopSelection() override { area_.StopSelection(); };
    void SelectionFollowCursor() override { area_.SelectionFollowCursor(); };
    Range SelectionRange() override {
        return area_.selection_ ? area_.selection_->ToSelectRange(area_.buffer_)
                                : Range{{0, 0}, {0, 0}};
    }

    Result DeleteAtCursor();
    Result DeleteCharacterFromCursor(size_t count) {
        return area_.DeleteCharacterFromCursor(count);
    }
    Result DeleteSelection() { return area_.DeleteSelection(); }
    Result DeleteWordBeforeCursor() { return area_.DeleteWordBeforeCursor(); }
    // raw means do not treat it as keystroke
    Result AddStringAtCursor(std::string_view str, bool raw = false);
    // Create a new line after or under the cursor line.
    // And make the cursor at the first non-blank character.
    // Selection should be off.
    Result NewLineAboveCursorline();
    Result NewLineUnderCursorline();
    // See Frame::Replace
    Result Replace(const Range& range, std::string_view str);
    Result TabAtCursor() { return area_.TabAtCursor(); }
    Result Redo() { return area_.Redo(); }
    Result Undo() { return area_.Undo(); }

    // return kOk, kError and kNotExist
    Result GotoFile();

    void Copy() { area_.Copy(); }
    Result Paste(size_t count, bool after_cursor) {
        return area_.Paste(count, after_cursor);
    }
    void Cut() { area_.Cut(); }

    Result IndentSelection(size_t count) {
        return area_.IndentSelection(count);
    }
    Result UnindentSelection(size_t count) {
        return area_.UnindentSelection(count);
    }

    void NextBuffer();
    void PrevBuffer();

    // attach will first detach
    void AttachBuffer(Buffer* buffer);
    // dangerous op, must attach buffer before preprocess & draw
    void DetachBuffer();

    void OnBufferDelete(const Buffer* buffer);

    // Search relevant
    void BuildSearchContext(const std::string& pattern,
                            const Range* range) override {
        b_search_context_ = BufferSearchContext{pattern, area_.buffer_, range};
    }
    void DestorySearchContext() { b_search_context_.Destroy(); }
    const std::string& GetSearchPattern() override {
        return b_search_context_.search_pattern;
    }
    SearchState CursorGoSearchResult(bool next, size_t count,
                                     bool keep_current_if_one) override;
    bool ViewGoSearchResult(bool next, size_t count,
                            bool keep_current_if_one) override {
        return area_.BufferViewGoSearchResult(b_search_context_, next, count,
                                              keep_current_if_one);
    }

    void InsertJumpHistory();
    bool FarEnoughWithCursor(const CursorState& state);
    void MoveJumpHistoryCursorForwardAndTruncate();
    void SetJumpPoint();
    void JumpForward();
    void JumpBackward();
    // TODO: Better name.

    void SaveView() override;
    void RestoreView() override;

    const Opts& opts() { return opts_; }

   private:
    static int64_t AllocId() noexcept;
    TextWindow() {}  // only for list head

    // We try to auto indent from pos.
    // We still use the cursor pos for history.
    Result TryAutoIndent(Pos pos);
    Result TryAutoPair(std::string_view str);

    template <typename T>
    T GetOpt(OptKey key) {
        if (opts_.GetScope(key) == OptScope::kGlobal) {
            return opts_.global_opts_->GetOpt<T>(key);
        }
        if (opts_.GetScope(key) == OptScope::kBuffer) {
            return area_.buffer_->opts().GetOpt<T>(key);
        }
        return opts_.GetOpt<T>(key);
    }

   private:
    Cursor* cursor_;
    Opts opts_ = {nullptr};
    SyntaxParser* parser_;
    BufferManager* buffer_manager_;
    std::unordered_map<int64_t, BufferView> buffer_views_;

    struct JumpPoint {
        BufferView b_view;
        int64_t buffer;

        JumpPoint(const BufferView& _b_view, int64_t _buffer)
            : b_view(_b_view), buffer(_buffer) {}
    };
    // TODO: use History facility
    // currently implemantion of jump point history is more complicated than
    // History, so we don't refactor to use History now.
    using JumpHistory = std::list<JumpPoint>;
    std::unique_ptr<JumpHistory> jump_history_ =
        std::make_unique<JumpHistory>();
    // History cursor maybe at end, means we just create a jump point and jump
    // here. when we want to jump to other places, current buffer view and
    // cursor must be saved at where the history cursor points to.
    JumpHistory::iterator jump_history_cursor_ = jump_history_->end();

    BufferView stored_b_view_;

    int64_t id_ = AllocId();
    static int64_t cur_window_id_;

   public:
    TextArea area_;
    BufferSearchContext b_search_context_;
};

}  // namespace charxed
