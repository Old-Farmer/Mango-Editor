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

static constexpr std::string_view kSublineIndicator = "<<<";

Frame::Frame(Cursor* cursor, Opts* opts, SyntaxParser* parser,
             ClipBoard* clipboard) noexcept
    : cursor_(cursor), clipboard_(clipboard), parser_(parser), opts_(opts) {}

void Frame::Draw() {
    MGO_ASSERT(buffer_ != nullptr);
    if (!buffer_->IsLoad()) {
        return;
    }

    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    size_t content_s_col = col_ + sidebar_width;
    size_t content_width = width_ - sidebar_width;

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
        if (!GetOpt<bool>(kOptWrap)) {
            size_t last_line =
                std::min(b_view_->line + height_ - 1, buffer_->LineCnt() - 1);
            // If not wrap, render range is larger than the real render range,
            // but it's ok.
            render_range = {{b_view_->line, 0},
                            {last_line, buffer_->GetLine(last_line).size()}};
        } else {
            render_range = CalcWrapRange(content_width);
        }
        auto syntax_context =
            parser_->GetBufferSyntaxContext(buffer_, render_range);
        if (syntax_context) {
            highlights.push_back(&syntax_context->syntax_highlight);
        }
    }

    if (GetOpt<bool>(kOptWrap)) {
        // An empty sidebar
        char sidebar_buf[kMaxSizeTWidth + 3 + 1];
        memset(sidebar_buf, kSpaceChar, sidebar_width);
        sidebar_buf[sidebar_width] = '\0';

        size_t line = b_view_->line;
        size_t byte_offset = 0;

        // First, we skip some sublines.
        for (size_t i = 0; i < b_view_->subline; i++) {
            byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                      content_width, tabstop, true);
        }

        MGO_ASSERT(line < buffer_->LineCnt());
        for (size_t i = 0; i < height_; i++) {
            if (line >= buffer_->LineCnt()) {
                break;
            }
            if (byte_offset == 0) {
                DrawSidebar(row_ + i, line, sidebar_width);
            } else {
                if (i == 0) {
                    // If first row is a subline, we draw a <<< at the sidebar
                    std::string buf(std::string(sidebar_width - 1 -
                                                    kSublineIndicator.size(),
                                                kSpaceChar) +
                                    std::string(kSublineIndicator) + kSpace);
                    term_->Print(0, row_ + i, scheme[kLineNumber], buf.data());
                } else {
                    term_->Print(0, row_ + i, scheme[kLineNumber], sidebar_buf);
                }
            }
            byte_offset =
                DrawLine(*term_, buffer_->GetLine(line), {line, byte_offset}, 0,
                         content_width, i + row_, content_s_col, &highlights,
                         scheme[kNormal], tabstop, true);
            if (byte_offset == buffer_->GetLine(line).size()) {
                line++;
                byte_offset = 0;
            }
        }
    } else {
        const size_t line_cnt = buffer_->LineCnt();
        for (size_t win_r = 0; win_r < height_; win_r++) {
            int cur_s_row = win_r + row_;
            size_t line = win_r + b_view_->line;

            if (line_cnt <= line) {
                Codepoint codepoint = '~';
                term_->SetCell(content_s_col, cur_s_row, &codepoint, 1,
                               scheme[kNormal]);
                continue;
            }
            DrawSidebar(cur_s_row, line, sidebar_width);
            DrawLine(*term_, buffer_->GetLine(line), {line, 0}, b_view_->col,
                     content_width, cur_s_row, content_s_col, &highlights,
                     scheme[kNormal], tabstop, false);
        }
    }
}

bool Frame::In(size_t s_col, size_t s_row) {
    return s_col >= col_ && s_col < s_col + width_ && s_row >= row_ &&
           s_row < row_ + height_;
}

