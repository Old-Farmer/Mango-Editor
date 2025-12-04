#pragma once

#include "buffer.h"
#include "frame.h"

namespace mango {

class Window {
   public:
    Window(Buffer* buffer, Cursor* cursor, GlobalOpts* global_opts,
           SyntaxParser* parser, ClipBoard* clipboard,
           BufferManager* buffer_manager) noexcept;
    ~Window() = default;
    MGO_DELETE_COPY(Window);
    MGO_DEFAULT_MOVE(Window);

    int id() { return id_; }

    void Draw() { frame_.Draw(); }

    void MakeCursorVisible() { frame_.MakeCursorVisible(); }

    void SetCursorHint(size_t s_row, size_t s_col) {
        frame_.SetCursorHint(s_row, s_col);
    }

    void ScrollRows(int64_t count) { frame_.ScrollRows(count); }
    void ScrollCols(int64_t count) { frame_.ScrollCols(count); }

    void CursorGoRight() { frame_.CursorGoRight(); }
    void CursorGoLeft() { frame_.CursorGoLeft(); }
    void CursorGoUp(size_t count) { frame_.CursorGoUp(count); }
    void CursorGoDown(size_t count) { frame_.CursorGoDown(count); }
    void CursorGoHome() { frame_.CursorGoHome(); }
    void CursorGoEnd() { frame_.CursorGoEnd(); }
    void CursorGoWordEnd(bool one_more_character) {
        frame_.CursorGoWordEnd(one_more_character);
    }
    void CursorGoWordBegin() { frame_.CursorGoWordBegin(); }

    void SelectAll() { frame_.SelectAll(); }

    Result DeleteAtCursor();
    Result DeleteWordBeforeCursor() { return frame_.DeleteWordBeforeCursor(); }
    // raw means do not treat it as keystroke
    Result AddStringAtCursor(std::string str, bool raw = false);
    // See Frame::Replace
    Result Replace(const Range& range, std::string str);
    Result TabAtCursor() { return frame_.TabAtCursor(); }
    Result Redo() { return frame_.Redo(); }
    Result Undo() { return frame_.Undo(); }

    void Copy() { frame_.Copy(); }
    Result Paste() { return frame_.Paste(); }
    void Cut() { frame_.Cut(); }

    void NextBuffer();
    void PrevBuffer();

    // attach will first detach
    void AttachBuffer(Buffer* buffer);
    // dangerous op, must attach buffer before preprocess & draw
    void DetachBuffer();

    void OnBufferDelete(const Buffer* buffer);

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

    Result TryAutoIndent();
    Result TryAutoPair(std::string str);

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
    BufferManager* buffer_manager_;
    std::unordered_map<int64_t, BufferView> buffer_views_;

    std::vector<Range> search_result_;
    std::string search_pattern_;
    int64_t search_buffer_version_ = -1;

    int64_t id_ = AllocId();
    static int64_t cur_window_id_;

   public:
    Frame frame_;
};

}  // namespace mango
