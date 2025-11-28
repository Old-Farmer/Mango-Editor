#include "frame.h"

#include <stdint.h>

#include <gsl/util>

#include "buffer.h"
#include "character.h"
#include "clipboard.h"
#include "constants.h"
#include "cursor.h"
#include "draw.h"
#include "options.h"
#include "syntax.h"

namespace mango {

Frame::Frame(Buffer* buffer, Cursor* cursor, Opts* opts, SyntaxParser* parser,
             ClipBoard* clipboard) noexcept
    : buffer_(buffer),
      cursor_(cursor),
      clipboard_(clipboard),
      parser_(parser),
      opts_(opts) {}

void Frame::Draw() {
    MGO_ASSERT(buffer_ != nullptr);
    if (!buffer_->IsLoad()) {
        return;
    }

    size_t sidebar_width = SidebarWidth();
    if (sidebar_width >= width_) {
        return;
    }

    size_t s_col_for_file_content = col_ + sidebar_width;
    size_t width_for_file_content = width_ - sidebar_width;

    auto scheme = GetOpt<ColorScheme>(kOptColorScheme);
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    // Prepare highlights
    std::vector<const std::vector<Highlight>*> highlights;
    std::vector<Highlight> selection_hl;
    if (selection_.active) {
        selection_hl.resize(1);
        selection_hl[0].range = selection_.ToRange();
        selection_hl[0].attr = scheme[kSelection];
        if (cursor_->line == selection_hl[0].range.begin.line &&
            cursor_->byte_offset == selection_hl[0].range.begin.byte_offset) {
            // TODO: maybe all ready end of line, ++ will out of line, currently
            // no problem
            selection_hl[0].range.begin.byte_offset++;
        }
        selection_hl.push_back({selection_.ToRange(), scheme[kSelection]});
        highlights.push_back(&selection_hl);
    }
    if (parser_) {
        Range render_range;
        if (wrap_) {
            MGO_ASSERT(false);
        } else {
            size_t last_line =
                std::min(b_view_line_ + height_ - 1, buffer_->LineCnt() - 1);
            render_range = {{b_view_line_, 0},
                            {last_line, buffer_->GetLine(last_line).size()}};
        }
        auto syntax_context = parser_->GetBufferSyntaxContext(
            buffer_, render_range);  // render range is larger than the real
                                     // render range, but it's ok.
        if (syntax_context) {
            highlights.push_back(&syntax_context->syntax_highlight);
        }
    }

    if (wrap_) {
        // TODO: wrap the content
        MGO_ASSERT(false);
    } else {
        const size_t line_cnt = buffer_->LineCnt();
        for (size_t win_r = 0; win_r < height_; win_r++) {
            int screen_r = win_r + row_;
            size_t b_view_r = win_r + b_view_line_;

            if (line_cnt <= b_view_r) {
                Codepoint codepoint = '~';
                term_->SetCell(s_col_for_file_content, screen_r, &codepoint, 1,
                               scheme[kNormal]);
                continue;
            }
            DrawSidebar(screen_r, b_view_r, sidebar_width);
            DrawLine(*term_, buffer_->GetLine(b_view_r), {b_view_r, 0},
                     b_view_col_, width_for_file_content, screen_r,
                     s_col_for_file_content, &highlights, scheme[kNormal],
                     tabstop);
        }
    }
}

bool Frame::In(size_t s_col, size_t s_row) {
    return s_col >= col_ && s_col < s_col + width_ && s_row >= row_ &&
           s_row < row_ + height_;
}

void Frame::MakeCursorVisible() {
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    // Calculate the cursor pos if we put the buffer from (0, 0)
    size_t row = cursor_->line;

    const std::string& cur_line = buffer_->GetLine(cursor_->line);
    Character character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    cursor_->character_in_line = 0;
    while (offset < cursor_->byte_offset) {
        int byte_len;
        Result res = ThisCharacter(cur_line, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        offset += byte_len;
        int character_width = character.Width();
        if (character_width <= 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                character_width = tabstop - cur_b_view_c % tabstop;
            } else {
                character_width = kReplacementCharWidth;
            }
        }
        cur_b_view_c += character_width;
        cursor_->character_in_line++;
    }

    // some opretions makes want change, reset it
    if (!cursor_->b_view_col_want.has_value()) {
        cursor_->b_view_col_want = cur_b_view_c;
    }

    size_t s_col_for_file_content = col_ + SidebarWidth();
    size_t width_for_file_content = width_ - (s_col_for_file_content - col_);

    // adjust col of view
    if (cur_b_view_c < b_view_col_) {
        b_view_col_ = cur_b_view_c;
    } else if (cur_b_view_c - b_view_col_ >= width_for_file_content) {
        b_view_col_ = cur_b_view_c + 2 - width_;
    }

    // adjust row of view
    if (row < b_view_line_) {
        b_view_line_ = row;
    } else if (row - b_view_line_ >= height_) {
        b_view_line_ = row + 1 - height_;
    }

    cursor_->s_row_last = cursor_->s_row;
    cursor_->s_col_last = cursor_->s_col;
    cursor_->s_row = row - b_view_line_ + row_;
    cursor_->s_col = cur_b_view_c - b_view_col_ + s_col_for_file_content;
}

size_t Frame::SetCursorByBViewCol(size_t b_view_col) {
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    size_t cur_b_view_line = cursor_->line;
    size_t target_b_view_col = b_view_col;
    const std::string& cur_line = buffer_->GetLine(cur_b_view_line);
    Character character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    while (offset < cur_line.size()) {
        int byte_len;
        Result res = ThisCharacter(cur_line, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        int character_width = character.Width();
        if (character_width <= 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                character_width = tabstop - cur_b_view_c % tabstop;
            } else {
                character_width = kReplacementCharWidth;
            }
        }
        if (cur_b_view_c <= target_b_view_col &&
            target_b_view_col < cur_b_view_c + character_width) {
            cursor_->line = cur_b_view_line;
            cursor_->byte_offset = offset;
            return cur_b_view_c;
        }
        offset += byte_len;
        cur_b_view_c += character_width;
    }
    cursor_->byte_offset = offset;
    return cur_b_view_c;
}

void Frame::SetCursorHint(size_t s_row, size_t s_col) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(In(s_col, s_row));

