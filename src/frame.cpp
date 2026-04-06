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
#include "search.h"
#include "syntax.h"

namespace mango {

static constexpr std::string_view kSublineIndicator = "<<<";

Frame::Frame(Cursor* cursor, Opts* opts, SyntaxParser* parser,
             ClipBoard* clipboard) noexcept
    : cursor_(cursor), clipboard_(clipboard), parser_(parser), opts_(opts) {}

void Frame::Draw(BufferSearchContext* search_context) {
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
    auto wrap = GetOpt<bool>(kOptWrap);
    auto eob_mark = GetOpt<bool>(kOptEndOfBufferMark);
    auto trailing_white = GetOpt<bool>(kOptTrailingWhite);

    // Prepare highlights, priority: index 0 -> n, high -> low
    std::vector<const std::vector<Highlight>*> highlights;

    Range render_range;
    if (!wrap) {
        size_t last_line =
            std::min(b_view_->line + height_ - 1, buffer_->LineCnt() - 1);
        // If not wrap, render range is larger than the real render range,
        // but it's ok.
        render_range = {{b_view_->line, 0},
                        {last_line, buffer_->GetLine(last_line).size()}};
    } else {
        render_range = CalcWrapRange(content_width);
    }

    // Search hl
    std::vector<Highlight> search_hl;
    if (search_context && search_context->EnsureSearched(buffer_)) {
        search_hl.reserve(search_context->search_result.size());
        // TODO: Only highlight ranges in the screen
        for (size_t i = 0; i < search_context->search_result.size(); i++) {
            search_hl.push_back(
                {search_context->search_result[i],
                 search_context->current_search == static_cast<int64_t>(i)
                     ? scheme[kSearchCurrent]
                     : scheme[kSearch]});
        }
        highlights.push_back(&search_hl);
    }

    // Selection hl
    std::vector<Highlight> selection_hl;
    if (IsSelectionActive()) {
        selection_hl.resize(1);
        selection_hl[0].range = selection_->ToSelectRange(buffer_);
        selection_hl[0].attr = scheme[kSelection];
        highlights.push_back(&selection_hl);
    }

    // Trailing blank hl
    std::vector<Highlight> trailing_white_hl;
    std::vector<int64_t> trailing_white_begin_pre_line;
    if (trailing_white) {
        size_t line_cnt = render_range.end.line - render_range.begin.line + 1;
        trailing_white_begin_pre_line.reserve(line_cnt);
        for (size_t l = b_view_->line; l < b_view_->line + line_cnt; l++) {
            const std::string& line = buffer_->GetLine(l);
            int64_t i = static_cast<int64_t>(line.size()) - 1;
            for (; i >= 0; i--) {
                // Backward codepoint scan is correct and enough.
                // '\t' will break all, ' ' only may after a pretend codepoint,
                // but we render from begin so we will skip it if it's wrong.
                if (line[i] != kSpaceChar && line[i] != '\t') {
                    break;
                }
            }
            i++;
            trailing_white_begin_pre_line.push_back(i);
            if (i != static_cast<int64_t>(line.size())) {
                trailing_white_hl.push_back(
                    {{{l, static_cast<size_t>(i)}, {l, line.size()}},
                     scheme[kTrailingWhite]});
            }
        }
        highlights.push_back(&trailing_white_hl);
    }

    // Syntax hl
    if (parser_) {
        auto syntax_context =
            parser_->GetBufferSyntaxContext(buffer_, render_range);
        if (syntax_context) {
            highlights.push_back(&syntax_context->syntax_highlight);
        }
    }

    if (wrap) {
        // An empty sidebar
        char empty_sidebar[kMaxSizeTWidth + 3 + 1];
        memset(empty_sidebar, kSpaceChar, sidebar_width);
        empty_sidebar[sidebar_width] = '\0';

        // subline indicator sidebar
        char subline_ind_sidebar[kMaxSizeTWidth + 3 + 1];

        size_t line = b_view_->line;
        size_t byte_offset = render_range.begin.byte_offset;

        MGO_ASSERT(line < buffer_->LineCnt());
        for (size_t i = 0; i < height_; i++) {
            if (line >= buffer_->LineCnt()) {
                if (!eob_mark) break;
                Codepoint codepoint = '~';
                term_->SetCell(content_s_col, i + row_, &codepoint, 1,
                               scheme[kNormal]);
                line++;
                continue;
            }

            if (byte_offset == 0) {
                DrawSidebar(row_ + i, line, sidebar_width);
            } else {
                if (i == 0) {
                    memset(subline_ind_sidebar, kSpaceChar, sidebar_width);
                    size_t left_space_size =
                        sidebar_width - 1 - kSublineIndicator.size();
                    memcpy(subline_ind_sidebar + left_space_size,
                           kSublineIndicator.data(), kSublineIndicator.size());
                    // If first row is a subline, we draw a <<< at the sidebar
                    term_->Print(0, row_ + i, scheme[kSidebar],
                                 subline_ind_sidebar);
                } else {
                    term_->Print(0, row_ + i, scheme[kSidebar], empty_sidebar);
                }
            }
            const auto& line_str = buffer_->GetLine(line);
            int64_t trailing_white_begin =
                trailing_white
                    ? trailing_white_begin_pre_line[line -
                                                    render_range.begin.line]
                    : line_str.size();
            byte_offset =
                DrawLine(*term_, line_str, {line, byte_offset}, 0,
                         content_width, i + row_, content_s_col, &highlights,
                         scheme[kNormal], trailing_white_begin, tabstop, true);
            if (byte_offset == line_str.size()) {
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
                if (!eob_mark) break;
                Codepoint codepoint = '~';
                term_->SetCell(content_s_col, cur_s_row, &codepoint, 1,
                               scheme[kNormal]);
                continue;
            }
            DrawSidebar(cur_s_row, line, sidebar_width);
            const auto& line_str = buffer_->GetLine(line);
            int64_t trailing_white_begin =
                trailing_white
                    ? trailing_white_begin_pre_line[line -
                                                    render_range.begin.line]
                    : line_str.size();
            DrawLine(*term_, line_str, {line, 0}, b_view_->col, content_width,
                     cur_s_row, content_s_col, &highlights, scheme[kNormal],
                     trailing_white_begin, tabstop, false);
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
    b_view_->line = cursor_->pos.line;

    size_t byte_offset = 0;
    size_t subline = 0;
    size_t end_b_view_col;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        byte_offset = ArrangeLine(
            buffer_->GetLine(b_view_->line), byte_offset, 0, content_width,
            GetOpt<int64_t>(kOptTabStop), true, &end_b_view_col,
            &cursor_->pos.byte_offset, &stop_at_target, &character_cnt);
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
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    // Which subline of the line the cursor in?
    size_t byte_offset = 0;
    size_t subline = 0;
    size_t end_view_col;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        byte_offset = ArrangeLine(buffer_->GetLine(cursor_->pos.line),
                                  byte_offset, 0, content_width, tabstop, true,
                                  &end_view_col, &cursor_->pos.byte_offset,
                                  &stop_at_target, &character_cnt);
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
    size_t line = cursor_->pos.line - 1;
    // We don't need to check line isn't 0, because there must be a screen range
    // of text before us.
    while (true) {
        size_t row_cnt =
            ScreenRows(buffer_->GetLine(line), content_width, tabstop);
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
    if (cursor_->pos < Pos{line, byte_offset}) {
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
        if (line == cursor_->pos.line) {
            size_t end_view_col;
            bool stop_at_cursor;
            size_t character_cnt;
            byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                      content_width, tabstop, true,
                                      &end_view_col, &cursor_->pos.byte_offset,
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
    size_t row = cursor_->pos.line;

    const std::string& cur_line = buffer_->GetLine(cursor_->pos.line);
    Character character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    cursor_->character_in_line = 0;
    while (offset < cursor_->pos.byte_offset) {
        int byte_len;
        ThisCharacter(cur_line, offset, character, byte_len);
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
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    MGO_ASSERT(state.pos.line < buffer_->LineCnt());
    if (GetOpt<bool>(kOptWrap)) {
        size_t byte_offset = 0;
        while (true) {
            bool stop;
            size_t b_view_col;
            byte_offset = ArrangeLine(
                buffer_->GetLine(state.pos.line), byte_offset, 0, content_width,
                tabstop, false, &b_view_col, &state.pos.byte_offset, &stop);
            if (stop) {
                state.b_view_col_want = b_view_col;
                break;
            }
        }
    } else {
        size_t b_view_col;
        ArrangeLine(buffer_->GetLine(state.pos.line), 0, 0, content_width,
                    tabstop, false, &b_view_col, &state.pos.byte_offset);
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
        ThisCharacter(line, cur_byte_offset, character, byte_len);
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
        cursor_->pos = {buffer_->LineCnt() - 1,
                        buffer_->GetLine(buffer_->LineCnt() - 1).size()};
        cursor_->DontHoldColWant();
        return;
    }

    cursor_->pos.line = cur_b_view_row;

    // Search througn line
    size_t target_b_view_col = s_col - (col_ + sidebar_width) + b_view_->col;
    cursor_->pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(cursor_->pos.line), target_b_view_col, 0,
        width_ - SidebarWidth(), false);
    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void Frame::SetCursorHintWrap(size_t s_row, size_t s_col,
                              size_t sidebar_width) {
    size_t line = b_view_->line;
    size_t byte_offset = 0;
    // to the screen row where hint is.

    int tabstop = GetOpt<int64_t>(kOptTabStop);
    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                  width_ - sidebar_width, tabstop, true);
    }
    size_t cur_screen_row = row_;
    for (; cur_screen_row < s_row; cur_screen_row++) {
        if (line >= buffer_->LineCnt()) {
            break;
        }
        byte_offset = ArrangeLine(buffer_->GetLine(line), byte_offset, 0,
                                  width_ - sidebar_width, tabstop, true);
        if (byte_offset == buffer_->GetLine(line).size()) {
            line++;
            byte_offset = 0;
        }
    }
    if (line >= buffer_->LineCnt()) {
        // Locate in last line end.
        cursor_->pos = {buffer_->LineCnt() - 1,
                        buffer_->GetLine(buffer_->LineCnt() - 1).size()};
        cursor_->DontHoldColWant();
        return;
    }

    // Search througn line after byte_offset
    cursor_->pos = {line, CalcByteOffsetByBViewCol(
                              buffer_->GetLine(line), s_col - sidebar_width,
                              byte_offset, width_ - sidebar_width, true)};

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
    MGO_ASSERT(count != 0);
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

bool Frame::CursorGoRightState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // end
    auto& line = buffer_->GetLine(state.pos.line);
    if (line.size() == state.pos.byte_offset) {
        return false;
    }

    Character c;
    int len;
    for (size_t i = 0; i < count; i++) {
        ThisCharacter(line, state.pos.byte_offset, c, len);
        if (len == 0) {
            break;
        }
        state.pos.byte_offset += len;
    }
    return true;
}

bool Frame::CursorGoLeftState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // home
    if (state.pos.byte_offset == 0) {
        return false;
    }

    auto& line = buffer_->GetLine(state.pos.line);
    Character c;
    int len;
    for (size_t i = 0; i < count; i++) {
        PrevCharacter(line, state.pos.byte_offset, c, len);
        if (len == 0) {
            break;
        }
        state.pos.byte_offset -= len;
    }
    return true;
}

bool Frame::CursorGoUpStateWrap(size_t count, size_t content_width,
                                CursorState& state) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    size_t byte_offset = 0;
    std::vector<size_t> subline_begin_byte_offsets;
    while (true) {
        subline_begin_byte_offsets.push_back(byte_offset);
        byte_offset = ArrangeLine(buffer_->GetLine(state.pos.line), byte_offset,
                                  0, content_width, tabstop, true);
        if (state.pos.byte_offset < byte_offset ||
            byte_offset == buffer_->GetLine(state.pos.line).size()) {
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
            if (state.pos.line == 0) {
                byte_offset = 0;
                break;
            }
            state.pos.line--;
        }

        subline_begin_byte_offsets.clear();
        if (buffer_->GetLine(state.pos.line).size() == 0) {
            subline_begin_byte_offsets.push_back(0);
            continue;
        }
        byte_offset = 0;
        while (byte_offset < buffer_->GetLine(state.pos.line).size()) {
            subline_begin_byte_offsets.push_back(byte_offset);
            byte_offset =
                ArrangeLine(buffer_->GetLine(state.pos.line), byte_offset, 0,
                            content_width, tabstop, true);
        }
    }
    if (i == count) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.pos.line), state.b_view_col_want.value(),
        byte_offset, content_width, true);
    return true;
}

bool Frame::CursorGoUpStateNoWrap(size_t count, size_t content_width,
                                  CursorState& state) {
    // first line
    if (state.pos.line == 0) {
        return false;
    }

    state.pos.line = state.pos.line > count ? state.pos.line - count : 0;
    MakeSureBColViewWantReady(state);
    state.pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.pos.line), cursor_->b_view_col_want.value(), 0,
        content_width, false);
    return true;
}