// cursor is before the first row.
// we just put that cursor on the first row of screen.
// TODO: scrolloff?
void Frame::MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(
    size_t content_width) {
    b_view_->line = cursor_->line;

    size_t byte_offset = 0;
    size_t subline = 0;
    size_t end_b_view_col;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        byte_offset = ArrangeLine(
            buffer_->GetLine(b_view_->line), byte_offset, 0, content_width,
            GetOpt<int64_t>(kOptTabStop), true, &end_b_view_col,
            &cursor_->byte_offset, &stop_at_target, &character_cnt);
        cursor_->character_in_line += character_cnt;
        if (stop_at_target) {
            cursor_->SetScreenPos(end_b_view_col + width_ - content_width,
                                  subline);
            b_view_->subline = subline;
            if (!cursor_->b_view_col_want.has_value()) {
                cursor_->b_view_col_want = end_b_view_col;
            }
            return;
        }
        subline++;
    }
}

void Frame::MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(
    size_t content_width) {
    // Which subline of the line the cursor in?
    size_t byte_offset = 0;
    size_t subline = 0;
    size_t end_view_col;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        byte_offset = ArrangeLine(
            buffer_->GetLine(cursor_->line), byte_offset, 0, content_width,
            GetOpt<int64_t>(kOptTabStop), true, &end_view_col,
            &cursor_->byte_offset, &stop_at_target, &character_cnt);
        cursor_->character_in_line += character_cnt;
        if (stop_at_target) {
            cursor_->SetScreenPos(end_view_col + width_ - content_width,
                                  row_ + height_ - 1);
            if (!cursor_->b_view_col_want.has_value()) {
                cursor_->b_view_col_want = end_view_col;
            }
            break;
        }
        subline++;
    }
    size_t row_cnt_before_cursor_line = height_ - (subline + 1);
    // Search backward to set the start.
    size_t line = cursor_->line - 1;
    // We don't need to check line isn't 0, because there must be a screen range
    // of text before us.
    while (true) {
        size_t row_cnt = ScreenRows(buffer_->GetLine(line), content_width,
                                    GetOpt<int64_t>(kOptTabStop));
        if (row_cnt < row_cnt_before_cursor_line) {
            row_cnt_before_cursor_line -= row_cnt;
            line--;
            continue;
        }

        b_view_->line = line;
        b_view_->subline = row_cnt - row_cnt_before_cursor_line;
        return;
    }
}

void Frame::MakeCursorVisibleWrap() {
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    size_t content_width = width_ - sidebar_width;
    int tabstop = GetOpt<int64_t>(kOptTabStop);

    size_t line = b_view_->line;
    size_t byte_offset = 0;

    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                  content_width, tabstop, true);
    }

    cursor_->character_in_line = 0;
    if (cursor_->ToPos() < Pos{line, byte_offset}) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(content_width);
        return;
    }

    // We walk through the render range.
    for (size_t i = 0; i < height_; i++) {
        if (line >= buffer_->LineCnt()) {
            break;
        }
        if (line == cursor_->line) {
            size_t end_view_col;
            bool stop_at_cursor;
            size_t character_cnt;
            byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                      content_width, tabstop, true,
                                      &end_view_col, &cursor_->byte_offset,
                                      &stop_at_cursor, &character_cnt);
            cursor_->character_in_line += character_cnt;
            // We find the cursor's location.
            if (stop_at_cursor) {
                // The cursor is in the screen, just return.
                cursor_->SetScreenPos(end_view_col + width_ - content_width,
                                      row_ + i);
                if (!cursor_->b_view_col_want.has_value()) {
                    cursor_->b_view_col_want = end_view_col;
                }
                return;
            }
        } else {
            byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                      content_width, tabstop, true);
        }
        if (byte_offset == buffer_->GetLine(line).size()) {
            line++;
            byte_offset = 0;
        }
    }

    if (!b_view_->make_cursor_visible) {
        cursor_->SetScreenPos(-1, -1);
        return;
    }

    // The cursor is after the render range.
    MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(content_width);
}