    size_t cur_b_view_row = s_row - row_ + b_view_line_;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->LineCnt()) {
        cursor_->line = buffer_->LineCnt() - 1;
        cursor_->byte_offset = buffer_->GetLine(buffer_->LineCnt() - 1).size();
        cursor_->DontHoldColWant();
        return;
    }

    cursor_->line = cur_b_view_row;

    // Click at line number columns.
    size_t line_number_width = SidebarWidth();
    if (s_col - col_ < line_number_width) {
        return;
    }

    // Search througn line
    size_t target_b_view_col = s_col - (col_ + line_number_width) + b_view_col_;
    SetCursorByBViewCol(target_b_view_col);
    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void Frame::ScrollRows(int64_t count, bool cursor_in_frame) {
    MGO_ASSERT(buffer_);
    if (count > 0) {
        b_view_line_ = std::min(b_view_line_ + count, buffer_->LineCnt() - 1);
    } else {
        b_view_line_ =
            std::max<int64_t>(static_cast<int64_t>(b_view_line_) + count,
                              0);  // cast is necessary here
    }

    if (!cursor_in_frame) {
        return;
    }

    if (cursor_->line < b_view_line_) {
        cursor_->line = b_view_line_;
    } else if (cursor_->line >= b_view_line_ + height_) {
        cursor_->line = b_view_line_ + height_ - 1;
    }
    MGO_ASSERT(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
    SelectionFollowCursor();
}

void Frame::ScrollCols(int64_t count) { (void)count; }

void Frame::CursorGoRight() {
    MGO_ASSERT(buffer_);
    auto _ = gsl::finally([this] { cursor_->DontHoldColWant(); });

    // end
    if (buffer_->GetLine(cursor_->line).size() == cursor_->byte_offset) {
        return;
    }

    Character charater;
    int len;
    Result ret = ThisCharacter(buffer_->GetLine(cursor_->line),
                               cursor_->byte_offset, charater, len);
    MGO_ASSERT(ret == kOk);
    cursor_->byte_offset += len;
    SelectionFollowCursor();
}

void Frame::CursorGoLeft() {
    MGO_ASSERT(buffer_);
    auto _ = gsl::finally([this] { cursor_->DontHoldColWant(); });

    // home
    if (cursor_->byte_offset == 0) {
        return;
    }

    Character charater;
    int len;
    Result ret = PrevCharacter(buffer_->GetLine(cursor_->line),
                               cursor_->byte_offset, charater, len);
    MGO_ASSERT(ret == kOk);
    cursor_->byte_offset -= len;

    SelectionFollowCursor();
}

void Frame::CursorGoUp() {
    MGO_ASSERT(buffer_);

    // first line
    if (cursor_->line == 0) {
        return;
    }

    cursor_->line--;
    MGO_ASSERT(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
    SelectionFollowCursor();
}

void Frame::CursorGoDown() {
    MGO_ASSERT(buffer_);

    // last line
    if (buffer_->LineCnt() - 1 == cursor_->line) {
        return;
    }

    cursor_->line++;
    MGO_ASSERT(cursor_->b_view_col_want.has_value());
    SetCursorByBViewCol(cursor_->b_view_col_want.value());
    SelectionFollowCursor();
}

void Frame::CursorGoHome() {
    MGO_ASSERT(buffer_);
    cursor_->byte_offset = 0;
    cursor_->DontHoldColWant();
    SelectionFollowCursor();
}

void Frame::CursorGoEnd() {
    MGO_ASSERT(buffer_);
    cursor_->byte_offset = buffer_->GetLine(cursor_->line).size();
    cursor_->DontHoldColWant();
    SelectionFollowCursor();
}

void Frame::CursorGoWordEnd(bool one_more_character) {
    MGO_ASSERT(buffer_);
    const std::string* cur_line = &buffer_->GetLine(cursor_->line);
    if (cursor_->byte_offset == cur_line->size()) {
        if (cursor_->line == buffer_->LineCnt() - 1) {
            return;
        }
        cursor_->line++;
        cursor_->byte_offset = 0;
        cur_line = &buffer_->GetLine(cursor_->line);
    }
    Result res = WordEnd(*cur_line, cursor_->byte_offset, one_more_character,
                         cursor_->byte_offset);
    MGO_ASSERT(res == kOk);
    cursor_->DontHoldColWant();
    SelectionFollowCursor();
}

void Frame::CursorGoWordBegin() {
    MGO_ASSERT(buffer_);
    const std::string* cur_line = &buffer_->GetLine(cursor_->line);
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }
        cursor_->line--;
        cur_line = &buffer_->GetLine(cursor_->line);
        cursor_->byte_offset = cur_line->size();
    }
    Result res =
        WordBegin(*cur_line, cursor_->byte_offset, cursor_->byte_offset);
    MGO_ASSERT(res == kOk || res == kNotExist);
    cursor_->DontHoldColWant();
    SelectionFollowCursor();
}