bool Frame::CursorGoUpState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
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
        byte_offset = ArrangeLine(buffer_->GetLine(state.pos.line),
                                  subline_begin_byte_offset, 0, content_width,
                                  tabstop, true);
        if (state.pos.byte_offset < byte_offset ||
            byte_offset == buffer_->GetLine(state.pos.line).size()) {
            break;
        }
        subline_begin_byte_offset = byte_offset;
    }

    size_t i = 0;
    while (true) {
        if (byte_offset == buffer_->GetLine(state.pos.line).size()) {
            if (state.pos.line == buffer_->LineCnt() - 1) {
                break;
            }
            state.pos.line++;
            subline_begin_byte_offset = 0;
        } else {
            subline_begin_byte_offset = byte_offset;
        }
        if (++i == count) {
            break;
        }
        byte_offset = ArrangeLine(buffer_->GetLine(state.pos.line),
                                  subline_begin_byte_offset, 0, content_width,
                                  tabstop, true);
    }
    if (i == 0) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.pos.line), state.b_view_col_want.value(),
        subline_begin_byte_offset, content_width, true);
    return true;
}

bool Frame::CursorGoDownStateNoWrap(size_t count, size_t content_width,
                                    CursorState& state) {
    // last line
    if (buffer_->LineCnt() - 1 == state.pos.line) {
        return false;
    }

    // TODO: Overflow?
    state.pos.line = std::min(buffer_->LineCnt() - 1, state.pos.line + count);
    MakeSureBColViewWantReady(state);
    state.pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.pos.line), state.b_view_col_want.value(), 0,
        content_width, false);
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
    if (state.pos.byte_offset == 0) {
        return false;
    }
    state.pos.byte_offset = 0;
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoFirstNonBlankState(CursorState& state) {
    MGO_ASSERT(buffer_);
    auto& line = buffer_->GetLine(cursor_->pos.line);
    Character c;
    size_t s = line.size();
    int byte_len;
    size_t i = 0;
    for (; i < s; i += byte_len) {
        ThisCharacter(line, i, c, byte_len);
        char ascii;
        if (c.Ascii(ascii) && (ascii == ' ' || ascii == '\t')) {
            continue;
        }
        break;
    }
    if (i == state.pos.byte_offset) {
        return false;
    }
    state.pos.byte_offset = i;
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoEndState(CursorState& state) {
    MGO_ASSERT(buffer_);
    if (state.pos.byte_offset == buffer_->GetLine(state.pos.line).size()) {
        return false;
    }
    state.pos.byte_offset = buffer_->GetLine(state.pos.line).size();
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoNextWordEndState(size_t count, bool one_more_character,
                                     CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
    size_t i = 0;
    for (; i < count; i++) {
        const std::string* cur_line = &buffer_->GetLine(state.pos.line);
        if (state.pos.byte_offset == cur_line->size()) {
            if (state.pos.line == buffer_->LineCnt() - 1) {
                break;
            }
            state.pos.line++;
            state.pos.byte_offset = 0;
            cur_line = &buffer_->GetLine(state.pos.line);
        }
        NextWordEnd(*cur_line, state.pos.byte_offset, one_more_character,
                    state.pos.byte_offset);
    }
    if (i == 0) {
        return false;
    }
    state.DontHoldColWant();
    return true;
}
bool Frame::CursorGoPrevWordBeginState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
    size_t i = 0;
    for (; i < count; i++) {
        const std::string* cur_line = &buffer_->GetLine(state.pos.line);
        if (state.pos.byte_offset == 0) {
            if (state.pos.line == 0) {
                break;
            }
            state.pos.line--;
            cur_line = &buffer_->GetLine(state.pos.line);
            state.pos.byte_offset = cur_line->size();
        }
        Result res = PrevWordBegin(*cur_line, state.pos.byte_offset,
                                   state.pos.byte_offset);
        MGO_ASSERT(res == kOk || res == kNotExist);
    }
    if (i == 0) {
        return false;
    }
    state.DontHoldColWant();
    return true;
}

bool Frame::CursorGoNextWordBeginState(size_t count, CursorState& state) {
    MGO_ASSERT(buffer_);
    MGO_ASSERT(count != 0);
    size_t i = 0;
    for (; i < count; i++) {
        const std::string* cur_line = &buffer_->GetLine(state.pos.line);
        if (state.pos.byte_offset == cur_line->size()) {
            if (state.pos.line == buffer_->LineCnt() - 1) {
                return false;
            }
            state.pos.line++;
            state.pos.byte_offset = 0;
            cur_line = &buffer_->GetLine(state.pos.line);
        }
        NextWordBegin(*cur_line, state.pos.byte_offset, state.pos.byte_offset);
    }
    if (i == 0) {
        return false;
    }
    state.DontHoldColWant();
    return true;
}

bool Frame::CursorGoLineState(size_t line, CursorState& state) {
    MGO_ASSERT(buffer_);
    if (line == state.pos.line) {
        return false;
    }

    state.pos.line = std::min(line, buffer_->LineCnt() - 1);
    MakeSureBColViewWantReady(state);
    state.pos.byte_offset = CalcByteOffsetByBViewCol(
        buffer_->GetLine(state.pos.line), state.b_view_col_want.value(), 0,
        width_ - SidebarWidth(), GetOpt<bool>(kOptWrap));
    return true;
}

void Frame::CursorGoRight(size_t count) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoRightState(count, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoLeft(size_t count) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoLeftState(count, state)) {
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

void Frame::CursorGoFirstNonBlank() {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoFirstNonBlankState(state)) {
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

void Frame::CursorGoNextWordEnd(size_t count, bool one_more_character) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoNextWordEndState(count, one_more_character, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoPrevWordBegin(size_t count) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoPrevWordBeginState(count, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void Frame::CursorGoNextWordBegin(size_t count) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoNextWordBeginState(count, state)) {
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

void Frame::StartSelection(Pos anchor) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<EditSelection>(anchor, cursor_->pos);
}

void Frame::StartVimSelection(Pos anchor) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<VimSelection>(anchor, cursor_->pos);
}

void Frame::StartVimLineSelection(Pos anchor) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<VimLineSelection>(anchor, cursor_->pos);
}

void Frame::SelectAll() {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<EditSelection>();
    selection_->anchor = {0, 0};
    selection_->head = {buffer_->LineCnt() - 1,
                        buffer_->GetLine(buffer_->LineCnt() - 1).size()};
    cursor_->pos = selection_->head;
    cursor_->DontHoldColWant();
}

void Frame::SelectionFollowCursor() {
    if (IsSelectionActive()) {
        selection_->head = cursor_->pos;
    }
}

Result Frame::DeleteAtCursor() {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        return DeleteSelection();
    } else {
        return DeleteCharacterBeforeCursor();
    }
}

Result Frame::DeleteWordBeforeCursor() {
    MGO_ASSERT(!IsSelectionActive());
    MGO_ASSERT(buffer_);
    b_view_->make_cursor_visible = true;
    Pos deleted_until;
    const std::string* cur_line = &buffer_->GetLine(cursor_->pos.line);
    if (cursor_->pos.byte_offset == 0) {
        if (cursor_->pos.line == 0) {
            return kOk;
        }
        deleted_until.line = cursor_->pos.line - 1;
        cur_line = &buffer_->GetLine(deleted_until.line);
        deleted_until.byte_offset = cur_line->size();
    } else {
        deleted_until = cursor_->pos;
        Result res = PrevWordBegin(*cur_line, deleted_until.byte_offset,
                                   deleted_until.byte_offset);
        MGO_ASSERT(res == kOk || res == kNotExist);
    }
    Pos pos;
    if (Result res; (res = buffer_->Delete({deleted_until, cursor_->pos},
                                           nullptr, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::AddStringAtCursor(std::string_view str, const Pos* cursor_pos) {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        return ReplaceSelection(str, cursor_pos);
    } else {
        return AddStringAtCursorNoSelection(str, cursor_pos);
    }
}

Result Frame::AddStringAtPos(Pos pos, std::string_view str,
                             const Pos* cursor_pos) {
    MGO_ASSERT(!IsSelectionActive());
    b_view_->make_cursor_visible = true;
    Pos new_pos;
    if (cursor_pos != nullptr) {
        new_pos = *cursor_pos;
    }
    if (Result res;
        (res = buffer_->Add(pos, str, &cursor_->pos, cursor_pos != nullptr,
                            new_pos)) != kOk) {
        return res;
    }
    AfterModify(new_pos);
    return kOk;
}

Result Frame::Replace(const Range& range, std::string_view str,
                      const Pos* cursor_pos) {
    b_view_->make_cursor_visible = true;
    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    Result res =
        buffer_->Replace(range, str, &cursor_->pos, cursor_pos != nullptr, pos);
    if (res != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::TabAtCursor() {
    b_view_->make_cursor_visible = true;
    // TODO: support tab when selection
    StopSelection();

    if (!GetOpt<bool>(kOptTabSpace)) {
        return AddStringAtCursor("\t");
    }

    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    int64_t cur_b_view_row = cursor_->pos.line;
    const std::string& cur_line = buffer_->GetLine(cur_b_view_row);
    Character character;
    size_t cur_b_view_c = 0;
    size_t offset = 0;
    while (offset < cursor_->pos.byte_offset) {
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
    StopSelection();

    Pos pos;
    if (Result res; (res = buffer_->Redo(pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::Undo() {
    b_view_->make_cursor_visible = true;
    StopSelection();

    Pos pos;
    if (Result res; (res = buffer_->Undo(pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

void Frame::Copy(bool lines) {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        Range range = selection_->ToSelectRange(buffer_);
        clipboard_->SetContent(buffer_->GetContent(range), lines);
        StopSelection();
    } else {
        Range range = {
            {cursor_->pos.line, 0},
            {cursor_->pos.line, buffer_->GetLine(cursor_->pos.line).size()}};
        clipboard_->SetContent(buffer_->GetContent(range),
                               true);  // always lines
    }
}

Result Frame::Paste(size_t count) {
    MGO_ASSERT(count != 0);
    b_view_->make_cursor_visible = true;
    bool lines;
    std::string content = clipboard_->GetContent(lines);
    if (content.empty()) {
        // Nothing in clipboard, do nothing at all.
        // TODO: when error codition occur,
        // content will also be empty, maybe a notification mechansim?
        return kFail;
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
    if (IsSelectionActive()) {
        return ReplaceSelection(std::move(content));
    } else {
        Pos pos;
        if (lines) {
            buffer_->Add(
                {cursor_->pos.line, buffer_->GetLine(cursor_->pos.line).size()},
                std::move(content), &cursor_->pos, false, pos);
        } else {
            buffer_->Add(cursor_->pos, std::move(content), nullptr, false, pos);
        }
        AfterModify(pos);
        return kOk;
    }
}

void Frame::Cut(bool lines) {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        Range range = selection_->ToSelectRange(buffer_);
        clipboard_->SetContent(buffer_->GetContent(range), lines);
        DeleteSelection();
    } else {
        Range range = {
            {cursor_->pos.line, 0},
            {cursor_->pos.line, buffer_->GetLine(cursor_->pos.line).size()}};
        clipboard_->SetContent(buffer_->GetContent(range),
                               true);  // always lines

        // We try to delete a line where cursor is located.
        Pos pos;
        auto cur_pos = cursor_->pos;
        if (buffer_->LineCnt() == 1) {
            return;
        }

        if (cursor_->pos.line == buffer_->LineCnt() - 1) {
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
    if (cursor_->pos.byte_offset == 0) {
        if (cursor_->pos.line == 0) {
            return kFail;
        }
        range = {{cursor_->pos.line - 1,
                  buffer_->GetLine(cursor_->pos.line - 1).size()},
                 {cursor_->pos.line, 0}};
    } else {
        Character charater;
        int len;
        PrevCharacter(buffer_->GetLine(cursor_->pos.line),
                      cursor_->pos.byte_offset, charater, len);
        range = {{cursor_->pos.line, cursor_->pos.byte_offset - len},
                 cursor_->pos};
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
    if (Result res; (res = buffer_->Delete(selection_->ToDeleteRange(buffer_),
                                           &cursor_->pos, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    StopSelection();
    return kOk;
}

Result Frame::AddStringAtCursorNoSelection(std::string_view str,
                                           const Pos* cursor_pos) {
    MGO_ASSERT(!IsSelectionActive());

    Pos pos;
    if (cursor_pos != nullptr) {
        pos = *cursor_pos;
    }
    if (Result res; (res = buffer_->Add(cursor_->pos, str, nullptr,
                                        cursor_pos != nullptr, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result Frame::ReplaceSelection(std::string_view str, const Pos* cursor_pos) {
    MGO_ASSERT(IsSelectionActive());
    if (Result res;
        // FIXME: ToSelectRange?
        (res = Replace(selection_->ToSelectRange(buffer_), str, cursor_pos)) !=
        kOk) {
        return res;
    }
    StopSelection();
    return kOk;
}

BufferSearchState Frame::CursorGoSearchResultState(BufferSearchContext& context,
                                                   bool next, size_t count,
                                                   bool keep_current_if_one,
                                                   CursorState& state) {
    if (!context.NearestSearchPos(state.pos, buffer_, next, count,
                                  keep_current_if_one)) {
        return {};
    }
    state.pos = context.search_result[context.current_search].begin;
    state.DontHoldColWant();
    return {static_cast<size_t>(context.current_search + 1),
            context.search_result.size()};
}

bool Frame::BufferViewGoSearchResult(BufferSearchContext& context, bool next,
                                     size_t count, bool keep_current_if_one,
                                     CursorState& state) {
    BufferSearchState s = CursorGoSearchResultState(context, next, count,
                                                    keep_current_if_one, state);
    if (s.total == 0) {
        return false;
    }

    CursorState cursor_state(cursor_);
    state.SetCursor(cursor_);
    // TODO: maybe we can pass an arg to MakeCursorVisible?
    MakeSureViewValid();
    MakeCursorVisible();
    cursor_state.SetCursor(cursor_);
    return true;
}

size_t Frame::SidebarWidth() {
    auto line_number =
        static_cast<LineNumberType>(GetOpt<int64_t>(kOptLineNumber));
    if (line_number == LineNumberType::kNone) {
        return 0;
    }

    // Now we only have line number in no wrap, or a <<< addition may in
    // wrap. We calc width according to the line cnt of the buffer to avoid
    // ui debounce.
    size_t max_line_number = buffer_->LineCnt();
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
                                 ? b_view_->cursor_state.pos.line
                                 : cursor_->pos.line;
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
    term_->Print(col_, s_row, scheme[kSidebar], sidebar_buf);
}

Range Frame::CalcWrapRange(size_t content_width) {
    size_t cur_b_view_line = b_view_->line;
    size_t byte_offset = 0;

    int tabstop = GetOpt<int64_t>(kOptTabStop);
    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        byte_offset = ArrangeLine(buffer_->GetLine(cur_b_view_line),
                                  byte_offset, 0, content_width, tabstop, true);
    }
    size_t start_byte_offset = byte_offset;

    for (size_t i = 0; i < height_; i++) {
        if (cur_b_view_line >= buffer_->LineCnt()) {
            break;
        }
        byte_offset = ArrangeLine(buffer_->GetLine(cur_b_view_line),
                                  byte_offset, 0, content_width, tabstop, true);
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

void Frame::AfterModify(const Pos& cursor_pos) {
    cursor_->pos = cursor_pos;
    cursor_->DontHoldColWant();
    UpdateSyntax();
}

bool Frame::SizeValid(size_t sidebar_width) {
    return sidebar_width < width_ && height_ > 0;
}

}  // namespace mango