void Frame::MakeCursorVisibleNotWrap() {
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

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

    size_t content_s_col = col_ + sidebar_width;
    size_t content_width = width_ - sidebar_width;

    // adjust col of view
    if (cur_b_view_c < b_view_->col) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        b_view_->col = cur_b_view_c;
    } else if (cur_b_view_c - b_view_->col >= content_width) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        b_view_->col = cur_b_view_c + 2 - width_;
    }

    // adjust row of view
    if (row < b_view_->line) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        b_view_->line = row;
    } else if (row - b_view_->line >= height_) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        b_view_->line = row + 1 - height_;
    }

    cursor_->SetScreenPos(cur_b_view_c - b_view_->col + content_s_col,
                          row - b_view_->line + row_);
}

void Frame::MakeCursorVisible() {
    // MakeSureViewValid(); // call it outside
    if (GetOpt<bool>(kOptWrap)) {
        MakeCursorVisibleWrap();
    } else {
        MakeCursorVisibleNotWrap();
    }
}

void Frame::MakeSureViewValid() {
    if (GetOpt<bool>(kOptWrap)) {
        if (b_view_->line >= buffer_->LineCnt()) {
            b_view_->line = buffer_->LineCnt() - 1;
            b_view_->subline = ScreenRows(buffer_->GetLine(b_view_->line),
                                          width_ - SidebarWidth(),
                                          GetOpt<int64_t>(kOptTabStop)) -
                               1;
        } else {
            if (b_view_->subline == 0) {
                return;
            }
            b_view_->subline =
                std::min(ScreenRows(buffer_->GetLine(b_view_->line),
                                    width_ - SidebarWidth(),
                                    GetOpt<int64_t>(kOptTabStop)),
                         b_view_->subline);
        }
    } else {
        b_view_->line = std::min(buffer_->LineCnt() - 1, b_view_->line);
    }
}

void Frame::MakeSureBColViewWantReady(CursorState& state) {
    if (state.b_view_col_want.has_value()) {
        return;
    }

    size_t content_width = width_ - SidebarWidth();
    MGO_ASSERT(state.line < buffer_->LineCnt());
    if (GetOpt<bool>(kOptWrap)) {
        size_t byte_offset = 0;
        while (true) {
            bool stop;
            size_t b_view_col;
            byte_offset =
                ArrangeLine(buffer_->GetLine(state.line), byte_offset, 0,
                            content_width, GetOpt<int64_t>(kOptTabStop), false,
                            &b_view_col, &state.byte_offset, &stop);
            if (stop) {
                state.b_view_col_want = b_view_col;
                break;
            }
        }
    } else {
        size_t b_view_col;
        ArrangeLine(buffer_->GetLine(state.line), 0, 0, content_width,
                    GetOpt<int64_t>(kOptTabStop), false, &b_view_col,
                    &state.byte_offset);
        state.b_view_col_want = b_view_col;
    }
}

