#include "mango_peel.h"

#include "clipboard.h"
#include "cursor.h"
#include "draw.h"
#include "options.h"

namespace mango {

MangoPeel::MangoPeel(Cursor* cursor, GlobalOpts* global_opts,
                     ClipBoard* clipboard, BufferManager* buffer_manager)
    : opts_(global_opts),
      buffer_(global_opts, false),
      area_(cursor, &opts_, nullptr, clipboard),
      completer_(this, buffer_manager) {
    buffer_.Load();
    buffer_.opts().SetOpt<int64_t>(kOptMaxEditHistory, 0);
    buffer_.opts().SetOpt(kOptAutoIndent, false);
    buffer_.opts().SetOpt(kOptWrap, true);

    area_.buffer_ = &buffer_;
    area_.b_view_ = &b_view_;
    b_view_.cursor_state_valid = false;

    opts_.SetOpt(kOptLineNumber, static_cast<int64_t>(LineNumberType::kNone));
    opts_.SetOpt(kOptTrailingWhite, false);
}

void MangoPeel::MakeCursorVisible() {
    MGO_ASSERT(area_.cursor_->in_window == nullptr);
    area_.MakeCursorVisible();
}

Result MangoPeel::Paste(size_t count) {
    MGO_ASSERT(!area_.IsSelectionActive());
    MGO_ASSERT(count != 0);
    bool lines;
    std::string content = area_.clipboard_->GetContent(lines);
    if (content.empty()) {
        return kFail;
    }

    for (char& c : content) {
        if (c == '\n') {
            c = ' ';
        }
    }
    if (count != 1) {
        // High potential oom
        try {
            std::string tmp_content = content;
            content.reserve(content.size() * count);
            for (size_t i = 0; i < count - 1; i++) {
                content += tmp_content;
            }
        } catch (std::bad_alloc) {
            return kError;  // TODO: more specific Result?
        }
    }
    return area_.AddStringAtCursor(std::move(content));
}

void MangoPeel::SetContent(std::string_view content) {
    buffer_.Clear();
    Pos pos;
    buffer_.Add({0, 0}, content, nullptr, false, pos);
}

size_t MangoPeel::NeedHeight(size_t width) {
    const Buffer* b = area_.buffer_;
    size_t line_cnt = b->LineCnt();
    size_t height = 0;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (size_t i = 0; i < line_cnt; i++) {
        height += ScreenRows(buffer_.GetLine(i), width, tabstop);
    }
    return std::max<size_t>(height, 1);
}

std::string_view MangoPeel::GetCmdContent() { return buffer_.GetLine(0); }

}  // namespace mango
