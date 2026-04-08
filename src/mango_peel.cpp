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

void MangoPeel::Draw() { area_.Draw(nullptr); }

void MangoPeel::MakeCursorVisible() {
    MGO_ASSERT(area_.cursor_->in_window == nullptr);
    area_.MakeCursorVisible();
}

void MangoPeel::SetCursorPosToAppend() {
    size_t last_l = buffer_.LineCnt() - 1;
    area_.cursor_->pos = {last_l, buffer_.GetLine(last_l).size()};
};

void MangoPeel::CursorGoUp(size_t count) { area_.CursorGoUp(count); }
void MangoPeel::CursorGoDown(size_t count) { area_.CursorGoDown(count); }
void MangoPeel::CursorGoRight(size_t count) { area_.CursorGoRight(count); }
void MangoPeel::CursorGoLeft(size_t count) {
    area_.b_view_->make_cursor_visible = true;
    CursorState s(area_.cursor_);
    area_.CursorGoLeftState(count, s);
    if (Range{{0, 0}, {0, prefix_len_}}.PosInMe(s.pos)) {
        s.pos = {0, prefix_len_};
    }
    s.SetCursor(area_.cursor_);
}
void MangoPeel::CursorGoHome() {
    area_.b_view_->make_cursor_visible = true;
    CursorState s(area_.cursor_);
    if (!area_.CursorGoHomeState(s)) {
        return;
    }
    if (Range{{0, 0}, {0, prefix_len_}}.PosInMe(s.pos)) {
        s.pos = {0, prefix_len_};
    }
    s.SetCursor(area_.cursor_);
}
void MangoPeel::CursorGoEnd() { area_.CursorGoEnd(); }
void MangoPeel::CursorGoNextWordEnd(size_t count, bool one_more_character) {
    area_.CursorGoNextWordEnd(count, one_more_character);
}
void MangoPeel::CursorGoPrevWordBegin(size_t count) {
    area_.b_view_->make_cursor_visible = true;
    CursorState s(area_.cursor_);
    if (!area_.CursorGoPrevWordBeginState(count, s)) {
        return;
    }
    if (Range{{0, 0}, {0, prefix_len_}}.PosInMe(s.pos)) {
        s.pos = {0, prefix_len_};
    }
    s.SetCursor(area_.cursor_);
}

void MangoPeel::Copy() {
    if (user_inputing_) {
        area_.clipboard_->SetContent(std::string(GetUserInput()), true);
    } else {
        area_.Copy();
    }
}

void MangoPeel::UserInputStart(std::string_view prefix) {
    user_inputing_ = true;
    buffer_.Clear();
    Pos pos;
    prefix_len_ = prefix.size();
    buffer_.Add({0, 0}, prefix, nullptr, false, pos);
    area_.cursor_->pos = Pos{0, prefix_len_};
}

std::string_view MangoPeel::GetUserInput() {
    if (!user_inputing_) {
        return {};
    }
    return buffer_.GetLine(0).substr(prefix_len_);
}

Result MangoPeel::DeleteCharacterBeforeCursor() {
    if (!user_inputing_) {
        return kFail;
    }
    MGO_ASSERT(area_.cursor_->pos.line == 0);
    if (area_.cursor_->pos == Pos{0, prefix_len_}) {
        area_.b_view_->make_cursor_visible = true;
        area_.cursor_->DontHoldColWant();
        return kFail;
    } else {
        return area_.DeleteCharacterBeforeCursor();
    }
}

Result MangoPeel::DeleteWordBeforeCursor() {
    if (!user_inputing_) {
        return kFail;
    }

    area_.b_view_->make_cursor_visible = true;
    Pos deleted_until;
    MGO_ASSERT(area_.cursor_->pos.line == 0);
    std::string_view line = area_.buffer_->GetLine(0);
    if (area_.cursor_->pos.byte_offset == prefix_len_) {
        return kFail;
    } else {
        deleted_until = area_.cursor_->pos;
        Result res = PrevWordBegin(line, deleted_until.byte_offset,
                                   deleted_until.byte_offset);
        MGO_ASSERT(res == kOk || res == kNotExist);
        (void)res;
        deleted_until.byte_offset =
            std::max(prefix_len_, deleted_until.byte_offset);
    }
    Pos pos;
    if (Result res; (res = buffer_.Delete({deleted_until, area_.cursor_->pos},
                                          nullptr, pos)) != kOk) {
        return res;
    }
    area_.AfterModify(pos);
    return kOk;
}

Result MangoPeel::AddStringAtCursor(std::string str) {
    if (!user_inputing_) {
        return kFail;
    }
    MGO_ASSERT(area_.cursor_->pos.line == 0);
    return area_.AddStringAtCursor(std::move(str));
}

Result MangoPeel::Paste() {
    MGO_ASSERT(!area_.IsSelectionActive());
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
    return area_.AddStringAtCursor(std::move(content));
}

void MangoPeel::ShowContent(std::string_view content) {
    user_inputing_ = false;
    prefix_len_ = 0;
    buffer_.Clear();
    Pos pos;
    buffer_.Add({0, 0}, content, nullptr, false, pos);
}

size_t MangoPeel::NeedHeight(size_t width) {
    Buffer* b = area_.buffer_;
    size_t line_cnt = b->LineCnt();
    size_t height = 0;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (size_t i = 0; i < line_cnt; i++) {
        height += ScreenRows(buffer_.GetLine(i), width, tabstop);
    }
    return std::max<size_t>(height, 1);
}

}  // namespace mango