void Frame::SelectAll() {
    selection_.active = true;
    selection_.anchor = {0, 0};
    selection_.head = {buffer_->LineCnt() - 1,
                       buffer_->GetLine(buffer_->LineCnt() - 1).size()};
    cursor_->SetPos(selection_.head);
    cursor_->DontHoldColWant();
}

void Frame::DeleteAtCursor() {
    if (selection_.active) {
        DeleteSelection();
    } else {
        DeleteCharacterBeforeCursor();
    }
}

void Frame::DeleteWordBeforeCursor() {
    MGO_ASSERT(!selection_.active);

    MGO_ASSERT(buffer_);
    Pos deleted_until;
    const std::string* cur_line = &buffer_->GetLine(cursor_->line);
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }
        deleted_until.line = cursor_->line - 1;
        cur_line = &buffer_->GetLine(deleted_until.line);
        deleted_until.byte_offset = cur_line->size();
    } else {
        deleted_until = cursor_->ToPos();
        Result res = WordBegin(*cur_line, deleted_until.byte_offset,
                               deleted_until.byte_offset);
        MGO_ASSERT(res == kOk || res == kNotExist);
    }
    Pos pos;
    if (buffer_->Delete({deleted_until, {cursor_->line, cursor_->byte_offset}},
                        nullptr, pos) != kOk) {
        return;
    }
    AfterModify(pos);
}