size_t Frame::CalcByteOffsetByBViewCol(std::string_view line,
                                       size_t b_view_col_from_byte_offset,
                                       size_t byte_offset, size_t content_width,
                                       bool wrap) {
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    size_t target_b_view_col = b_view_col_from_byte_offset;
    Character character;
    size_t cur_b_view_col = 0;
    size_t cur_byte_offset = byte_offset;
    while (cur_byte_offset < line.size()) {
        int byte_len;
        Result res = ThisCharacter(line, cur_byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        int character_width = character.Width();
        if (character_width <= 0) {
            char c;
            if (character.Ascii(c) && c == '\t') {
                character_width = tabstop - cur_b_view_col % tabstop;
            } else {
                character_width = kReplacementCharWidth;
            }
        }
        if (cur_b_view_col + character_width <= content_width) {
            if (wrap && cur_byte_offset + byte_len == line.size() &&
                cur_b_view_col + character_width == content_width) {
                break;
            }
            if (cur_b_view_col <= target_b_view_col &&
                target_b_view_col < cur_b_view_col + character_width) {
                return cur_byte_offset;
            }
        } else {
            break;
        }
        cur_byte_offset += byte_len;
        cur_b_view_col += character_width;
    }
    return cur_byte_offset;
}

void Frame::SetCursorHintNoWrap(size_t s_row, size_t s_col,
                                size_t sidebar_width) {
    size_t cur_b_view_row = s_row - row_ + b_view_->line;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->LineCnt()) {
        cursor_->line = buffer_->LineCnt() - 1;
        cursor_->byte_offset = buffer_->GetLine(buffer_->LineCnt() - 1).size();
        cursor_->DontHoldColWant();
        return;
    }

    cursor_->line = cur_b_view_row;

    // Search througn line
    size_t target_b_view_col = s_col - (col_ + sidebar_width) + b_view_->col;
    cursor_->byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(cursor_->line), target_b_view_col, 0,
        width_ - SidebarWidth(), false);
    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void Frame::SetCursorHintWrap(size_t s_row, size_t s_col,
                              size_t sidebar_width) {
    size_t line = b_view_->line;
    size_t byte_offset = 0;
    // to the screen row where hint is.

    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                  width_ - sidebar_width,
                                  GetOpt<int64_t>(kOptTabStop), true);
    }
    size_t cur_screen_row = row_;
    for (; cur_screen_row < s_row; cur_screen_row++) {
        if (line >= buffer_->LineCnt()) {
            break;
        }
        byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                  width_ - sidebar_width,
                                  GetOpt<int64_t>(kOptTabStop), true);
        if (byte_offset == buffer_->GetLine(line).size()) {
            line++;
            byte_offset = 0;
        }
    }
    if (line >= buffer_->LineCnt()) {
        // Locate in last line end.
        cursor_->line = buffer_->LineCnt() - 1;
        cursor_->byte_offset = buffer_->GetLine(buffer_->LineCnt() - 1).size();
        cursor_->DontHoldColWant();
        return;
    }

    // Search througn line after byte_offset
    cursor_->line = line;
    cursor_->byte_offset =
        CalcByteOffsetByBViewCol(buffer_->GetLine(line), s_col - sidebar_width,
                                 byte_offset, width_ - sidebar_width, true);
    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void Frame::SetCursorHint(size_t s_row, size_t s_col) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(In(s_col, s_row));
    b_view_->make_cursor_visible = true;

    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    // Click at sidebar columns.
    if (s_col - col_ < sidebar_width) {
        return;
    }

    MakeSureViewValid();
    if (GetOpt<bool>(kOptWrap)) {
        SetCursorHintWrap(s_row, s_col, sidebar_width);
    } else {
        SetCursorHintNoWrap(s_row, s_col, sidebar_width);
    }
}

void Frame::ScrollRowsWrap(int64_t count, size_t content_width) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    if (count > 0) {
        while (count > 0) {
            size_t row_cnt = ScreenRows(buffer_->GetLine(b_view_->line),
                                        content_width, tabstop);
            if (row_cnt - b_view_->subline <= static_cast<size_t>(count)) {
                if (b_view_->line == buffer_->LineCnt() - 1) {
                    b_view_->subline = row_cnt - 1;
                    return;
                }
                count -= (row_cnt - b_view_->subline);
                b_view_->line++;
                b_view_->subline = 0;
            } else {
                // >
                b_view_->subline += count;
                count = 0;
            }
        }
    } else {
        count = -count;
        while (count > 0) {
            if (b_view_->subline == 0) {
                if (b_view_->line == 0) {
                    return;
                }
                b_view_->line--;
                size_t row_cnt = ScreenRows(buffer_->GetLine(b_view_->line),
                                            content_width, tabstop);
                b_view_->subline = row_cnt - 1;
                count -= 1;
            } else {
                if (b_view_->subline < static_cast<size_t>(count)) {
                    count -= b_view_->subline;
                    b_view_->subline = 0;
                } else {
                    b_view_->subline -= count;
                    count = 0;
                }
            }
        }
    }
}

