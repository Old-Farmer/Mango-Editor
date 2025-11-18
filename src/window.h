#pragma once

#include "buffer.h"
#include "frame.h"

namespace mango {

class Window {
   public:
    Window(Buffer* buffer, Cursor* cursor, GlobalOpts* global_opts,
           SyntaxParser* parser, ClipBoard* clipboard) noexcept;
    ~Window() = default;
    MGO_DELETE_COPY(Window);
    MGO_DEFAULT_MOVE(Window);

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
    void CursorGoNextWordEnd(bool one_more_character);
    void CursorGoPrevWord();

    void SelectAll();

    void DeleteAtCursor();
    void DeleteWordBeforeCursor();
    // raw means do not treat it as keystroke
    void AddStringAtCursor(std::string str, bool raw = false);
    // See Frame::Replace
    Result Replace(const Range& range, std::string str);
    void TabAtCursor();
    void Redo();
    void Undo();

    void Copy();
    void Paste();
    void Cut();

    void TriggerCompletion(bool autocmp);

    void NextBuffer();
    void PrevBuffer();

    // attach will first detach
    void AttachBuffer(Buffer* buffer);
    // dangerous op, must attach buffer before preprocess & draw
    void DetachBuffer();

    // Search relevant
    struct SearchState {
        size_t i = 0;  // from 1 instead of zero
        size_t total = 0;
    };
    //
    void BuildSearchContext(std::string pattern);
    void DestorySearchContext();
    const std::string& GetSearchPattern() { return search_pattern_; }
    SearchState CursorGoNextSearchResult();
    SearchState CursorGoPrevSearchResult();

    const Opts& opts() { return opts_; }

   private:
    static int64_t AllocId() noexcept;
    Window() {}  // only for list head

    void TryAutoIndent();
    void TryAutoPair(std::string str);

    template <typename T>
    T GetOpt(OptKey key) {
        if (opts_.GetScope(key) == OptScope::kGlobal) {
            return opts_.global_opts_->GetOpt<T>(key);
        }
        if (opts_.GetScope(key) == OptScope::kBuffer) {
            return frame_.buffer_->opts().GetOpt<T>(key);
        }
        return opts_.GetOpt<T>(key);
    }

   private:
    Cursor* cursor_;
    Opts opts_ = {nullptr};
    SyntaxParser* parser_;

    std::vector<Range> search_result_;
    std::string search_pattern_;
    int64_t search_buffer_version_ = -1;

    int64_t id_ = AllocId();
    static int64_t cur_window_id_;

   public:
    Frame frame_;
};

}  // namespace mango
