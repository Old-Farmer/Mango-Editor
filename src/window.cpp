#include "window.h"

#include <gsl/util>

#include "buffer.h"
#include "buffer_manager.h"
#include "character.h"
#include "cursor.h"
#include "options.h"
#include "search.h"
#include "syntax.h"

namespace mango {
Window::Window(Cursor* cursor, GlobalOpts* global_opts, SyntaxParser* parser,
               ClipBoard* clipboard, BufferManager* buffer_manager) noexcept
    : cursor_(cursor),
      opts_(global_opts),
      parser_(parser),
      buffer_manager_(buffer_manager),
      frame_(cursor, &opts_, parser, clipboard) {}

void Window::CursorGoLine(size_t line) {
    frame_.b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (frame_.CursorGoLineState(line, state)) {
        if (FarEnoughWithCursor(state)) {
            SetJumpPoint();
        }
        state.SetCursor(cursor_);
        frame_.SelectionFollowCursor();
    }
}

Result Window::DeleteAtCursor() {
    if (frame_.IsSelectionActive()) {
        return frame_.DeleteSelection();
    }

    Buffer* buffer = frame_.buffer_;
    Range range;
    if (cursor_->pos.byte_offset == 0) {  // first byte
        if (cursor_->pos.line == 0) {
            return kFail;
        }
        range = {{cursor_->pos.line - 1,
                  buffer->GetLine(cursor_->pos.line - 1).size()},
                 {cursor_->pos.line, 0}};
    } else if (GetOpt<bool>(kOptTabSpace)) {
        bool all_space = true;
        const std::string& line = buffer->GetLine(cursor_->pos.line);
        for (size_t byte_offset = 0; byte_offset < cursor_->pos.byte_offset;
             byte_offset++) {
            if (line[byte_offset] != kSpaceChar) {
                all_space = false;
                break;
            }
        }
        if (all_space) {
            range = {
                {cursor_->pos.line, (cursor_->pos.byte_offset - 1) / 4 * 4},
                cursor_->pos};
        } else {
            goto slow;
        }
    } else {
    slow:
        const std::string& cur_line =
            frame_.buffer_->GetLine(cursor_->pos.line);
        Character character;
        int len;
        PrevCharacter(cur_line, cursor_->pos.byte_offset, character, len);

        char c = -1;
        character.Ascii(c);

        if (cur_line.size() == cursor_->pos.byte_offset) {
            range = {{cursor_->pos.line, cursor_->pos.byte_offset - len},
                     cursor_->pos};
        } else {
            // may delete pairs
            int byte_len;
            ThisCharacter(cur_line, cursor_->pos.byte_offset, character,
                          byte_len);
            char this_char = -1;
            character.Ascii(this_char);
            bool need_delete_pairs =
                c != -1 && this_char != -1 && IsPair(c, this_char);
            range = {{cursor_->pos.line, cursor_->pos.byte_offset - len},
                     {cursor_->pos.line,
                      cursor_->pos.byte_offset + (need_delete_pairs ? 1 : 0)}};
        }
    }

    Pos pos;
    if (Result res; (res = buffer->Delete(range, nullptr, pos)) != kOk) {
        return res;
    }
    cursor_->pos = pos;
    cursor_->DontHoldColWant();
    parser_->ParseSyntaxAfterEdit(buffer);
    return kOk;
}

Result Window::AddStringAtCursor(std::string_view str, bool raw) {
    if (raw) {
        return frame_.AddStringAtCursor(str);
    }

    // TODO: better support autopair autoindent when selection
    if (frame_.IsSelectionActive()) {
        return frame_.AddStringAtCursor(str);
    }

    char c = -1;
    if (str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0) {
        c = str[0];
    }

    if (c == -1) {
        return frame_.AddStringAtCursor(str);
    }

    if (GetOpt<bool>(kOptAutoIndent) && c == '\n') {
        return TryAutoIndent(cursor_->pos);
    } else if (GetOpt<bool>(kOptAutoPair)) {
        if (IsPair(c)) {
            return TryAutoPair(str);
        } else {
            return frame_.AddStringAtCursor(str);
        }
    }
    // Will not reach here.
    return kOk;
}

Result Window::NewLineAboveCursorline() {
    MGO_ASSERT(!frame_.IsSelectionActive());
    if (cursor_->pos.line == 0) {
        return frame_.AddStringAtPos({0, 0}, "\n", &cursor_->pos);
    }
    return TryAutoIndent(
        {cursor_->pos.line - 1,
         frame_.buffer_->GetLine(cursor_->pos.line - 1).size()});
}

Result Window::NewLineUnderCursorline() {
    MGO_ASSERT(!frame_.IsSelectionActive());
    return TryAutoIndent(
        {cursor_->pos.line, frame_.buffer_->GetLine(cursor_->pos.line).size()});
}

Result Window::Replace(const Range& range, std::string_view str) {
    return frame_.Replace(range, str);
}

Result Window::TryAutoPair(std::string_view str) {
    MGO_ASSERT(str.size() == 1 && str[0] < CHAR_MAX && str[0] >= 0);

    bool end_of_line = frame_.buffer_->GetLine(cursor_->pos.line).size() ==
                       cursor_->pos.byte_offset;
    char cur_c =
        end_of_line
            ? -1
            : frame_.buffer_->GetLine(
                  cursor_->pos
                      .line)[cursor_->pos.byte_offset];  // -1 here just makes
                                                         // compiler happy, not
                                                         // used.
    bool start_of_line = cursor_->pos.byte_offset == 0;
    char prev_c = start_of_line
                      ? -1
                      : frame_.buffer_->GetLine(
                            cursor_->pos.line)[cursor_->pos.byte_offset - 1];
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
            cursor_->pos.byte_offset++;
            cursor_->DontHoldColWant();
            return kOk;
        }
    }

    // Can't just move cursor next.
    // try auto pair
    auto [is_open, c_close] = IsPairOpen(str[0]);
    if (!is_open) {
        return frame_.AddStringAtCursor(str);
    }

    if (end_of_line || !IsPair(str[0], cur_c)) {
        Pos pos = {cursor_->pos.line, cursor_->pos.byte_offset + 1};
        std::string pairs = std::string(str) + c_close;
        return frame_.AddStringAtCursor(pairs, &pos);
    }

    return frame_.AddStringAtCursor(str);
}