void Frame::ScrollRowsNoWrap(int64_t count, size_t content_width) {
    (void)content_width;
    if (count > 0) {
        b_view_->line = std::min(b_view_->line + count, buffer_->LineCnt() - 1);
    } else {
        b_view_->line =
            std::max<int64_t>(static_cast<int64_t>(b_view_->line) + count,
                              0);  // cast is necessary here
    }
}

void Frame::ScrollRows(int64_t count) {
    MGO_ASSERT(buffer_);
    b_view_->make_cursor_visible = false;
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    MakeSureViewValid();
    if (!GetOpt<bool>(kOptWrap)) {
        ScrollRowsNoWrap(count, width_ - sidebar_width);
    } else {
        ScrollRowsWrap(count, width_ - sidebar_width);
    }
}

void Frame::ScrollCols(int64_t count) { (void)count; }

bool Frame::CursorGoRightState(CursorState& state) {
    MGO_ASSERT(buffer_);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // end
    if (buffer_->GetLine(state.line).size() == state.byte_offset) {
        return false;
    }

    Character charater;
    int len;
    Result ret = ThisCharacter(buffer_->GetLine(state.line), state.byte_offset,
                               charater, len);
    MGO_ASSERT(ret == kOk);
    state.byte_offset += len;
    return true;
}

bool Frame::CursorGoLeftState(CursorState& state) {
    MGO_ASSERT(buffer_);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // home
    if (state.byte_offset == 0) {
        return false;
    }

    Character charater;
    int len;
    Result ret = PrevCharacter(buffer_->GetLine(state.line), state.byte_offset,
                               charater, len);
    MGO_ASSERT(ret == kOk);
    state.byte_offset -= len;
    return true;
}

bool Frame::CursorGoUpStateWrap(size_t count, size_t content_width,
                                CursorState& state) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    size_t byte_offset = 0;
    std::vector<size_t> subline_begin_byte_offsets;
    while (true) {
        subline_begin_byte_offsets.push_back(byte_offset);
        byte_offset = ArrangeLine(buffer_->GetLine(state.line), byte_offset, 0,
                                  content_width, tabstop, true);
        if (state.byte_offset < byte_offset ||
            byte_offset == buffer_->GetLine(state.line).size()) {
            break;
        }
    }
    subline_begin_byte_offsets.pop_back();

    size_t i = count;
    while (true) {
        if (i <= subline_begin_byte_offsets.size()) {
            byte_offset =
                subline_begin_byte_offsets[subline_begin_byte_offsets.size() -
                                           i];
            i = 0;
            break;
        } else {
            i -= subline_begin_byte_offsets.size();
            if (state.line == 0) {
                byte_offset = 0;
                break;
            }
            state.line--;
        }

        subline_begin_byte_offsets.clear();
        if (buffer_->GetLine(state.line).size() == 0) {
            subline_begin_byte_offsets.push_back(0);
            continue;
        }
        byte_offset = 0;
        while (byte_offset < buffer_->GetLine(state.line).size()) {
            subline_begin_byte_offsets.push_back(byte_offset);
            byte_offset = ArrangeLine(buffer_->GetLine(state.line), byte_offset,
                                      0, content_width, tabstop, true);
        }
    }
    if (i == count) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.line), state.b_view_col_want.value(),
        byte_offset, content_width, true);
    return true;
}

bool Frame::CursorGoUpStateNoWrap(size_t count, size_t content_width,
                                  CursorState& state) {
    // first line
    if (state.line == 0) {
        return false;
    }

    state.line = state.line > count ? state.line - count : 0;
    MakeSureBColViewWantReady(state);
    state.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.line), cursor_->b_view_col_want.value(), 0,
        content_width, false);
    return true;
}

bool Frame::CursorGoUpState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return false;
    }
    if (!GetOpt<bool>(kOptWrap)) {
        return CursorGoUpStateNoWrap(count, width_ - sidebar_width, state);
    } else {
        return CursorGoUpStateWrap(count, width_ - sidebar_width, state);
    }
}

