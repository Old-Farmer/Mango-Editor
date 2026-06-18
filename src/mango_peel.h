#pragma once

#include "buffer.h"
#include "text_area.h"

namespace charxed {

class CommandManager;

// Mango peel is an user interaction area.
// It fix at the bottome of the screan.
// It usually shows in one row, when multiple row contents needed, it will
// extend it row numbers.
// This class is named like this because the project was initially called
// "mango".
class MangoPeel {
   public:
    MangoPeel(Cursor* cursor, GlobalOpts* global_opts, ClipBoard* clipboard,
              BufferManager* buffer_manager, CommandManager* command_manager);
    ~MangoPeel() = default;
    CHX_DELETE_COPY(MangoPeel);
    CHX_DEFAULT_MOVE(MangoPeel);

    void Draw();

    void MakeCursorVisible();

    void SetCursorPosToAppend();

    void CursorGoUp(size_t count);
    void CursorGoDown(size_t count);
    void CursorGoHalfPageUp(size_t count) {
        area_.CursorGoUp(count * area_.height_ / 2);
    }
    void CursorGoHalfPageDown(size_t count) {
        area_.CursorGoDown(count * area_.height_ / 2);
    }
    void CursorGoPageUp(size_t count) {
        area_.CursorGoUp(count * area_.height_);
    }
    void CursorGoPageDown(size_t count) {
        area_.CursorGoDown(count * area_.height_);
    }
    void CursorGoRight(size_t count);
    void CursorGoLeft(size_t count);
    void CursorGoHome();
    void CursorGoEnd();
    void CursorGoNextWordEnd(size_t count);
    void CursorGoNextWordBegin(size_t count);
    void CursorGoPrevWordBegin(size_t count);
    void CursorGoBracket() { area_.CursorGoBracket(); }
    void CursorGoLine(size_t line) { area_.CursorGoLine(line); }
    void Copy();

    // One line editable peel, Users can input sth to it.

    // Start Userinput, Prefix should be readable ascii, will clear content.
    // cursor will be set too.
    void UserInputStart(std::string_view prefix);
    // Clear userinput, and set cursor to the input beginning.
    void ClearUserInput();
    std::string_view GetUserInput();
    Result DeleteCharacterBeforeCursor();
    Result DeleteWordBeforeCursor();
    Result AddStringAtCursor(std::string_view str);
    Result Paste();

    // History manipulate
    enum class HistoryType {
        kCmd,
        kSearch,

        __kCount,
    };

    // set peel to prev history item, stately
    void PrevHistoryItem(HistoryType history);
    // set peel to next history item, stately
    void NextHistoryItem(HistoryType history);
    void AppendHistoryItem(HistoryType history);
    void SetHistoryCursorToEnd();

    // Non editable, but multiple line is ok.
    void ShowContent(std::string_view content);

    // The height of peel needed for rendering peel.
    size_t NeedHeight(size_t width);

   private:
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
    BufferView b_view_;      // And it's view
    Opts opts_;              // local opts
    size_t prefix_len_ = 0;  // because prefix is only ascii, so prefix len is
                             // its width.
    bool user_inputing_ = false;

    History<std::string> history_[static_cast<size_t>(HistoryType::__kCount)];
    std::string
        current_input_;  // When users want to view the history, this field
                         // records the parts that have been input.

   public:
    Buffer buffer_;  // Unlike window, Peel owns her nofilebacked buffer
    TextArea area_;
    PeelCompleter completer_;
};

}  // namespace charxed
