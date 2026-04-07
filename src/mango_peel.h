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

    void Draw() { area_.Draw(nullptr); }

    void MakeCursorVisible();

    void CursorGoUp(size_t count) { area_.CursorGoUp(count); }
    void CursorGoDown(size_t count) { area_.CursorGoDown(count); }
    void CursorGoRight(size_t count) { area_.CursorGoRight(count); }
    void CursorGoLeft(size_t count) { area_.CursorGoLeft(count); }
    void CursorGoHome() { area_.CursorGoHome(); }
    void CursorGoEnd() { area_.CursorGoEnd(); }
    void CursorGoNextWordEnd(size_t count, bool one_more_character) {
        area_.CursorGoNextWordEnd(count, one_more_character);
    }
    void CursorGoPrevWordBegin(size_t count) {
        area_.CursorGoPrevWordBegin(count);
    }

    Result DeleteCharacterBeforeCursor() {
        return area_.DeleteCharacterBeforeCursor();
    }
    Result DeleteWordBeforeCursor() { return area_.DeleteWordBeforeCursor(); }
    Result AddStringAtCursor(std::string str) {
        return area_.AddStringAtCursor(std::move(str));
    }

    void Copy() { area_.Copy(false); }
    Result Paste(size_t count);

    void SetContent(std::string_view content);
    std::string_view GetCmdContent();

    // The height of peel needed for rendering its content.
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
    BufferView b_view_;  // And it's view
    Opts opts_;          // local opts

   public:
    Buffer buffer_;  // Unlike window, Peel owns her nofilebacked buffer
    TextArea area_;
    PeelCompleter completer_;
};

}  // namespace mango