bool Frame::CursorGoDownStateWrap(size_t count, size_t content_width,
                                  CursorState& state) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    size_t byte_offset = 0;

    size_t subline_begin_byte_offset = byte_offset;
    while (true) {
        byte_offset =
            ArrangeLine(buffer_->GetLine(state.line), subline_begin_byte_offset,
                        0, content_width, tabstop, true);
        if (state.byte_offset < byte_offset ||
            byte_offset == buffer_->GetLine(state.line).size()) {
            break;
        }
        subline_begin_byte_offset = byte_offset;
    }

    size_t i = 0;
    while (true) {
        if (byte_offset == buffer_->GetLine(state.line).size()) {
            if (state.line == buffer_->LineCnt() - 1) {
                break;
            }
            state.line++;
            subline_begin_byte_offset = 0;
        } else {
            subline_begin_byte_offset = byte_offset;
        }
        if (++i == count) {
            break;
        }
        byte_offset =
            ArrangeLine(buffer_->GetLine(state.line), subline_begin_byte_offset,
                        0, content_width, tabstop, true);
    }
    if (i == 0) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.line), state.b_view_col_want.value(),
        subline_begin_byte_offset, content_width, true);
    return true;
}

bool Frame::CursorGoDownStateNoWrap(size_t count, size_t content_width,
                                    CursorState& state) {
    // last line
    if (buffer_->LineCnt() - 1 == state.line) {
        return false;
    }

    // TODO: Overflow?
    state.line = std::min(buffer_->LineCnt() - 1, state.line + count);
    MakeSureBColViewWantReady(state);
    state.byte_offset = CalcByteOffsetByBViewCol(buffer_->GetLine(state.line),
                                                 state.b_view_col_want.value(),
                                                 0, content_width, false);
    return true;
}

bool Frame::CursorGoDownState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return false;
    }
    if (!GetOpt<bool>(kOptWrap)) {
        return CursorGoDownStateNoWrap(count, width_ - sidebar_width, state);
    } else {
        return CursorGoDownStateWrap(count, width_ - sidebar_width, state);
    }
}