void Frame::AddStringAtCursor(std::string str, const Pos* cursor_pos) {
    if (selection_.active) {
        ReplaceSelection(std::move(str), cursor_pos);
    } else {
        AddStringAtCursorNoSelection(std::move(str), cursor_pos);
    }
}

Result Frame::Replace(const Range& range, std::string str,
                      const Pos* cursor_pos) {
    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    Pos cur_cursor_pos = {cursor_->line, cursor_->byte_offset};
    Result res = buffer_->Replace(range, std::move(str), &cur_cursor_pos,
                                  cursor_pos != nullptr, pos);
    if (res != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

void Frame::TabAtCursor() {
    // TODO: support tab when selection
    selection_.active = false;

    if (!GetOpt<bool>(kOptTabSpace)) {
        AddStringAtCursor("\t");
        return;
    }

    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    int64_t cur_b_view_row = cursor_->line;
    const std::string& cur_line = buffer_->GetLine(cur_b_view_row);
    Character character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    while (offset < cursor_->byte_offset) {
        int byte_len;
        Result res = ThisCharacter(cur_line, offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        int character_width = character.Width();
        if (character_width <= 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                character_width = tabstop - cur_b_view_c % tabstop;
            } else {
                character_width = kReplacementCharWidth;
            }
        }
        offset += byte_len;
        cur_b_view_c += character_width;
    }
    int need_space = tabstop - cur_b_view_c % tabstop;
    AddStringAtCursor(std::string(need_space, kSpaceChar));
}

void Frame::Redo() {
    selection_.active = false;

    Pos pos;
    if (buffer_->Redo(pos) != kOk) {
        return;
    }
    cursor_->SetPos(pos);
    cursor_->DontHoldColWant();
    UpdateSyntax();
}

void Frame::Undo() {
    selection_.active = false;

    Pos pos;
    if (buffer_->Undo(pos) != kOk) {
        return;
    }
    cursor_->SetPos(pos);
    cursor_->DontHoldColWant();
    UpdateSyntax();
}

void Frame::Copy() {
    if (selection_.active) {
        Range range = selection_.ToRange();
        clipboard_->SetContent(buffer_->GetContent(range), false);
        SelectionCancell();
    } else {
        Range range = {{cursor_->line, 0},
                       {cursor_->line, buffer_->GetLine(cursor_->line).size()}};
        clipboard_->SetContent("\n" + buffer_->GetContent(range), true);
    }
}

void Frame::Paste() {
    bool lines;
    std::string content = clipboard_->GetContent(lines);
    if (content.empty()) {
        // Nothing in clipboard, do nothing at all.
        // TODO: when error codition occur,
        // content will also be empty, maybe a notification mechansim?
        return;
    }
    if (selection_.active) {
        ReplaceSelection(std::move(content));
    } else {
        Pos pos;
        if (lines) {
            Pos cursor_pos = cursor_->ToPos();
            buffer_->Add(
                {cursor_->line, buffer_->GetLine(cursor_->line).size()},
                std::move(content), &cursor_pos, false, pos);
        } else {
            buffer_->Add({cursor_->line, cursor_->byte_offset},
                         std::move(content), nullptr, false, pos);
        }
        AfterModify(pos);
    }
}

void Frame::Cut() {
    if (selection_.active) {
        Range range = selection_.ToRange();
        clipboard_->SetContent(buffer_->GetContent(range), false);
        DeleteSelection();
    } else {
        Range range = {{cursor_->line, 0},
                       {cursor_->line, buffer_->GetLine(cursor_->line).size()}};
        clipboard_->SetContent("\n" + buffer_->GetContent(range), true);

        // We try to delete a line where cursor is located.
        Pos pos;
        auto cur_pos = cursor_->ToPos();
        if (buffer_->LineCnt() == 1) {
            return;
        }

        if (cursor_->line == buffer_->LineCnt() - 1) {
            range.begin.line--;
            range.begin.byte_offset = buffer_->GetLine(range.begin.line).size();
        } else {
            range.end.line++;
            range.end.byte_offset = 0;
        }
        buffer_->Delete(range, &cur_pos, pos);
        AfterModify(pos);
    }
}

void Frame::DeleteCharacterBeforeCursor() {
    Range range;
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return;
        }
        range = {
            {cursor_->line - 1, buffer_->GetLine(cursor_->line - 1).size()},
            {cursor_->line, 0}};
    } else {
        Character charater;
        int len;
        Result ret = PrevCharacter(buffer_->GetLine(cursor_->line),
                                   cursor_->byte_offset, charater, len);
        MGO_ASSERT(ret == kOk);
        range = {{cursor_->line, cursor_->byte_offset - len},
                 {cursor_->line, cursor_->byte_offset}};
    }
    Pos pos;
    if (buffer_->Delete(range, nullptr, pos) != kOk) {
        return;
    }
    AfterModify(pos);
}
void Frame::DeleteSelection() {
    Pos pos;
    Pos cursor_pos = {cursor_->line, cursor_->byte_offset};
    if (buffer_->Delete(selection_.ToRange(), &cursor_pos, pos) != kOk) {
        return;
    }
    AfterModify(pos);
    SelectionCancell();
}

