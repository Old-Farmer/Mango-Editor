#include "window.h"

#include <gsl/util>

#include "buffer.h"
#include "character.h"
#include "cursor.h"
#include "options.h"
#include "syntax.h"

namespace mango {
Window::Window(Buffer* buffer, Cursor* cursor, Options* options,
               SyntaxParser* parser) noexcept
    : frame_(buffer, cursor, options, parser),
      cursor_(cursor),
      options_(options),
      parser_(parser) {}

void Window::Draw() { frame_.Draw(); }

void Window::MakeCursorVisible() {
    MGO_ASSERT(cursor_->in_window == this);
    frame_.MakeCursorVisible();
}

size_t Window::SetCursorByBViewCol(size_t b_view_col) {
    return frame_.SetCursorByBViewCol(b_view_col);
}

void Window::SetCursorHint(size_t s_row, size_t s_col) {
    frame_.SetCursorHint(s_row, s_col);
}

void Window::ScrollRows(int64_t count) {
    frame_.ScrollRows(count, cursor_->in_window == this);
}

void Window::ScrollCols(int64_t count) { frame_.ScrollCols(count); }

void Window::CursorGoRight() { frame_.CursorGoRight(); }

void Window::CursorGoLeft() { frame_.CursorGoLeft(); }

void Window::CursorGoUp() { frame_.CursorGoUp(); }

void Window::CursorGoDown() { frame_.CursorGoDown(); }

void Window::CursorGoHome() { frame_.CursorGoHome(); }

void Window::CursorGoEnd() { frame_.CursorGoEnd(); }

void Window::CursorGoNextWord() { frame_.CursorGoNextWord(); }

void Window::CursorGoPrevWord() { frame_.CursorGoPrevWord(); }

void Window::DeleteCharacterBeforeCursor() {
    Buffer* buffer = frame_.buffer_;
    Range range;
    if (cursor_->byte_offset == 0) {  // first byte
        if (cursor_->line == 0) {
            return;
        }
        range = {{cursor_->line - 1,
                  buffer->lines()[cursor_->line - 1].line_str.size()},
                 {cursor_->line, 0}};
    } else if (options_->tabspace) {
        bool all_space = true;
        const std::string& line = buffer->lines()[cursor_->line].line_str;
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
        const std::string& cur_line =
            frame_.buffer_->lines()[cursor_->line].line_str;
        std::vector<uint32_t> charater;
        int len, character_width;
        Result ret = PrevCharacterInUtf8(cur_line, cursor_->byte_offset,
                                         charater, len, character_width);
        MGO_ASSERT(ret == kOk);
        if (cur_line.size() <= cursor_->byte_offset) {
            range = {{cursor_->line, cursor_->byte_offset - len},
                     {cursor_->line, cursor_->byte_offset}};
        } else {
            // may delete pairs
            char this_char = cur_line[cursor_->byte_offset];
            bool need_delete_pairs = false;
            switch (charater[0]) {
                case kLeftBraceChar: {
                    if (this_char == kRightBraceChar) {
                        need_delete_pairs = true;
                    }
                    break;
                }
                case kLeftParenthesisChar: {
                    if (this_char == kRightParenthesisChar) {
                        need_delete_pairs = true;
                    }
                    break;
                }
                case kLeftSquareBracketChar: {
                    if (this_char == kRightSquareBracketChar) {
                        need_delete_pairs = true;
                    }
                    break;
                }
                case kSingleQuoteChar: {
                    if (this_char == kSingleQuoteChar) {
                        need_delete_pairs = true;
                    }
                    break;
                }
                case kDoubleQuoteChar: {
                    if (this_char == kDoubleQuoteChar) {
                        need_delete_pairs = true;
                    }
                    break;
                }
                default: {
                }
            }
            range = {{cursor_->line, cursor_->byte_offset - 1},
                     {cursor_->line,
                      cursor_->byte_offset + (need_delete_pairs ? 1 : 0)}};
        }
    }

    Pos pos;
    if (buffer->Delete(range, pos) != kOk) {
        return;
    }
    cursor_->line = pos.line;
    cursor_->byte_offset = pos.byte_offset;
    cursor_->DontHoldColWant();
    parser_->SyntaxHighlightAfterEdit(buffer);
}

void Window::DeleteWordBeforeCursor() { frame_.DeleteWordBeforeCursor(); }

void Window::AddStringAtCursor(std::string str) {
    if (options_->auto_indent && str == kNewLine) {
        TryAutoIndent();
    } else if (options_->auto_pair &&
               (str == kLeftBrace || str == kLeftParenthesis ||
                str == kLeftSquareBracket || str == kSingleQuote ||
                str == kDoubleQuote)) {
        TryAutoPair(std::move(str));
    } else {
        frame_.AddStringAtCursor(std::move(str));
    }
}

void Window::TryAutoPair(std::string str) {
    // Use char is ok here, we only test some ascii characters
    char at_cursor = 0;
    if (frame_.buffer_->lines()[cursor_->line].line_str.size() !=
        cursor_->byte_offset) {
        at_cursor = frame_.buffer_->lines()[cursor_->line]
                        .line_str[cursor_->byte_offset];
    }
    if (str == kLeftBrace) {
        if (at_cursor == kRightBraceChar) {
            goto out;
        }
        str += kRightBrace;
    } else if (str == kLeftParenthesis) {
        if (at_cursor == kRightParenthesisChar) {
            goto out;
        }
        str += kRightParenthesis;
    } else if (str == kLeftSquareBracket) {
        if (at_cursor == kRightSquareBracketChar) {
            goto out;
        }
        str += kRightSquareBracket;
    } else if (str == kSingleQuote) {
        if (at_cursor == kSingleQuoteChar) {
            goto out;
        }
        str += kSingleQuote;
    } else if (str == kDoubleQuote) {
        if (at_cursor == kDoubleQuoteChar) {
            goto out;
        }
        str += kDoubleQuote;
    }
out:
    Pos pos = {cursor_->line, cursor_->byte_offset + 1};
    frame_.AddStringAtCursor(std::move(str), &pos);
}

void Window::TryAutoIndent() {
    const std::string& line = frame_.buffer_->lines()[cursor_->line].line_str;
    std::string indent = "";
    std::string str = kNewLine;
    // keep with this line's indent
    size_t i = 0;
    size_t cur_indent = 0;
    for (; i < line.size(); i++) {
        if (line[i] == kSpaceChar) {
            cur_indent++;
            indent.push_back(line[i]);
        } else if (line[i] == kTabChar) {
            cur_indent =
                (cur_indent / options_->tabstop + 1) * options_->tabstop;
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
    if (ft == "c" || ft == "cpp" || ft == "java") {  // TODO: more languages
        // traditional languages, try to check () {} []
        // e.g.
        // {<cursor>}
        // ->
        // {
        // <indent><cursor>
        // }
        for (int64_t i = cursor_->byte_offset - 1; i >= 0; i--) {
            char want_right;
            if (line[i] == kLeftBraceChar) {
                want_right = kRightBraceChar;
            } else if (line[i] == kLeftParenthesisChar) {
                want_right = kRightParenthesisChar;
            } else if (line[i] == kLeftSquareBracketChar) {
                want_right = kRightSquareBracketChar;
            } else if (line[i] == kSpaceChar) {
                continue;
            } else {
                break;
            }

            if (options_->tabspace) {
                int need_space =
                    (cur_indent / options_->tabstop + 1) * options_->tabstop -
                    cur_indent;
                str += std::string(need_space, kSpaceChar);
            } else {
                str += kTab;
            }
            for (i = cursor_->byte_offset;
                 i < static_cast<int64_t>(line.size()); i++) {
                if (line[i] == want_right) {
                    cursor_pos.line = cursor_->line + 1;
                    cursor_pos.byte_offset = str.size() - 1;
                    maunally_set_cursor_pos = true;
                    str += kNewLine + indent;
                    break;
                } else if (line[i] != kSpaceChar) {
                    break;
                }
            }
            break;
        }
    }
    if (maunally_set_cursor_pos) {
        frame_.AddStringAtCursor(std::move(str), &cursor_pos);
    } else {
        frame_.AddStringAtCursor(std::move(str));
    }
}

void Window::TabAtCursor() { frame_.TabAtCursor(); }

void Window::Redo() { frame_.Redo(); }
void Window::Undo() { frame_.Undo(); }

void Window::NextBuffer() {
    if (frame_.buffer_->IsLastBuffer()) {
        return;
    }
    Buffer* next = frame_.buffer_->next_;
    DetachBuffer();
    frame_.buffer_ = next;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::PrevBuffer() {
    if (frame_.buffer_->IsFirstBuffer()) {
        return;
    }
    Buffer* prev = frame_.buffer_->prev_;
    DetachBuffer();
    frame_.buffer_ = prev;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::AttachBuffer(Buffer* buffer) {
    if (frame_.buffer_) {
        DetachBuffer();
    }
    frame_.buffer_ = buffer;
    frame_.buffer_->RestoreCursorState(*cursor_);
}

void Window::DetachBuffer() {
    if (frame_.buffer_) {
        frame_.buffer_->SaveCursorState(*cursor_);
        frame_.buffer_ = nullptr;
        DestorySearchContext();
    }
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
}

Window::SearchState Window::CursorGoNextSearchResult() {
    if (search_buffer_version_ == -1) {
        return {};
    }

    if (frame_.buffer_->version() != search_buffer_version_) {
        // The buffer has changed, we do search again;
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
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

    if (frame_.buffer_->version() != search_buffer_version_) {
        // The buffer has changed, we do search again;
        search_result_ = frame_.buffer_->Search(search_pattern_);
        search_buffer_version_ = frame_.buffer_->version();
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

int64_t Window::AllocId() noexcept { return cur_window_id_++; }

int64_t Window::cur_window_id_ = 0;

}  // namespace mango