bool Frame::CursorGoHomeState(CursorState& state) {
    MGO_ASSERT(buffer_);
    if (state.byte_offset == 0) {
        return false;
    }
    state.byte_offset = 0;
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoEndState(CursorState& state) {
    MGO_ASSERT(buffer_);
    if (state.byte_offset == buffer_->GetLine(state.line).size()) {
        return false;
    }
    state.byte_offset = buffer_->GetLine(state.line).size();
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoWordEndState(bool one_more_character, CursorState& state) {
    MGO_ASSERT(buffer_);
    const std::string* cur_line = &buffer_->GetLine(state.line);
    if (state.byte_offset == cur_line->size()) {
        if (state.line == buffer_->LineCnt() - 1) {
            return false;
        }
        state.line++;
        state.byte_offset = 0;
        cur_line = &buffer_->GetLine(state.line);
    }
    Result res = WordEnd(*cur_line, state.byte_offset, one_more_character,
                         state.byte_offset);
    MGO_ASSERT(res == kOk);
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoWordBeginState(CursorState& state) {
    MGO_ASSERT(buffer_);
    const std::string* cur_line = &buffer_->GetLine(state.line);
    if (state.byte_offset == 0) {
        if (state.line == 0) {
            return false;
        }
        state.line--;
        cur_line = &buffer_->GetLine(state.line);
        state.byte_offset = cur_line->size();
    }
    Result res = WordBegin(*cur_line, state.byte_offset, state.byte_offset);
    MGO_ASSERT(res == kOk || res == kNotExist);
    state.DontHoldColWant();
    return true;
}

bool Frame::CursorGoLineState(size_t line, CursorState& state) {
    MGO_ASSERT(buffer_);
    if (line == state.line) {
        return false;
    }

    state.line = std::min(line, buffer_->LineCnt() - 1);
    MakeSureBColViewWantReady(state);
    state.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.line), state.b_view_col_want.value(), 0,
        width_ - SidebarWidth(), GetOpt<bool>(kOptWrap));
    return true;
}

void Frame::CursorGoRight() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoRightState(state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoLeft() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoLeftState(state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoUp(size_t count) {
    MGO_ASSERT(count != 0);
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoUpState(count, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoDown(size_t count) {
    MGO_ASSERT(count != 0);
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoDownState(count, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoHome() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoHomeState(state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoEnd() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoEndState(state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoWordEnd(bool one_more_character) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoWordEndState(one_more_character, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoWordBegin() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoWordBeginState(state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoLine(size_t line) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoLineState(line, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::SelectAll() {
    b_view_->make_cursor_visible = true;
    selection_.active = true;
    selection_.anchor = {0, 0};
    selection_.head = {buffer_->LineCnt() - 1,
                       buffer_->GetLine(buffer_->LineCnt() - 1).size()};
    cursor_->SetPos(selection_.head);
    cursor_->DontHoldColWant();
}

Result Frame::DeleteAtCursor() {
    b_view_->make_cursor_visible = true;
    if (selection_.active) {
        return DeleteSelection();
    } else {
        return DeleteCharacterBeforeCursor();
    }
}

Result Frame::DeleteWordBeforeCursor() {
    MGO_ASSERT(!selection_.active);
    MGO_ASSERT(buffer_);
    b_view_->make_cursor_visible = true;
    Pos deleted_until;
    const std::string* cur_line = &buffer_->GetLine(cursor_->line);
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return kOk;
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
    if (Result res; (res = buffer_->Delete(
                         {deleted_until, {cursor_->line, cursor_->byte_offset}},
                         nullptr, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::AddStringAtCursor(std::string str, const Pos* cursor_pos) {
    b_view_->make_cursor_visible = true;
    if (selection_.active) {
        return ReplaceSelection(std::move(str), cursor_pos);
    } else {
        return AddStringAtCursorNoSelection(std::move(str), cursor_pos);
    }
}

Result Frame::Replace(const Range& range, std::string str,
                      const Pos* cursor_pos) {
    b_view_->make_cursor_visible = true;
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

Result Frame::TabAtCursor() {
    b_view_->make_cursor_visible = true;
    // TODO: support tab when selection
    selection_.active = false;

    if (!GetOpt<bool>(kOptTabSpace)) {
        return AddStringAtCursor("\t");
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
    return AddStringAtCursor(std::string(need_space, kSpaceChar));
}

Result Frame::Redo() {
    b_view_->make_cursor_visible = true;
    selection_.active = false;

    Pos pos;
    if (Result res; (res = buffer_->Redo(pos)) != kOk) {
        return res;
    }
    cursor_->SetPos(pos);
    cursor_->DontHoldColWant();
    UpdateSyntax();
    return kOk;
}

Result Frame::Undo() {
    b_view_->make_cursor_visible = true;
    selection_.active = false;

    Pos pos;
    if (Result res; (res = buffer_->Undo(pos)) != kOk) {
        return res;
    }
    cursor_->SetPos(pos);
    cursor_->DontHoldColWant();
    UpdateSyntax();
    return kOk;
}

void Frame::Copy() {
    b_view_->make_cursor_visible = true;
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

Result Frame::Paste() {
    b_view_->make_cursor_visible = true;
    bool lines;
    std::string content = clipboard_->GetContent(lines);
    if (content.empty()) {
        // Nothing in clipboard, do nothing at all.
        // TODO: when error codition occur,
        // content will also be empty, maybe a notification mechansim?
        return kFail;
    }
    if (selection_.active) {
        return ReplaceSelection(std::move(content));
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
        return kOk;
    }
}

void Frame::Cut() {
    b_view_->make_cursor_visible = true;
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

Result Frame::DeleteCharacterBeforeCursor() {
    Range range;
    if (cursor_->byte_offset == 0) {
        if (cursor_->line == 0) {
            return kFail;
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
    if (Result res; (res = buffer_->Delete(range, nullptr, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}
Result Frame::DeleteSelection() {
    Pos pos;
    Pos cursor_pos = {cursor_->line, cursor_->byte_offset};
    if (Result res; (res = buffer_->Delete(selection_.ToRange(), &cursor_pos,
                                           pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    SelectionCancell();
    return kOk;
}

Result Frame::AddStringAtCursorNoSelection(std::string str,
                                           const Pos* cursor_pos) {
    MGO_ASSERT(!selection_.active);

    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    if (Result res; (res = buffer_->Add({cursor_->line, cursor_->byte_offset},
                                        std::move(str), nullptr,
                                        cursor_pos != nullptr, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::ReplaceSelection(std::string str, const Pos* cursor_pos) {
    if (Result res; (res = Replace(selection_.ToRange(), std::move(str),
                                   cursor_pos)) != kOk) {
        return res;
    }
    SelectionCancell();
    return kOk;
}

size_t Frame::SidebarWidth() {
    auto line_number = GetOpt<int64_t>(kOptLineNumber);
    if (line_number == static_cast<int64_t>(LineNumberType::kNone)) {
        return 0;
    }

    // Now we only have line number in no wrap, or a <<< addition may in wrap.
    // We calc width according to the line cnt of the buffer to avoid ui
    // debounce.
    size_t max_line_number = buffer_->LineCnt() + 1;
    bool wrap = GetOpt<bool>(kOptWrap);
    return (wrap ? std::max<size_t>(NumberWidth(max_line_number),
                                    kSublineIndicator.size())
                 : NumberWidth(max_line_number)) +
           2 + 1;  // 2 spaces left and 1 right.
    return 0;
}

void Frame::DrawSidebar(int s_row, size_t absolute_line, size_t sidebar_width) {
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
        size_t cursor_line = b_view_->cursor_state_valid
                                 ? b_view_->cursor_state.line
                                 : cursor_->line;
        line_number =
            absolute_line == cursor_line
                ? absolute_line + 1
                : (absolute_line > cursor_line ? absolute_line - cursor_line
                                               : cursor_line - absolute_line);
    } else {
        // Make compiler happy
        MGO_ASSERT(false);
        line_number = 0;
    }
    std::string line_number_str = std::to_string(line_number);
    size_t left_space = sidebar_width - 1 - line_number_str.size();
    memset(sidebar_buf, kSpaceChar, left_space);
    memcpy(sidebar_buf + left_space, line_number_str.data(),
           line_number_str.size());
    sidebar_buf[left_space + line_number_str.size()] = kSpaceChar;
    sidebar_buf[left_space + line_number_str.size() + 1] = '\0';
    term_->Print(col_, s_row, scheme[kLineNumber], sidebar_buf);
}

Range Frame::CalcWrapRange(size_t content_width) {
    size_t cur_b_view_line = b_view_->line;
    size_t byte_offset = 0;

    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        byte_offset =
            ArrangeLine(buffer_->GetLine(cur_b_view_line), byte_offset, 0,
                        content_width, GetOpt<int64_t>(kOptTabStop), true);
    }
    size_t start_byte_offset = byte_offset;

    for (size_t i = 0; i < height_; i++) {
        if (cur_b_view_line >= buffer_->LineCnt()) {
            break;
        }
        byte_offset =
            ArrangeLine(buffer_->GetLine(cur_b_view_line), byte_offset, 0,
                        content_width, GetOpt<int64_t>(kOptTabStop), true);
        if (byte_offset == buffer_->GetLine(cur_b_view_line).size()) {
            cur_b_view_line++;
            byte_offset = 0;
        }
    }
    return {{b_view_->line, start_byte_offset}, {cur_b_view_line, byte_offset}};
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

bool Frame::SizeValid(size_t sidebar_width) {
    return sidebar_width < width_ && height_ > 0;
}

}  // namespace mango