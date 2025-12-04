#include "mango_peel.h"

#include "clipboard.h"
#include "cursor.h"
#include "options.h"

namespace mango {

MangoPeel::MangoPeel(Cursor* cursor, GlobalOpts* global_opts,
                     ClipBoard* clipboard, BufferManager* buffer_manager)
    : buffer_(global_opts, false),
      opts_(global_opts),
      frame_(cursor, &opts_, nullptr, clipboard),
      completer_(this, buffer_manager) {
    buffer_.Load();

    frame_.buffer_ = &buffer_;
    frame_.b_view_ = &b_view_;
    b_view_.cursor_state.reset();

    opts_.SetOpt(kOptLineNumber, static_cast<int64_t>(LineNumberType::kNone));
}

void MangoPeel::Draw() { frame_.Draw(); }

void MangoPeel::MakeCursorVisible() {
    MGO_ASSERT(frame_.cursor_->in_window == nullptr);
    frame_.MakeCursorVisible();
}

Result MangoPeel::Paste() {
    MGO_ASSERT(!frame_.selection_.active);
    bool lines;
    std::string content = frame_.clipboard_->GetContent(lines);
    if (content.empty()) {
        return kFail;
    }

    for (char& c : content) {
        if (c == '\n') {
            c = ' ';
        }
    }
    return frame_.AddStringAtCursor(std::move(content));
}

void MangoPeel::SetContent(std::string content) {
    buffer_.Clear();
    Pos pos;
    buffer_.Add({0, 0}, std::move(content), nullptr, false, pos);
}

const std::string& MangoPeel::GetContent() { return buffer_.GetLine(0); }

}  // namespace mango