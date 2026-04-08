#pragma once

#include "buffer.h"
#include "text_area.h"

namespace mango {

// Mango peel is an user interaction area.
// It fix at the bottome of the screan.
// It usually shows in one row, when multiple row contents needed, it will
// extend it row numbers.
class MangoPeel {
   public:
    MangoPeel(Cursor* cursor, GlobalOpts* global_opts, ClipBoard* clipboard,
              BufferManager* buffer_manager);
    ~MangoPeel() = default;
    MGO_DELETE_COPY(MangoPeel);
    MGO_DEFAULT_MOVE(MangoPeel);

    void Draw();

    void MakeCursorVisible();

    void SetCursorPosToAppend();

    void CursorGoUp(size_t count);
    void CursorGoDown(size_t count);
    void CursorGoRight(size_t count);
    void CursorGoLeft(size_t count);
    void CursorGoHome();
    void CursorGoEnd();
    void CursorGoNextWordEnd(size_t count, bool one_more_character);
    void CursorGoPrevWordBegin(size_t count);
    void Copy();

    // One line editable peel, Users can input sth to it.

    // Start Userinput, Prefix should be readable ascii, will clear content.
    // cursor will be set too.
    void UserInputStart(std::string_view prefix);
    std::string_view GetUserInput();
    Result DeleteCharacterBeforeCursor();
    Result DeleteWordBeforeCursor();
    Result AddStringAtCursor(std::string str);
    Result Paste();

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

   public:
    Buffer buffer_;  // Unlike window, Peel owns her nofilebacked buffer
    TextArea area_;
    PeelCompleter completer_;
};

}  // namespace mango