Result Window::TryAutoIndent(Pos pos) {
    MGO_ASSERT(!frame_.IsSelectionActive());

    const std::string& line = frame_.buffer_->GetLine(pos.line);
    std::string indent = "";
    size_t cur_indent = 0;
    // Same as this line's indent
    size_t i = 0;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (; i < line.size(); i++) {
        if (line[i] == kSpaceChar) {
            cur_indent++;
            indent.push_back(kSpaceChar);
        } else if (line[i] == '\t') {
            cur_indent = (cur_indent / tabstop + 1) * tabstop;
            indent.push_back('\t');
        } else {
            break;
        }
    }

    // When encounter some syntax blocks, We want one more indentation and sth.
    // else.
    // TODO: better language support
    Pos cursor_pos;
    bool maunally_set_cursor_pos = false;
    std::string str = "\n" + indent;
    auto tabspace = GetOpt<bool>(kOptTabSpace);
    // Try to check () {} [].
    // e.g.
    // {<cursor>}
    // ->
    // {
    // <indent><cursor>
    // }
    for (int64_t i = pos.byte_offset - 1; i >= 0; i--) {
        auto [is_open, want_right] = IsPairOpen(line[i]);
        if (!is_open) {
            if (line[i] == kSpaceChar || line[i] == '\t') {
                continue;
            }
            break;
        }

        if (tabspace) {
            int need_space = (cur_indent / tabstop + 1) * tabstop - cur_indent;
            str += std::string(need_space, kSpaceChar);
        } else {
            str += "\t";
        }
        for (i = pos.byte_offset; i < static_cast<int64_t>(line.size()); i++) {
            if (line[i] == want_right) {
                cursor_pos.line = pos.line + 1;
                cursor_pos.byte_offset = str.size() - 1;
                maunally_set_cursor_pos = true;
                str += "\n" + indent;
                break;
            } else if (line[i] != kSpaceChar && line[i] != '\t') {
                break;
            }
        }
        break;
    }

    if (maunally_set_cursor_pos) {
        return frame_.AddStringAtPos(pos, str, &cursor_pos);
    } else {
        return frame_.AddStringAtPos(pos, str);
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
        InsertJumpHistory();
        frame_.b_view_ = nullptr;
        frame_.buffer_ = nullptr;
        frame_.StopSelection();
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

BufferSearchState Window::CursorGoSearchResult(bool next, size_t count,
                                               bool keep_current_if_one) {
    CursorState state(cursor_);
    BufferSearchState search_state = frame_.CursorGoSearchResultState(
        b_search_context_, next, count, keep_current_if_one, state);
    if (search_state.total == 0) {
        return search_state;
    }
    SetJumpPoint();
    state.SetCursor(cursor_);
    return search_state;
}

void Window::InsertJumpHistory() {
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

bool Window::FarEnoughWithCursor(const CursorState& state) {
    return (cursor_->pos.line > state.pos.line
                ? cursor_->pos.line - state.pos.line
                : state.pos.line - cursor_->pos.line) >= frame_.height_ / 2;
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

void Window::SetJumpPoint() {
    InsertJumpHistory();
    MoveJumpHistoryCursorForwardAndTruncate();
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
