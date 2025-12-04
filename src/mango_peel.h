#pragma once

#include "buffer.h"
#include "frame.h"

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

    void CursorGoRight();
    void CursorGoLeft();
    void CursorGoHome();
    void CursorGoEnd();
    void CursorGoNextWordEnd(bool one_more_character);
    void CursorGoPrevWord();

    Result DeleteCharacterBeforeCursor() {
        return frame_.DeleteCharacterBeforeCursor();
    }
    Result DeleteWordBeforeCursor() { return frame_.DeleteWordBeforeCursor(); }
    Result AddStringAtCursor(std::string str) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    void Copy() { frame_.Paste(); }
    Result Paste();

    void SetContent(std::string content);
    const std::string& GetContent();

   private:
    Buffer buffer_;      // Unlike window, Peel owns her nofilebacked buffer
    BufferView b_view_;  // And it's view
    Opts opts_;

   public:
    Frame frame_;
    PeelCompleter completer_;
};

}  // namespace mango
