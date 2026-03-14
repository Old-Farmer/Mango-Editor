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

    void CursorGoRight(size_t count) { frame_.CursorGoRight(count); }
    void CursorGoLeft(size_t count) { frame_.CursorGoLeft(count); }
    void CursorGoHome() { frame_.CursorGoHome(); }
    void CursorGoEnd() { frame_.CursorGoEnd(); }
    void CursorGoNextWordEnd(size_t count, bool one_more_character) {
        frame_.CursorGoNextWordEnd(count, one_more_character);
    }
    void CursorGoPrevWordBegin(size_t count) {
        frame_.CursorGoPrevWordBegin(count);
    }

    Result DeleteCharacterBeforeCursor() {
        return frame_.DeleteCharacterBeforeCursor();
    }
    Result DeleteWordBeforeCursor() { return frame_.DeleteWordBeforeCursor(); }
    Result AddStringAtCursor(std::string str) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    void Copy() { frame_.Copy(false); }
    Result Paste(size_t count);

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