void Frame::AddStringAtCursorNoSelection(std::string str,
                                         const Pos* cursor_pos) {
    MGO_ASSERT(!selection_.active);

    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    if (buffer_->Add({cursor_->line, cursor_->byte_offset}, std::move(str),
                     nullptr, cursor_pos != nullptr, pos) != kOk) {
        return;
    }
    AfterModify(pos);
}

void Frame::ReplaceSelection(std::string str, const Pos* cursor_pos) {
    if (Replace(selection_.ToRange(), std::move(str), cursor_pos) != kOk) {
        return;
    }
    SelectionCancell();
}

size_t Frame::SidebarWidth() {
    auto line_number = GetOpt<int64_t>(kOptLineNumber);
    if (line_number == static_cast<int64_t>(LineNumberType::kNone)) {
        return 0;
    }

    // Now we only have line number.
    // We calc width according to the line cnt of the buffer to avoid ui
    // debounce.
    size_t max_line_number = buffer_->LineCnt() + 1;
    return NumberWidth(max_line_number) + 3;  // two spaces left and 1 right.
    return 0;
}

void Frame::DrawSidebar(int screen_row, size_t absolute_line, size_t width) {
    auto line_number_type =
        static_cast<LineNumberType>(GetOpt<int64_t>(kOptLineNumber));
    if (line_number_type == LineNumberType::kNone) {
        return;
    }

    char sidebar_buf[kMaxSizeTWidth + 3 + 1];

    auto scheme = GetOpt<ColorScheme>(kOptColorScheme);
    size_t line_number;
    if (line_number_type == LineNumberType::kAboslute) {
        line_number = absolute_line + 1;
    } else if (line_number_type == LineNumberType::kRelative) {
        line_number = absolute_line == cursor_->line
                               ? absolute_line + 1
                               : (absolute_line > cursor_->line
                                      ? absolute_line - cursor_->line
                                      : cursor_->line - absolute_line);
    } else {
        // Make compiler happy
        MGO_ASSERT(false);
        line_number = 0;
    }
    std::string line_number_str = std::to_string(line_number);
    size_t left_space = width - 1 - line_number_str.size();
    memset(sidebar_buf, kSpaceChar, left_space);
    memcpy(sidebar_buf + left_space, line_number_str.data(),
           line_number_str.size());
    sidebar_buf[left_space + line_number_str.size()] = kSpaceChar;
    sidebar_buf[left_space + line_number_str.size() + 1] = '\0';
    term_->Print(col_, screen_row, scheme[kLineNumber], sidebar_buf);
}

void Frame::UpdateSyntax() {
    if (parser_) parser_->ParseSyntaxAfterEdit(buffer_);
}

void Frame::SelectionFollowCursor() {
    if (selection_.active) {
        selection_.head = cursor_->ToPos();
    }
}

void Frame::AfterModify(const Pos& cursor_pos) {
    cursor_->SetPos(cursor_pos);
    cursor_->DontHoldColWant();
    UpdateSyntax();
}

}  // namespace mango