#include "window.h"

#include <gsl/util>

#include "buffer.h"
#include "buffer_manager.h"
#include "character.h"
#include "cursor.h"
#include "options.h"
#include "syntax.h"

namespace mango {
Window::Window(Cursor* cursor, GlobalOpts* global_opts, SyntaxParser* parser,
               ClipBoard* clipboard, BufferManager* buffer_manager) noexcept
    : cursor_(cursor),
      opts_(global_opts),
      parser_(parser),
      buffer_manager_(buffer_manager),
      frame_(cursor, &opts_, parser, clipboard) {}

void Window::CursorGoUp(size_t count) {
    MGO_ASSERT(count != 0);
    frame_.b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (frame_.CursorGoUpState(count, state)) {
        if (SetJumpPointIfFarEnough(state)) {
            MoveJumpHistoryCursorForwardAndTruncate();
        }
        state.SetCursor(cursor_);
        frame_.SelectionFollowCursor();
    }
}

void Window::CursorGoDown(size_t count) {
    frame_.b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (frame_.CursorGoDownState(count, state)) {
        if (SetJumpPointIfFarEnough(state)) {
            MoveJumpHistoryCursorForwardAndTruncate();
        }
        state.SetCursor(cursor_);
        frame_.SelectionFollowCursor();
    }
}

void Window::CursorGoLine(size_t line) {
    frame_.b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (frame_.CursorGoLineState(line, state)) {
        if (SetJumpPointIfFarEnough(state)) {
            MoveJumpHistoryCursorForwardAndTruncate();
        }
        state.SetCursor(cursor_);
        frame_.SelectionFollowCursor();
    }
}

Result Window::DeleteAtCursor() {
    if (frame_.selection_.active) {
        return frame_.DeleteSelection();
    }

    Buffer* buffer = frame_.buffer_;
    Range range;
    if (cursor_->byte_offset == 0) {  // first byte
        if (cursor_->line == 0) {
            return kFail;
        }
        range = {{cursor_->line - 1, buffer->GetLine(cursor_->line - 1).size()},
                 {cursor_->line, 0}};
    } else if (GetOpt<bool>(kOptTabSpace)) {
        bool all_space = true;
        const std::string& line = buffer->GetLine(cursor_->line);
        for (size_t byte_offset = 0; byte_offset < cursor_->byte_offset;
             byte_offset++) {
            if (line[byte_offset] != kSpaceChar) {
                all_space = false;
                break;
            }
        }
        if (all_space) {
            range = {{cursor_->line, (cursor_->byte_offset - 1) / 4 * 4},
                     {cursor_->line, cursor_->byte_offset}};
        } else {
            goto slow;
        }
    } else {
    slow:
        const std::string& cur_line = frame_.buffer_->GetLine(cursor_->line);
        Character character;
        int len;
        Result ret =
            PrevCharacter(cur_line, cursor_->byte_offset, character, len);
        MGO_ASSERT(ret == kOk);

        char c = -1;
        character.Ascii(c);

        if (cur_line.size() == cursor_->byte_offset) {
            range = {{cursor_->line, cursor_->byte_offset - len},
                     {cursor_->line, cursor_->byte_offset}};
        } else {
            // may delete pairs
            int byte_len;
            ThisCharacter(cur_line, cursor_->byte_offset, character, byte_len);
            char this_char = -1;
            character.Ascii(this_char);
            bool need_delete_pairs =
                c != -1 && this_char != -1 && IsPair(c, this_char);
            range = {{cursor_->line, cursor_->byte_offset - len},
                     {cursor_->line,
                      cursor_->byte_offset + (need_delete_pairs ? 1 : 0)}};
        }
    }

    Pos pos;
    if (Result res; (res = buffer->Delete(range, nullptr, pos)) != kOk) {
        return res;
    }
    cursor_->SetPos(pos);
    cursor_->DontHoldColWant();
    parser_->ParseSyntaxAfterEdit(buffer);
    return kOk;
}

Result Window::AddStringAtCursor(std::string str, bool raw) {
    if (raw) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    // TODO: better support autopair autoindent when selection
    if (frame_.selection_.active) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    char c = -1;
    if (str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0) {
        c = str[0];
    }

    if (c == -1) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    if (GetOpt<bool>(kOptAutoIndent) && c == '\n') {
        TryAutoIndent();
    } else if (GetOpt<bool>(kOptAutoPair)) {
        if (IsPair(c)) {
            TryAutoPair(std::move(str));
        } else {
            return frame_.AddStringAtCursor(std::move(str));
        }
    }
    // Will not reach here.
    return kOk;
}

Result Window::Replace(const Range& range, std::string str) {
    return frame_.Replace(range, std::move(str));
}

Result Window::TryAutoPair(std::string str) {
    MGO_ASSERT(str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0);

    bool end_of_line =
        frame_.buffer_->GetLine(cursor_->line).size() == cursor_->byte_offset;
    char cur_c =
        end_of_line
            ? -1
            : frame_.buffer_->GetLine(
                  cursor_->line)[cursor_->byte_offset];  // -1 here just makes
                                                         // compiler happy, not
                                                         // used.
    bool start_of_line = cursor_->byte_offset == 0;
    char prev_c =
        start_of_line
            ? -1
            : frame_.buffer_->GetLine(cursor_->line)[cursor_->byte_offset - 1];
    // Can we just move cursor next ?
    // e.g. (<cursor>) and input ')', we just move cursor right to ()<cursor>
    if (!start_of_line && !end_of_line) {
        if (IsPair(prev_c, cur_c) && cur_c == str[0]) {
            if (!frame_.buffer_->IsLoad()) {
                return kBufferCannotLoad;
            }
            if (frame_.buffer_->read_only()) {
                return kBufferReadOnly;
            }
            cursor_->byte_offset++;
            cursor_->DontHoldColWant();
            return kOk;
        }
    }

    // Can't just move cursor next.
    // try auto pair
    auto [is_open, c_close] = IsPairOpen(str[0]);
    if (!is_open) {
        return frame_.AddStringAtCursor(std::move(str));
    }

    if (end_of_line || !IsPair(str[0], cur_c)) {
        Pos pos = {cursor_->line, cursor_->byte_offset + 1};
        str += c_close;
        return frame_.AddStringAtCursor(std::move(str), &pos);
    }

    return frame_.AddStringAtCursor(std::move(str));
}

Result Window::TryAutoIndent() {
    const std::string& line = frame_.buffer_->GetLine(cursor_->line);
    std::string indent = "";
    std::string str = "\n";
    // keep with this line's indent
    size_t i = 0;
    size_t cur_indent = 0;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (; i < line.size(); i++) {
        if (line[i] == kSpaceChar) {
            cur_indent++;
            indent.push_back(line[i]);
        } else if (line[i] == '\t') {
            cur_indent = (cur_indent / tabstop + 1) * tabstop;
            indent.push_back(line[i]);
        } else {
            break;
        }
    }

    // When encounter some syntax blocks, We want one more indentation and sth.
    // else.
    Pos cursor_pos;
    bool maunally_set_cursor_pos = false;
    str += indent;
    zstring_view ft = frame_.buffer_->filetype();
    auto tabspace = GetOpt<bool>(kOptTabSpace);
    if (ft == "c" || ft == "cpp" || ft == "java") {  // TODO: more languages
        // traditional languages, try to check () {} []
        // e.g.
        // {<cursor>}
        // ->
        // {
        // <indent><cursor>
        // }
        for (int64_t i = cursor_->byte_offset - 1; i >= 0; i--) {
            auto [is_open, want_right] = IsPairOpen(line[i]);
            if (!is_open) {
                if (line[i] == kSpaceChar) {
                    continue;
                }
                break;
            }

            if (tabspace) {
                int need_space =
                    (cur_indent / tabstop + 1) * tabstop - cur_indent;
                str += std::string(need_space, kSpaceChar);
            } else {
                str += "\t";
            }
            for (i = cursor_->byte_offset;
                 i < static_cast<int64_t>(line.size()); i++) {
                if (line[i] == want_right) {
                    cursor_pos.line = cursor_->line + 1;
                    cursor_pos.byte_offset = str.size() - 1;
                    maunally_set_cursor_pos = true;
                    str += "\n" + indent;
                    break;
                } else if (line[i] != kSpaceChar) {
                    break;
                }
            }
            break;
        }
    }
    if (maunally_set_cursor_pos) {
        return frame_.AddStringAtCursor(std::move(str), &cursor_pos);
    } else {
        return frame_.AddStringAtCursor(std::move(str));
    }
}

void Window::NextBuffer() {
    if (frame_.buffer_->IsLastBuffer()) {
        return;
    }
    Buffer* next = frame_.buffer_->next_;
    AttachBuffer(next);
}

void Window::PrevBuffer() {
    if (frame_.buffer_->IsFirstBuffer()) {
        return;
    }
    Buffer* prev = frame_.buffer_->prev_;
    AttachBuffer(prev);
}

void Window::AttachBuffer(Buffer* buffer) {
    DetachBuffer();
    MoveJumpHistoryCursorForwardAndTruncate();
    auto b_view_iter = buffer_views_.find(buffer->id());
    if (b_view_iter == buffer_views_.end()) {
        bool ok;
        std::tie(b_view_iter, ok) =
            buffer_views_.emplace(buffer->id(), BufferView());
        MGO_ASSERT(ok);
    }
    b_view_iter->second.RestoreCursorState(cursor_, buffer);
    frame_.b_view_ = &b_view_iter->second;
    frame_.buffer_ = buffer;
}

void Window::DetachBuffer() {
    if (frame_.buffer_) {
        frame_.b_view_->SaveCursorState(cursor_);
        SetJumpPoint();
        frame_.b_view_ = nullptr;
        frame_.buffer_ = nullptr;
        frame_.selection_.active = false;
    }
}

void Window::OnBufferDelete(const Buffer* buffer) {
    if (buffer == frame_.buffer_) {
        if (buffer->IsFirstBuffer() && buffer->IsLastBuffer()) {
            AttachBuffer(buffer_manager_->AddBuffer({opts_.global_opts_}));
        } else if (buffer->IsFirstBuffer()) {
            NextBuffer();
        } else {
            PrevBuffer();
        }
    }
    buffer_views_.erase(buffer->id());
}

void Window::BuildSearchContext(std::string pattern) {
    search_pattern_ = std::move(pattern);
    search_result_ = frame_.buffer_->Search(search_pattern_);
    search_buffer_version_ = frame_.buffer_->version();
}

void Window::DestorySearchContext() {
    search_pattern_.clear();
    search_result_.clear();
    search_buffer_version_ = -1;
    search_buffer_id_ = -1;
}

Window::SearchState Window::CursorGoNextSearchResult() {
    if (search_buffer_version_ == -1) {
        return {};
    }

    if (frame_.buffer_->id() != search_buffer_id_ ||
        frame_.buffer_->version() != search_buffer_version_) {
        // Another buffer or the buffer has changed, we do search again.
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
        search_buffer_id_ = frame_.buffer_->id();
    }

    if (search_result_.size() == 0) {
        return {};
    }

    int64_t left = 0, right = search_result_.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        size_t line = search_result_[mid].begin.line;
        size_t byte_offset = search_result_[mid].begin.byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (static_cast<size_t>(mid) == search_result_.size() - 1) {
                mid = 0;
            } else {
                mid++;
            }
            cursor_->line = search_result_[mid].begin.line;
            cursor_->byte_offset = search_result_[mid].begin.byte_offset;
            cursor_->DontHoldColWant();
            return {static_cast<size_t>(mid + 1), search_result_.size()};
        } else if (cursor_->line < line ||
                   (cursor_->line == line &&
                    cursor_->byte_offset < byte_offset)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    if (static_cast<size_t>(left) == search_result_.size()) {
        cursor_->line = search_result_[0].begin.line;
        cursor_->byte_offset = search_result_[0].begin.byte_offset;
        cursor_->DontHoldColWant();
        return {1, search_result_.size()};
    }
    cursor_->line = search_result_[left].begin.line;
    cursor_->byte_offset = search_result_[left].begin.byte_offset;
    cursor_->DontHoldColWant();
    return {static_cast<size_t>(left + 1), search_result_.size()};
}

Window::SearchState Window::CursorGoPrevSearchResult() {
    if (search_buffer_version_ == -1) {
        return {};
    }

    if (frame_.buffer_->id() != search_buffer_id_ ||
        frame_.buffer_->version() != search_buffer_version_) {
        // Another buffer or the buffer has changed, we do search again.
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
        search_buffer_id_ = frame_.buffer_->id();
    }

    if (search_result_.size() == 0) {
        return {};
    }

    int64_t left = 0, right = search_result_.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        size_t line = search_result_[mid].begin.line;
        size_t byte_offset = search_result_[mid].begin.byte_offset;
        if (line == cursor_->line && byte_offset == cursor_->byte_offset) {
            if (mid == 0) {
                mid = search_result_.size() - 1;
            } else {
                mid--;
            }
            cursor_->line = search_result_[mid].begin.line;
            cursor_->byte_offset = search_result_[mid].begin.byte_offset;
            cursor_->DontHoldColWant();
            return {static_cast<size_t>(mid + 1), search_result_.size()};
        } else if (cursor_->line < line ||
                   (cursor_->line == line &&
                    cursor_->byte_offset < byte_offset)) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    if (static_cast<size_t>(left) == 0) {
        size_t last = search_result_.size() - 1;
        cursor_->line = search_result_[last].begin.line;
        cursor_->byte_offset = search_result_[last].begin.byte_offset;
        cursor_->DontHoldColWant();
        return {last, search_result_.size()};
    }
    cursor_->line = search_result_[left - 1].begin.line;
    cursor_->byte_offset = search_result_[left - 1].begin.byte_offset;
    cursor_->DontHoldColWant();
    return {static_cast<size_t>(left), search_result_.size()};
}

void Window::SetJumpPoint() {
    BufferView b_view = *frame_.b_view_;
    b_view.SaveCursorState(cursor_);
    if (jump_history_cursor_ == jump_history_->end()) {
        jump_history_->emplace_back(b_view, frame_.buffer_->id());
        jump_history_cursor_--;
        return;
    }
    jump_history_cursor_->b_view = b_view;
    jump_history_cursor_->buffer = frame_.buffer_->id();
}

bool Window::SetJumpPointIfFarEnough(CursorState& state) {
    if ((cursor_->line > state.line
             ? cursor_->line - state.line
             : state.line - cursor_->line) >= frame_.height_ / 2) {
        SetJumpPoint();
        return true;
    }
    return false;
}

void Window::MoveJumpHistoryCursorForwardAndTruncate() {
    if (jump_history_cursor_ != jump_history_->end()) {
        jump_history_cursor_ =
            jump_history_->erase(++jump_history_cursor_, jump_history_->end());
    }
    while (jump_history_->size() >
           static_cast<size_t>(GetOpt<int64_t>(kOptMaxJumpHistory))) {
        jump_history_->pop_front();
    }
}

void Window::JumpForward() {
    JumpHistory::iterator iter = jump_history_cursor_;
    if (iter == jump_history_->end() || ++iter == jump_history_->end()) {
        return;
    }
    while (iter != jump_history_->end()) {
        Buffer* b = buffer_manager_->FindBuffer(iter->buffer);
        if (b == nullptr) {
            iter = jump_history_->erase(iter);
            continue;
        }
        DetachBuffer();
        buffer_views_[b->id()] = iter->b_view;
        frame_.b_view_ = &buffer_views_[b->id()];
        frame_.buffer_ = b;
        frame_.b_view_->RestoreCursorState(cursor_, b);
        jump_history_cursor_ = iter;
        return;
    }
}

void Window::JumpBackward() {
    JumpHistory::iterator iter = jump_history_cursor_;
    if (iter == jump_history_->begin()) {
        return;
    }
    while (iter != jump_history_->begin()) {
        iter--;
        MGO_ASSERT(iter != jump_history_->end());
        Buffer* b = buffer_manager_->FindBuffer(iter->buffer);
        if (b == nullptr) {
            iter = jump_history_->erase(iter);
            continue;
        }
        DetachBuffer();
        buffer_views_[b->id()] = iter->b_view;
        frame_.b_view_ = &buffer_views_[b->id()];
        frame_.buffer_ = b;
        frame_.b_view_->RestoreCursorState(cursor_, frame_.buffer_);
        jump_history_cursor_ = iter;
        return;
    }
}

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango