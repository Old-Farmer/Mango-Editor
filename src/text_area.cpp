#include "text_area.h"

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
#include "str.h"

namespace charxed {

static constexpr std::string_view kSublineIndicator = "<<<";

TextArea::TextArea(Cursor* cursor, Opts* opts, SyntaxParser* parser,
                   ClipBoard* clipboard) noexcept
    : cursor_(cursor), clipboard_(clipboard), parser_(parser), opts_(opts) {}

void TextArea::Draw(BufferSearchReplaceContext* search_context) {
    CHX_ASSERT(buffer_ != nullptr);
    if (!buffer_->IsLoad()) {
        return;
    }

    DrawContext context;

    context.sidebar_width = SidebarWidth();
    if (!SizeValid(context.sidebar_width)) {
        return;
    }

    context.content_s_col = col_ + context.sidebar_width;
    context.content_width = width_ - context.sidebar_width;
    context.need_hl_cursor_line =
        GetOpt<bool>(kOptHighlightCursorLine) && !IsSelectionActive();
    context.cursor_line = b_view_->cursor_state_valid
                              ? b_view_->cursor_state.pos.line
                              : cursor_->pos.line;

    auto wrap = GetOpt<bool>(kOptWrap);

    // Prepare highlights, priority: index 0 -> n, high -> low

    if (!wrap) {
        size_t last_line =
            std::min(b_view_->line + height_ - 1, buffer_->LineCnt() - 1);
        // If not wrap, render range is larger than the real render range,
        // but it's ok.
        context.render_range = {
            {b_view_->line, 0},
            {last_line, buffer_->GetLineView(last_line).Size()}};
    } else {
        context.render_range = CalcWrapRange(context.content_width);
    }

    auto seach_hl = PrepareSearchHighlight(search_context);
    if (!seach_hl.empty()) context.highlights.push_back(&seach_hl);
    context.selection_hl = PrepareSelectionHighlight();
    if (!context.selection_hl.empty())
        context.highlights.push_back(&context.selection_hl);
    std::vector<Highlight> trailing_white_hl;
    std::tie(trailing_white_hl, context.trailing_white_begin_pre_line) =
        PrepareTrailingBlankHighlight(context.render_range);
    if (!trailing_white_hl.empty())
        context.highlights.push_back(&trailing_white_hl);

    // Syntax hl
    if (parser_) {
        auto syntax_context =
            parser_->GetBufferSyntaxContext(buffer_, context.render_range);
        if (syntax_context) {
            context.highlights.push_back(&syntax_context->syntax_highlight);
        }
    }

    if (wrap) {
        DrawWarp(context);
    } else {
        DrawNoWarp(context);
    }
}

bool TextArea::In(size_t s_col, size_t s_row) {
    return s_col >= col_ && s_col < s_col + width_ && s_row >= row_ &&
           s_row < row_ + height_;
}

// cursor is before the first row.
void TextArea::MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(
    size_t top_scroll_off, size_t content_width) {
    CHX_ASSERT(b_view_->make_cursor_visible);
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    b_view_->line = cursor_->pos.line;
    b_view_->subline = 0;

    size_t end_b_view_col;
    auto line = buffer_->GetLineView(b_view_->line);
    auto iter = line.begin;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        iter = ArrangeLine(line, iter, 0, content_width, tabstop, true,
                           &end_b_view_col, &cursor_->pos.byte_offset,
                           &stop_at_target, &character_cnt);
        cursor_->character_in_line += character_cnt;
        if (stop_at_target) {
            break;
        }
        b_view_->subline++;
    }

    // try to put top_scroll_off screen lines before cursor
    size_t actual_scroll = 0;
    if (top_scroll_off <= b_view_->subline) {
        b_view_->subline -= top_scroll_off;
        actual_scroll = top_scroll_off;
    } else {
        actual_scroll = b_view_->subline;
        while (actual_scroll < top_scroll_off) {
            if (b_view_->line == 0) {
                break;
            }
            b_view_->line--;
            size_t rows = ScreenRows(buffer_->GetLineView(b_view_->line),
                                     content_width, tabstop);
            if (rows >= top_scroll_off - actual_scroll) {
                b_view_->subline = rows - (top_scroll_off - actual_scroll);
                actual_scroll = top_scroll_off;
            } else {
                b_view_->subline = 0;
                actual_scroll += rows;
            }
        }
    }

    cursor_->SetScreenPos(end_b_view_col + width_ - content_width,
                          row_ + actual_scroll);
    if (!cursor_->b_view_col_want.has_value()) {
        cursor_->b_view_col_want = end_b_view_col;
    }
}

void TextArea::MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(
    size_t bottom_scroll_off, size_t content_width) {
    CHX_ASSERT(b_view_->make_cursor_visible);
    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    // Which subline of the line the cursor in?
    b_view_->line = cursor_->pos.line;
    b_view_->subline = 0;
    size_t end_view_col;
    auto line_view = buffer_->GetLineView(cursor_->pos.line);
    auto iter = line_view.begin;
    while (true) {
        bool stop_at_target;
        size_t character_cnt;
        iter = ArrangeLine(line_view, iter, 0, content_width, tabstop, true,
                           &end_view_col, &cursor_->pos.byte_offset,
                           &stop_at_target, &character_cnt);
        cursor_->character_in_line += character_cnt;
        if (stop_at_target) {
            break;
        }
        b_view_->subline++;
    }

    // screen lines cnt from top to cursor
    size_t top_lines = height_ - bottom_scroll_off - 1;

    // Search backward to set the start.
    if (top_lines <= b_view_->subline) {
        b_view_->subline -= top_lines;
    } else {
        size_t i = b_view_->subline;
        while (i < top_lines) {
            if (b_view_->line == 0) {
                break;
            }
            b_view_->line--;
            size_t rows = ScreenRows(buffer_->GetLineView(b_view_->line),
                                     width_, tabstop);
            if (rows >= top_lines - i) {
                b_view_->subline = rows - (top_lines - i);
                i = top_lines;
            } else {
                b_view_->subline = 0;
                i += rows;
            }
        }
    }

    cursor_->SetScreenPos(end_view_col + width_ - content_width,
                          row_ + height_ - bottom_scroll_off - 1);
    if (!cursor_->b_view_col_want.has_value()) {
        cursor_->b_view_col_want = end_view_col;
    }
}

int64_t TextArea::MakeCursorVisibleWrapWhenCursorInRenderRange(
    size_t row, size_t top_scroll_off, size_t bottom_scroll_off,
    size_t content_width) {
    CHX_ASSERT(b_view_->make_cursor_visible);
    int tabstop = GetOpt<int64_t>(kOptTabStop);

    int64_t actual_scroll = 0;
    if (row < top_scroll_off) {  // cursor is too close to the top.
        size_t need_scroll = top_scroll_off - row;
        if (need_scroll <= b_view_->subline) {
            b_view_->subline -= need_scroll;
            actual_scroll = need_scroll;
        } else {
            actual_scroll = b_view_->subline;
            while (actual_scroll < static_cast<int64_t>(need_scroll)) {
                if (b_view_->line == 0) {
                    break;
                }
                b_view_->line--;
                size_t rows = ScreenRows(buffer_->GetLineView(b_view_->line),
                                         content_width, tabstop);
                if (rows >= need_scroll - actual_scroll) {
                    b_view_->subline = rows - (need_scroll - actual_scroll);
                    actual_scroll = need_scroll;
                } else {
                    b_view_->subline = 0;
                    actual_scroll += rows;
                }
            }
        }
        actual_scroll = -actual_scroll;
    } else if (height_ - row <=
               bottom_scroll_off) {  // cursor is too close to the bottom
        size_t need_scroll = bottom_scroll_off - (height_ - row) + 1;
        size_t rows = ScreenRows(buffer_->GetLineView(b_view_->line),
                                 content_width, tabstop);
        if (need_scroll < rows - b_view_->subline) {
            actual_scroll = need_scroll;
        } else {
            actual_scroll = rows - b_view_->subline - 1;
            while (actual_scroll < static_cast<int64_t>(need_scroll)) {
                if (b_view_->line == buffer_->LineCnt() - 1) {
                    break;
                }
                b_view_->line++;
                rows = ScreenRows(buffer_->GetLineView(b_view_->line), width_,
                                  tabstop);
                if (rows >= need_scroll - actual_scroll) {
                    b_view_->subline = need_scroll - actual_scroll - 1;
                    actual_scroll = need_scroll;
                } else {
                    b_view_->subline = rows - 1;
                    actual_scroll += rows;
                }
            }
        }
    }
    return actual_scroll;
}

void TextArea::MakeCursorVisibleWrap(size_t top_scroll_off,
                                     size_t bottom_scroll_off) {
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    size_t content_width = width_ - sidebar_width;
    int tabstop = GetOpt<int64_t>(kOptTabStop);

    size_t line = b_view_->line;
    auto line_view = buffer_->GetLineView(line);
    auto iter = line_view.begin;

    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        iter = ArrangeLine(line_view, iter, 0, content_width, tabstop, true);
    }

    cursor_->character_in_line = 0;
    if (cursor_->pos < Pos{line, iter.offset() - line_view.begin.offset()}) {
        if (!b_view_->make_cursor_visible) {
            cursor_->SetScreenPos(-1, -1);
            return;
        }
        MakeCursorVisibleWrapInnerWhenCursorBeforeRenderRange(top_scroll_off,
                                                              content_width);
        return;
    }

    // We walk through the render range.
    for (size_t i = 0; i < height_; i++) {
        if (line == cursor_->pos.line) {
            size_t end_view_col;
            bool stop_at_cursor;
            size_t character_cnt;
            iter = ArrangeLine(line_view, iter, 0, content_width, tabstop, true,
                               &end_view_col, &cursor_->pos.byte_offset,
                               &stop_at_cursor, &character_cnt);
            cursor_->character_in_line += character_cnt;
            // We find the cursor's location. It's in the screen.
            if (stop_at_cursor) {
                int64_t actual_scroll =
                    b_view_->make_cursor_visible
                        ? MakeCursorVisibleWrapWhenCursorInRenderRange(
                              i, top_scroll_off, bottom_scroll_off,
                              content_width)
                        : 0;
                cursor_->SetScreenPos(end_view_col + width_ - content_width,
                                      row_ + i - actual_scroll);
                if (!cursor_->b_view_col_want.has_value()) {
                    cursor_->b_view_col_want = end_view_col;
                }
                return;
            }
        } else {
            iter =
                ArrangeLine(line_view, iter, 0, content_width, tabstop, true);
        }
        if (iter == line_view.end) {
            line++;
            if (line >= buffer_->LineCnt()) {
                break;
            }
            line_view = buffer_->GetLineView(line);
            iter = line_view.begin;
        }
    }

    if (!b_view_->make_cursor_visible) {
        cursor_->SetScreenPos(-1, -1);
        return;
    }

    // The cursor is after the render range.
    MakeCursorVisibleWrapInnerWhenCursorAfterRenderRange(bottom_scroll_off,
                                                         content_width);
}

void TextArea::MakeCursorVisibleNotWrap(size_t top_scroll_off,
                                        size_t bottom_scroll_off) {
    size_t sidebar_width = SidebarWidth();
    if (!SizeValid(sidebar_width)) {
        return;
    }

    auto tabstop = GetOpt<int64_t>(kOptTabStop);

    // Calculate the cursor pos if we put the buffer from (0, 0)
    size_t row = cursor_->pos.line;

    auto cur_line = buffer_->GetLineView(cursor_->pos.line);
    Character character;
    size_t cur_b_view_c = 0;
    auto iter = cur_line.begin;
    cursor_->character_in_line = 0;
    while (iter.offset() - cur_line.begin.offset() < cursor_->pos.byte_offset) {
        iter = NextCharacter(iter, cur_line.end, character);
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
        b_view_->col = cur_b_view_c + 2 - content_width;
    }

    // adjust row of view
    if (row < b_view_->line + top_scroll_off) {
        if (!b_view_->make_cursor_visible) {
            if (row < b_view_->line) {
                cursor_->SetScreenPos(-1, -1);
                return;
            }
        } else {
            b_view_->line = row > top_scroll_off ? row - top_scroll_off : 0;
        }
    } else if (row - b_view_->line >= height_ - bottom_scroll_off) {
        if (!b_view_->make_cursor_visible) {
            if (row - b_view_->line >= height_) {
                cursor_->SetScreenPos(-1, -1);
                return;
            }
        } else {
            b_view_->line = row + 1 - height_ + bottom_scroll_off;
        }
    }

    cursor_->SetScreenPos(cur_b_view_c - b_view_->col + content_s_col,
                          row - b_view_->line + row_);
}

void TextArea::MakeCursorVisible() {
    // MakeSureViewValid(); // call it outside
    size_t scroll_off = GetOpt<int64_t>(kOptScrollOff);
    size_t top_scroll_off;
    size_t bottom_scroll_off;
    if (scroll_off * 2 + 1 <= height_) {
        top_scroll_off = bottom_scroll_off = scroll_off;
    } else {
        CHX_ASSERT(height_ >= 1);
        top_scroll_off = (height_ - 1) / 2;
        bottom_scroll_off = (height_ - 1) - top_scroll_off;
    }

    if (GetOpt<bool>(kOptWrap)) {
        MakeCursorVisibleWrap(top_scroll_off, bottom_scroll_off);
    } else {
        MakeCursorVisibleNotWrap(top_scroll_off, bottom_scroll_off);
    }
}

void TextArea::MakeSureViewValid() {
    if (GetOpt<bool>(kOptWrap)) {
        if (b_view_->line >= buffer_->LineCnt()) {
            b_view_->line = buffer_->LineCnt() - 1;
            b_view_->subline = ScreenRows(buffer_->GetLineView(b_view_->line),
                                          width_ - SidebarWidth(),
                                          GetOpt<int64_t>(kOptTabStop)) -
                               1;
        } else {
            if (b_view_->subline == 0) {
                return;
            }
            b_view_->subline =
                std::min(ScreenRows(buffer_->GetLineView(b_view_->line),
                                    width_ - SidebarWidth(),
                                    GetOpt<int64_t>(kOptTabStop)) -
                             1,
                         b_view_->subline);
        }
    } else {
        b_view_->line = std::min(buffer_->LineCnt() - 1, b_view_->line);
    }
}

void TextArea::MakeSureBColViewWantReady(CursorState& state) {
    if (state.b_view_col_want.has_value()) {
        return;
    }

    size_t content_width = width_ - SidebarWidth();
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    CHX_ASSERT(state.pos.line < buffer_->LineCnt());
    auto line = buffer_->GetLineView(state.pos.line);
    size_t b_view_col;
    if (GetOpt<bool>(kOptWrap)) {
        auto iter = line.begin;
        while (true) {
            bool stop;
            iter = ArrangeLine(line, iter, 0, content_width, tabstop, false,
                               &b_view_col, &state.pos.byte_offset, &stop);
            if (stop) {
                state.b_view_col_want = b_view_col;
                break;
            }
        }
    } else {
        ArrangeLine(line, line.begin, 0, content_width, tabstop, false,
                    &b_view_col, &state.pos.byte_offset);
        state.b_view_col_want = b_view_col;
    }
}

size_t TextArea::CalcByteOffsetByBViewCol(std::string_view line,
                                          size_t b_view_col_from_byte_offset,
                                          size_t byte_offset,
                                          size_t content_width) {
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    auto wrap = GetOpt<bool>(kOptWrap);

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

TextTree::Iterator TextArea::CalcByteOffsetByBViewCol(
    const TextTree::TextView& line, size_t b_view_col_from_byte_offset,
    TextTree::Iterator iter, size_t content_width) {
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    auto wrap = GetOpt<bool>(kOptWrap);

    size_t target_b_view_col = b_view_col_from_byte_offset;
    Character character;
    size_t cur_b_view_col = 0;
    while (iter != line.end) {
        auto next = NextCharacter(iter, line.end, character);
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
            if (wrap && next == line.end &&
                cur_b_view_col + character_width == content_width) {
                break;
            }
            if (cur_b_view_col <= target_b_view_col &&
                target_b_view_col < cur_b_view_col + character_width) {
                break;
            }
        } else {
            break;
        }
        cur_b_view_col += character_width;
        iter = next;
    }
    return iter;
}

void TextArea::SetCursorHintNoWrap(size_t s_row, size_t s_col,
                                   size_t sidebar_width) {
    size_t cur_b_view_row = s_row - row_ + b_view_->line;
    // empty line, locate the last line end
    if (cur_b_view_row >= buffer_->LineCnt()) {
        cursor_->pos = {buffer_->LineCnt() - 1,
                        buffer_->GetLineView(buffer_->LineCnt() - 1).Size()};
        cursor_->DontHoldColWant();
        return;
    }

    cursor_->pos.line = cur_b_view_row;

    // Search througn line
    size_t target_b_view_col = s_col - (col_ + sidebar_width) + b_view_->col;
    auto line = buffer_->GetLineView(cursor_->pos.line);
    auto iter = line.begin;
    cursor_->pos.byte_offset =
        CalcByteOffsetByBViewCol(line, target_b_view_col, iter,
                                 width_ - SidebarWidth())
            .offset() -
        line.begin.offset();
    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void TextArea::SetCursorHintWrap(size_t s_row, size_t s_col,
                                 size_t sidebar_width) {
    size_t line = b_view_->line;
    auto line_view = buffer_->GetLineView(line);
    auto iter = line_view.begin;
    // to the screen row where hint is.

    int tabstop = GetOpt<int64_t>(kOptTabStop);
    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        iter = ArrangeLine(line_view, iter, 0, width_ - sidebar_width, tabstop,
                           true);
    }
    size_t cur_screen_row = row_;
    for (; cur_screen_row < s_row; cur_screen_row++) {
        iter = ArrangeLine(line_view, iter, 0, width_ - sidebar_width, tabstop,
                           true);
        if (iter == line_view.end) {
            line++;
            if (line >= buffer_->LineCnt()) {
                break;
            }
            line_view = buffer_->GetLineView(line);
            iter = line_view.begin;
        }
    }
    if (line >= buffer_->LineCnt()) {
        // Locate in last line end.
        cursor_->pos = {buffer_->LineCnt() - 1,
                        buffer_->GetLineView(buffer_->LineCnt() - 1).Size()};
        cursor_->DontHoldColWant();
        return;
    }

    // Search througn line after byte_offset
    cursor_->pos = {
        line, CalcByteOffsetByBViewCol(line_view, s_col - sidebar_width, iter,
                                       width_ - sidebar_width)
                      .offset() -
                  line_view.begin.offset()};

    SelectionFollowCursor();
    cursor_->DontHoldColWant();
}

void TextArea::SetCursorHint(size_t s_row, size_t s_col) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(In(s_col, s_row));
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

void TextArea::ScrollRowsWrap(int64_t count, size_t content_width) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    if (count > 0) {
        while (count > 0) {
            size_t row_cnt = ScreenRows(buffer_->GetLineView(b_view_->line),
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
                size_t row_cnt = ScreenRows(buffer_->GetLineView(b_view_->line),
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

void TextArea::ScrollRowsNoWrap(int64_t count, size_t content_width) {
    (void)content_width;
    if (count > 0) {
        b_view_->line = std::min(b_view_->line + count, buffer_->LineCnt() - 1);
    } else {
        b_view_->line =
            std::max<int64_t>(static_cast<int64_t>(b_view_->line) + count,
                              0);  // cast is necessary here
    }
}

void TextArea::ScrollRows(int64_t count) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
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

void TextArea::ScrollCols(int64_t count) { (void)count; }

bool TextArea::CursorGoRightState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // end
    auto iter = buffer_->Find(state.pos);
    auto end = buffer_->LineEnd(state.pos.line);
    if (iter == end) {
        return false;
    }

    Character c;
    size_t offset = iter.offset();
    for (size_t i = 0; i < count && iter != end; i++) {
        iter = NextCharacter(iter, end, c);
    }
    state.pos.byte_offset += iter.offset() - offset;
    return true;
}

bool TextArea::CursorGoLeftState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
    auto _ = gsl::finally([&state] { state.DontHoldColWant(); });

    // home
    if (state.pos.byte_offset == 0) {
        return false;
    }

    auto begin = buffer_->Find({state.pos.line, 0});
    auto iter = buffer_->Find(state.pos);
    Character c;
    for (size_t i = 0; i < count && iter != begin; i++) {
        iter = PrevCharacter(iter, begin, c);
    }
    state.pos.byte_offset = iter.offset() - begin.offset();
    return true;
}

bool TextArea::CursorGoUpStateWrap(size_t count, size_t content_width,
                                   CursorState& state) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    auto iter = buffer_->Find(state.pos);
    auto state_iter = iter;
    auto line = buffer_->GetLineView(state.pos.line);
    std::vector<TextTree::Iterator> subline_begin_iters;
    iter = line.begin;
    while (true) {
        subline_begin_iters.push_back(iter);
        iter = ArrangeLine(line, iter, 0, content_width, tabstop, true);
        if (state_iter < iter || iter == line.end) {
            break;
        }
    }
    subline_begin_iters.pop_back();

    size_t i = count;
    while (true) {
        if (i <= subline_begin_iters.size()) {
            iter = subline_begin_iters[subline_begin_iters.size() - i];
            i = 0;
            break;
        } else {
            i -= subline_begin_iters.size();
            if (state.pos.line == 0) {
                iter = line.begin;
                break;
            }
            state.pos.line--;
            line = buffer_->GetLineView(state.pos.line);
        }

        subline_begin_iters.clear();
        if (line.Size() == 0) {
            subline_begin_iters.push_back(line.begin);
            continue;
        }
        iter = line.begin;
        while (iter != line.end) {
            subline_begin_iters.push_back(iter);
            iter = ArrangeLine(line, iter, 0, content_width, tabstop, true);
        }
    }
    if (i == count) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.pos.byte_offset =
        CalcByteOffsetByBViewCol(line, state.b_view_col_want.value(), iter,
                                 content_width)
            .offset() -
        line.begin.offset();
    return true;
}

bool TextArea::CursorGoUpStateNoWrap(size_t count, size_t content_width,
                                     CursorState& state) {
    // first line
    if (state.pos.line == 0) {
        return false;
    }

    state.pos.line = state.pos.line > count ? state.pos.line - count : 0;
    MakeSureBColViewWantReady(state);
    auto line = buffer_->GetLineView(state.pos.line);
    auto iter = line.begin;
    state.pos.byte_offset =
        CalcByteOffsetByBViewCol(line, cursor_->b_view_col_want.value(), iter,
                                 content_width)
            .offset() -
        line.begin.offset();
    return true;
}

bool TextArea::CursorGoUpState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
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

bool TextArea::CursorGoDownStateWrap(size_t count, size_t content_width,
                                     CursorState& state) {
    int tabstop = GetOpt<int64_t>(kOptTabStop);
    auto iter = buffer_->Find(state.pos);
    auto state_iter = iter;
    auto line = buffer_->GetLineView(state.pos.line);

    auto subline_begin_iter = line.begin;
    while (true) {
        iter = ArrangeLine(line, subline_begin_iter, 0, content_width, tabstop,
                           true);
        if (state_iter < iter || iter == line.end) {
            break;
        }
        subline_begin_iter = iter;
    }

    size_t i = 0;
    line = buffer_->GetLineView(state.pos.line);
    while (true) {
        if (iter == line.end) {
            if (state.pos.line == buffer_->LineCnt() - 1) {
                break;
            }
            state.pos.line++;
            line = buffer_->GetLineView(state.pos.line);
            subline_begin_iter = line.begin;
        } else {
            subline_begin_iter = iter;
        }
        if (++i == count) {
            break;
        }
        iter = ArrangeLine(line, subline_begin_iter, 0, content_width, tabstop,
                           true);
    }
    if (i == 0) {
        return false;
    }

    MakeSureBColViewWantReady(state);
    state.pos.byte_offset =
        CalcByteOffsetByBViewCol(line, state.b_view_col_want.value(),
                                 subline_begin_iter, content_width)
            .offset() -
        line.begin.offset();
    return true;
}

bool TextArea::CursorGoDownStateNoWrap(size_t count, size_t content_width,
                                       CursorState& state) {
    // last line
    if (buffer_->LineCnt() - 1 == state.pos.line) {
        return false;
    }

    // TODO: Overflow?
    state.pos.line = std::min(buffer_->LineCnt() - 1, state.pos.line + count);
    MakeSureBColViewWantReady(state);
    auto line = buffer_->GetLineView(state.pos.line);
    auto iter = line.begin;
    state.pos.byte_offset =
        CalcByteOffsetByBViewCol(line, state.b_view_col_want.value(), iter,
                                 content_width)
            .offset() -
        line.begin.offset();
    return true;
}

bool TextArea::CursorGoDownState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
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

bool TextArea::CursorGoHomeState(CursorState& state) {
    CHX_ASSERT(buffer_);
    if (state.pos.byte_offset == 0) {
        return false;
    }
    state.pos.byte_offset = 0;
    state.DontHoldColWant();
    return true;
}
bool TextArea::CursorGoFirstNonBlankState(CursorState& state) {
    CHX_ASSERT(buffer_);
    auto line = buffer_->GetLineView(cursor_->pos.line);
    Character c;
    auto iter = line.begin;
    while (iter != line.end) {
        auto next = NextCharacter(iter, line.end, c);
        char ascii;
        if (c.Ascii(ascii) && (ascii == ' ' || ascii == '\t')) {
            iter = next;
            continue;
        }
        break;
    }
    if (iter.offset() - line.begin.offset() == state.pos.byte_offset) {
        return false;
    }
    state.pos.byte_offset = iter.offset() - line.begin.offset();
    state.DontHoldColWant();
    return true;
}
bool TextArea::CursorGoEndState(CursorState& state) {
    CHX_ASSERT(buffer_);
    size_t size = buffer_->GetLineView(state.pos.line).Size();
    if (state.pos.byte_offset == size) {
        return false;
    }
    state.pos.byte_offset = size;
    state.b_view_col_want = SIZE_MAX;
    return true;
}
bool TextArea::CursorGoNextWordEndState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
    auto iter = buffer_->Find(state.pos);
    auto end = buffer_->End();
    size_t i = 0;
    for (; i < count && iter != end; i++) {
        iter = NextWordEnd(iter, end);
    }
    if (i == 0) {
        return false;
    }
    state.pos = buffer_->OffsetToPos(iter.offset());
    state.DontHoldColWant();
    return true;
}
bool TextArea::CursorGoPrevWordBeginState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
    auto iter = buffer_->Find(state.pos);
    auto begin = buffer_->Begin();
    size_t i = 0;
    for (; i < count && iter != begin; i++) {
        iter = PrevWordBegin(iter, begin);
    }
    if (i == 0) {
        return false;
    }
    state.pos = buffer_->OffsetToPos(iter.offset());
    state.DontHoldColWant();
    return true;
}

bool TextArea::CursorGoNextWordBeginState(size_t count, CursorState& state) {
    CHX_ASSERT(buffer_);
    CHX_ASSERT(count != 0);
    auto iter = buffer_->Find(state.pos);
    auto end = buffer_->End();
    size_t i = 0;
    for (; i < count && iter != end; i++) {
        iter = NextWordBegin(iter, end);
    }
    if (i == 0) {
        return false;
    }
    state.pos = buffer_->OffsetToPos(iter.offset());
    state.DontHoldColWant();
    return true;
}

bool TextArea::CursorGoLineState(size_t line, CursorState& state) {
    CHX_ASSERT(buffer_);
    if (line == state.pos.line) {
        return false;
    }

    state.pos.line = std::min(line, buffer_->LineCnt() - 1);
    MakeSureBColViewWantReady(state);
    auto line_view = buffer_->GetLineView(state.pos.line);
    auto iter = line_view.begin;
    state.pos.byte_offset =
        CalcByteOffsetByBViewCol(line_view, state.b_view_col_want.value(), iter,
                                 width_ - SidebarWidth())
            .offset() -
        line_view.begin.offset();
    return true;
}

bool TextArea::FindNextCharacterAndCursorGoInCurrentLineState(
    const Character& c, CursorState& state) {
    CHX_ASSERT(buffer_);
    auto line = buffer_->GetLineView(state.pos.line);
    auto target_iter =
        NextSpecificCharacter(buffer_->Find(state.pos), c, line.end);
    if (!target_iter.has_value()) {
        return false;
    }
    state.pos.byte_offset = target_iter->offset() - line.begin.offset();
    state.DontHoldColWant();
    return true;
}

bool TextArea::FindPrevCharacterAndCursorGoInCurrentLineState(
    const Character& c, CursorState& state) {
    CHX_ASSERT(buffer_);
    auto line = buffer_->GetLineView(state.pos.line);
    auto target_iter =
        PrevSpecificCharacter(buffer_->Find(state.pos), c, line.begin);
    if (!target_iter.has_value()) {
        return false;
    }
    state.pos.byte_offset = target_iter->offset() - line.begin.offset();
    state.DontHoldColWant();
    return true;
}

bool TextArea::CursorGoBracketState(CursorState& state) {
    auto iter = buffer_->Find(state.pos);
    auto end = buffer_->End();
    if (iter == end) {
        return false;
    }
    auto [found_iter, is_open] = ClosestBracket(iter, {buffer_->Begin(), end});
    if (found_iter == end) {
        return false;
    }

    if (found_iter != iter) {
        state.pos = buffer_->OffsetToPos(found_iter.offset());
        state.DontHoldColWant();
        return true;
    }
    // current pos is a bracket, jump to the other bracket
    char open;
    if (is_open) {
        open = found_iter.ThisByte();
    } else {
        open = IsPairClose(found_iter.ThisByte()).second;
    }
    auto v = FindBracketPairAround(found_iter, {buffer_->Begin(), end}, open);
    if (v.Size() == 0) {
        return false;
    }
    state.pos = buffer_->OffsetToPos(
        is_open ? v.end.offset() - 1
                : v.begin.offset());  // 1 is because bracket is now char
    state.DontHoldColWant();
    return true;
}

void TextArea::CursorGoRight(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoRightState>(count);
}
void TextArea::CursorGoLeft(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoLeftState>(count);
}
void TextArea::CursorGoUp(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoUpState>(count);
}
void TextArea::CursorGoDown(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoDownState>(count);
}
void TextArea::CursorGoHome() { CursorGo<&TextArea::CursorGoHomeState>(); }
void TextArea::CursorGoFirstNonBlank() {
    CursorGo<&TextArea::CursorGoFirstNonBlankState>();
}
void TextArea::CursorGoEnd() { CursorGo<&TextArea::CursorGoEndState>(); }
void TextArea::CursorGoNextWordEnd(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoNextWordEndState>(count);
}
void TextArea::CursorGoPrevWordBegin(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoPrevWordBeginState>(count);
}
void TextArea::CursorGoNextWordBegin(size_t count) {
    CursorGoWithCount<&TextArea::CursorGoNextWordBeginState>(count);
}
void TextArea::CursorGoBracket() {
    CursorGo<&TextArea::CursorGoBracketState>();
}

void TextArea::CursorGoLine(size_t line) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (CursorGoLineState(line, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void TextArea::FindNextCharacterAndCursorGoInCurrentLine(const Character& c) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (FindNextCharacterAndCursorGoInCurrentLineState(c, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void TextArea::FindPrevCharacterAndCursorGoInCurrentLine(const Character& c) {
    b_view_->make_cursor_visible = true;
    CursorState state(cursor_);
    if (FindPrevCharacterAndCursorGoInCurrentLineState(c, state)) {
        state.SetCursor(cursor_);
        SelectionFollowCursor();
    }
}

void TextArea::StartSelection(Pos anchor) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<NormalSelection>(anchor, cursor_->pos);
}

void TextArea::StartLineSelection(Pos anchor) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<LineSelection>(anchor, cursor_->pos);
}

void TextArea::SelectAll() {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<NormalSelection>();
    selection_->anchor = {0, 0};
    selection_->head = {buffer_->LineCnt() - 1,
                        buffer_->GetLineView(buffer_->LineCnt() - 1).Size()};
    cursor_->pos = selection_->head;
    cursor_->DontHoldColWant();
}

bool TextArea::SelectWord() {
    b_view_->make_cursor_visible = true;
    auto line = buffer_->GetLineView(cursor_->pos.line);
    auto iter = buffer_->Find(cursor_->pos);
    auto word = ThisWord(iter, line);
    if (word.Size() == 0) {
        return false;
    }

    selection_ = std::make_unique<NormalSelection>();
    selection_->anchor = {cursor_->pos.line,
                          word.begin.offset() - line.begin.offset()};
    selection_->head = {cursor_->pos.line,
                        word.end.offset() - line.begin.offset()};
    return true;
}

void TextArea::SelectLinesToLine(size_t line) {
    b_view_->make_cursor_visible = true;
    selection_ = std::make_unique<LineSelection>(Pos{cursor_->pos.line, 0},
                                                 Pos{line, 0});
}

void TextArea::SelectNextLines(size_t count) {
    CHX_ASSERT(count != 0);
    SelectLinesToLine(
        std::min(cursor_->pos.line + count - 1, buffer_->LineCnt() - 1));
}

void TextArea::SelectPrevLines(size_t count) {
    SelectLinesToLine(
        cursor_->pos.line >= (count - 1) ? cursor_->pos.line - (count - 1) : 0);
}

// TODO: include ' ' & '\t' before or after the pair when inner is false.
bool TextArea::SelectPair(char open, bool inner) {
    b_view_->make_cursor_visible = true;
    auto iter = buffer_->Find(cursor_->pos);
    if (isQuote(open)) {
        auto line = buffer_->GetLineView(cursor_->pos.line);
        auto quoted = FindQuotePairAroundInLine(iter, line, open);
        if (quoted.Size() == 0) {
            return false;
        }
        selection_ = std::make_unique<NormalSelection>(
            Pos{cursor_->pos.line, quoted.begin.offset() - line.begin.offset()},
            Pos{cursor_->pos.line, quoted.end.offset() - line.begin.offset()});
    } else {
        auto end = buffer_->End();
        if (iter == end) {
            return false;
        }

        auto brackets =
            FindBracketPairAround(iter, {buffer_->Begin(), end}, open);
        if (brackets.Size() == 0) {
            return false;
        }
        selection_ = std::make_unique<NormalSelection>(
            buffer_->OffsetToPos(brackets.begin.offset()),
            buffer_->OffsetToPos(brackets.end.offset()));
    }

    if (inner) {
        // anchor is open and head is close.
        // Currenty, pairs are all ascii characters, so we can use one byte ++
        // and --.
        selection_->anchor.byte_offset++;
        selection_->head.byte_offset--;
    }
    return true;
}

void TextArea::SelectionFollowCursor() {
    if (IsSelectionActive()) {
        selection_->head = cursor_->pos;
    }
}

Result TextArea::DeleteAtCursor() {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        return DeleteSelection();
    } else {
        return DeleteCharacterBeforeCursor();
    }
}

Result TextArea::DeleteWordBeforeCursor() {
    CHX_ASSERT(!IsSelectionActive());
    CHX_ASSERT(buffer_);
    b_view_->make_cursor_visible = true;
    Pos deleted_until;
    if (cursor_->pos.byte_offset == 0) {
        if (cursor_->pos.line == 0) {
            return kOk;
        }
        deleted_until.line = cursor_->pos.line - 1;
        auto cur_line = buffer_->GetLineView(deleted_until.line);
        deleted_until.byte_offset = cur_line.Size();
    } else {
        auto cur_line_begin = buffer_->Find({cursor_->pos.line, 0});
        auto iter = buffer_->Find(cursor_->pos);
        iter = PrevWordBegin(iter, cur_line_begin);
        deleted_until.line = cursor_->pos.line;
        deleted_until.byte_offset = iter.offset() - cur_line_begin.offset();
    }
    Pos pos;
    if (Result res; (res = buffer_->Delete({deleted_until, cursor_->pos},
                                           nullptr, false, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result TextArea::AddStringAtPos(Pos pos, std::string_view str,
                                const Pos* cursor_pos) {
    CHX_ASSERT(!IsSelectionActive());
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

Result TextArea::Replace(const Range& range, std::string_view str,
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

Result TextArea::TabAtCursor() {
    b_view_->make_cursor_visible = true;
    // TODO: support tab when selection
    StopSelection();

    if (!GetOpt<bool>(kOptTabSpace)) {
        return AddStringAtCursorNoSelection("\t");
    }

    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    int64_t cur_b_view_row = cursor_->pos.line;
    auto cur_line = buffer_->GetLineView(cur_b_view_row);
    Character character;
    size_t cur_b_view_c = 0;
    auto iter = cur_line.begin;
    while (iter.offset() - cur_line.begin.offset() < cursor_->pos.byte_offset) {
        iter = NextCharacter(iter, cur_line.end, character);
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
    }
    int need_space = tabstop - cur_b_view_c % tabstop;
    return AddStringAtCursorNoSelection(std::string(need_space, kSpaceChar));
}

Result TextArea::Redo() {
    b_view_->make_cursor_visible = true;
    StopSelection();

    Pos pos;
    if (Result res; (res = buffer_->Redo(pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result TextArea::Undo() {
    b_view_->make_cursor_visible = true;
    StopSelection();

    Pos pos;
    if (Result res; (res = buffer_->Undo(pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

void TextArea::Copy() {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        Range range = selection_->ToSelectRange(buffer_);
        clipboard_->SetContent(buffer_->GetContent(range),
                               selection_->LineSemantic());
        StopSelection();
    } else {
        Range range = {{cursor_->pos.line, 0},
                       {cursor_->pos.line,
                        buffer_->GetLineView(cursor_->pos.line).Size()}};
        clipboard_->SetContent(buffer_->GetContent(range),
                               true);  // always lines
    }
}

Result TextArea::Paste(size_t count, bool after_cursor) {
    CHX_ASSERT(count != 0);
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
            content.reserve(content.size() * count + (lines ? count : 0));
            for (size_t i = 0; i < count - 1; i++) {
                if (lines) content += '\n';
                content += tmp_content;
            }
        } catch (std::bad_alloc&) {
            return kError;  // TODO: more specific Result?
        }
    }
    if (IsSelectionActive()) {
        (void)after_cursor;
        return ReplaceSelection(content, lines);
    } else {
        Pos pos;
        Result res;
        Pos insert_pos;
        if (lines) {
            pos.byte_offset = 0;
            // try put cursor at the first non blank.
            // use codepoint here for convenience.
            for (char c : content) {
                if (c != '\t' && c != kSpaceChar) break;
                pos.byte_offset++;
            }
            if (after_cursor) {
                pos.line = cursor_->pos.line + 1;
                content.insert(0, 1, '\n');
                insert_pos = {cursor_->pos.line,
                              buffer_->GetLineView(cursor_->pos.line).Size()};
            } else {
                pos.line = cursor_->pos.line;
                content.append(1, '\n');
                insert_pos = {cursor_->pos.line, 0};
            }
        } else {
            insert_pos = cursor_->pos;
            if (after_cursor) {
                auto iter = buffer_->Find(insert_pos);
                if (iter != buffer_->End()) {
                    Character c;
                    auto next = NextCharacter(iter, buffer_->End(), c);
                    if (char ascii_c; c.Ascii(ascii_c) && ascii_c == '\n') {
                        insert_pos.line++;
                        insert_pos.byte_offset = 0;
                    } else {
                        insert_pos.byte_offset += next.offset() - iter.offset();
                    }
                }
            }
        }
        res = buffer_->Add(insert_pos, content, &cursor_->pos, lines, pos);
        if (res != kOk) return res;
        AfterModify(pos);
        return kOk;
    }
}

void TextArea::Cut() {
    b_view_->make_cursor_visible = true;
    if (IsSelectionActive()) {
        Range range = selection_->ToSelectRange(buffer_);
        clipboard_->SetContent(buffer_->GetContent(range),
                               selection_->LineSemantic());
        DeleteSelection();
    } else {
        // Copy a line where cursor is located.
        Range range = {{cursor_->pos.line, 0},
                       {cursor_->pos.line,
                        buffer_->GetLineView(cursor_->pos.line).Size()}};
        clipboard_->SetContent(buffer_->GetContent(range),
                               true);  // always lines

        // We try to delete a line where cursor is located.
        Pos pos;
        auto cur_pos = cursor_->pos;
        if (cursor_->pos.line == buffer_->LineCnt() - 1) {
            range.begin.line--;
            range.begin.byte_offset =
                buffer_->GetLineView(range.begin.line).Size();
        } else if (buffer_->LineCnt() != 1) {
            range.end.line++;
            range.end.byte_offset = 0;
        }
        buffer_->Delete(range, &cur_pos, false, pos);
        AfterModify(pos);
    }
}

Result TextArea::IndentSelection(size_t count) {
    CHX_ASSERT(IsSelectionActive());
    auto range = selection_->ToSelectRange(buffer_);
    size_t begin_line = range.begin.line;
    size_t end_line = range.end.byte_offset == 0 && range.end.line != 0
                          ? range.end.line - 1
                          : range.end.line;
    StopSelection();
    return IndentLines(count, begin_line, end_line);
}

Result TextArea::UnindentSelection(size_t count) {
    CHX_ASSERT(IsSelectionActive());
    auto range = selection_->ToSelectRange(buffer_);
    size_t begin_line = range.begin.line;
    size_t end_line = range.end.byte_offset == 0 && range.end.line != 0
                          ? range.end.line - 1
                          : range.end.line;
    StopSelection();
    return UnindentLines(count, begin_line, end_line);
}

Result TextArea::DeleteCharacterBeforeCursor() {
    Range range;
    if (cursor_->pos.byte_offset == 0) {
        if (cursor_->pos.line == 0) {
            return kFail;
        }
        range = {{cursor_->pos.line - 1,
                  buffer_->GetLineView(cursor_->pos.line - 1).Size()},
                 {cursor_->pos.line, 0}};
    } else {
        Character charater;
        auto line = buffer_->GetLineView(cursor_->pos.line);
        auto iter = buffer_->Find(cursor_->pos);
        iter = PrevCharacter(iter, line.begin, charater);
        range = {{cursor_->pos.line, iter.offset() - line.begin.offset()},
                 cursor_->pos};
    }
    Pos pos;
    if (Result res;
        (res = buffer_->Delete(range, nullptr, false, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result TextArea::DeleteCharacterFromCursor(size_t count) {
    CHX_ASSERT(!IsSelectionActive());
    CHX_ASSERT(count != 0);
    b_view_->make_cursor_visible = true;
    auto iter = buffer_->Find(cursor_->pos);
    auto end = buffer_->End();
    size_t i = 0;
    for (; i < count && iter != end; i++) {
        Character c;
        iter = NextCharacter(iter, end, c);
    }
    if (i == 0) {
        return kFail;
    }

    Pos pos = buffer_->OffsetToPos(iter.offset());
    Result res;
    if ((res = buffer_->Delete({cursor_->pos, pos}, &cursor_->pos, true,
                               cursor_->pos)) != kOk) {
        return res;
    }
    AfterModify(cursor_->pos);
    return kOk;
}

Result TextArea::DeleteSelection() {
    CHX_ASSERT(IsSelectionActive());
    Range r = selection_->ToDeleteRange(buffer_);
    if (r.begin == r.end) {
        return kFail;
    }

    Pos pos;
    // We try to make the cursor pos same
    bool line_semantic = selection_->LineSemantic();
    if (line_semantic) {
        size_t begin_line =
            std::min(selection_->head.line, selection_->anchor.line);
        size_t end_line =
            std::max(selection_->head.line, selection_->anchor.line);
        if (end_line == buffer_->LineCnt() - 1) {
            if (begin_line == 0) {
                pos = {0, 0};
            } else {
                CursorState state(cursor_);
                pos.line = begin_line - 1;
                CursorGoLineState(pos.line, state);
                pos.byte_offset = state.pos.byte_offset;
            }
        } else {
            CursorState state(cursor_);
            CursorGoLineState(end_line + 1, state);
            pos.line = begin_line;
            pos.byte_offset = state.pos.byte_offset;
        }
    }
    StopSelection();
    if (Result res;
        (res = buffer_->Delete(r, &cursor_->pos, line_semantic, pos)) != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result TextArea::AddStringAtCursorNoSelection(std::string_view str,
                                              const Pos* cursor_pos) {
    CHX_ASSERT(!IsSelectionActive());
    b_view_->make_cursor_visible = true;

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

Result TextArea::ReplaceSelection(std::string_view str, bool lines) {
    CHX_ASSERT(IsSelectionActive());
    b_view_->make_cursor_visible = true;

    // We need select range here because we want replace the
    // selection range.
    Range r = selection_->ToSelectRange(buffer_);
    bool line_semantic = selection_->LineSemantic();
    StopSelection();

    Pos pos;
    std::string line_str;
    if (lines) {
        // try put cursor at the first non blank.
        // Use codepoint for convenience.
        size_t blank_bytes = 0;
        for (char c : str) {
            if (c != '\t' && c != kSpaceChar) break;
            blank_bytes++;
        }
        if (line_semantic) {
            line_str.append(1, '\n');
            line_str += str;
            str = line_str;
            pos.line = r.begin.line + 1;
            pos.byte_offset = blank_bytes;
        } else {  // special case: use lines to replace lines
            pos.line = r.begin.line;
            pos.byte_offset = r.begin.byte_offset + blank_bytes;
        }
    }
    Result res = buffer_->Replace(r, str, &cursor_->pos, lines, pos);
    if (res != kOk) {
        return res;
    }
    AfterModify(pos);
    return kOk;
}

Result TextArea::IndentLines(size_t count, size_t begin_line, size_t end_line) {
    b_view_->make_cursor_visible = true;
    BufferEditBatch edit_batch;
    auto tabspace = GetOpt<bool>(kOptTabSpace);
    std::string str =
        tabspace ? std::string(count * GetOpt<int64_t>(kOptTabStop), kSpaceChar)
                 : std::string(count, '\t');
    Pos pos = cursor_->pos;
    for (size_t i = begin_line; i <= end_line; i++) {
        auto line = buffer_->GetLineView(i);
        if (line.Size() == 0) {
            continue;
        }
        if (cursor_->pos.line == i) {
            pos.byte_offset += tabspace
                                   ? str.size()
                                   : str.size() * GetOpt<int64_t>(kOptTabStop);
        }
        edit_batch.PushBack({{{i, 0}, {i, 0}}, str});
    }
    if (edit_batch.Size() == 0) {
        return kFail;
    }
    Result res = buffer_->BatchEdit(edit_batch, &cursor_->pos, true, pos);
    if (res == kOk) {
        AfterModify(pos);
    }
    return res;
}

Result TextArea::UnindentLines(size_t count, size_t begin_line,
                               size_t end_line) {
    b_view_->make_cursor_visible = true;
    BufferEditBatch edit_batch;
    Pos pos = cursor_->pos;
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    for (size_t i = begin_line; i <= end_line; i++) {
        auto line = buffer_->GetLineView(i);
        if (line.Size() == 0) {
            continue;
        }
        auto iter = IndentationEnd(count, line, tabstop);
        if (iter == line.begin) {
            continue;
        }
        Range range = {{i, 0}, {i, iter.offset() - line.begin.offset()}};
        edit_batch.PushBack({range, ""});
        if (i == cursor_->pos.line) {
            pos.byte_offset -= range.end.byte_offset;
        }
    }
    if (edit_batch.Size() == 0) {
        return kFail;
    }
    Result res = buffer_->BatchEdit(edit_batch, &cursor_->pos, true, pos);
    if (res == kOk) {
        AfterModify(pos);
    }
    return res;
}

SearchState TextArea::CursorGoSearchResultState(
    BufferSearchReplaceContext& context, bool next, size_t count,
    bool keep_current_if_one, CursorState& state) {
    if (!context.NearestSearchPos(state.pos, buffer_, next, count,
                                  keep_current_if_one)) {
        return {};
    }
    state.pos = context.search_result[context.current_search].begin;
    state.DontHoldColWant();
    return {static_cast<size_t>(context.current_search + 1),
            context.search_result.size()};
}

// Just make buffer view move.
// A little bit ugly, but just make sure we don't modify cursor, and
// make the cursor state right accroding to the buffer state.
bool TextArea::BufferViewGoSearchResult(BufferSearchReplaceContext& context,
                                        bool next, size_t count,
                                        bool keep_current_if_one) {
    Cursor c;
    b_view_->RestoreCursorState(&c, buffer_);
    CursorState state(&c);
    SearchState s = CursorGoSearchResultState(context, next, count,
                                              keep_current_if_one, state);
    if (s.total == 0) {
        b_view_->SaveCursorState(&c);
        return false;
    }

    CursorState cursor_state(cursor_);
    state.SetCursor(cursor_);
    // TODO: maybe we can pass an arg to MakeCursorVisible?
    MakeSureViewValid();
    MakeCursorVisible();
    cursor_state.SetCursor(cursor_);

    state.SetCursor(&c);
    b_view_->SaveCursorState(&c);
    return true;
}

size_t TextArea::SidebarWidth() {
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

void TextArea::DrawSidebar(int s_row, size_t absolute_line,
                           size_t sidebar_width) {
    auto line_number_type =
        static_cast<LineNumberType>(GetOpt<int64_t>(kOptLineNumber));
    if (line_number_type == LineNumberType::kNone) {
        return;
    }

    char sidebar_buf[kMaxSizeTWidth + 3 + 1];

    auto theme = GetOpt<Theme>(kOptTheme);
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
        CHX_ASSERT(false);
        line_number = 0;
    }
    std::string line_number_str = std::to_string(line_number);
    size_t left_space = sidebar_width - 1 - line_number_str.size();
    memset(sidebar_buf, kSpaceChar, left_space);
    memcpy(sidebar_buf + left_space, line_number_str.data(),
           line_number_str.size());
    sidebar_buf[left_space + line_number_str.size()] = kSpaceChar;
    sidebar_buf[left_space + line_number_str.size() + 1] = '\0';
    term_->Print(col_, s_row, theme[kSidebar], sidebar_buf);
}

Range TextArea::CalcWrapRange(size_t content_width) {
    size_t cur_b_view_line = b_view_->line;
    auto line = buffer_->GetLineView(cur_b_view_line);
    auto iter = line.begin;

    int tabstop = GetOpt<int64_t>(kOptTabStop);
    // First, we skip some sub lines.
    for (size_t i = 0; i < b_view_->subline; i++) {
        iter = ArrangeLine(line, iter, 0, content_width, tabstop, true);
    }
    size_t start_byte_offset = iter.offset() - line.begin.offset();

    for (size_t i = 0; i < height_; i++) {
        iter = ArrangeLine(line, iter, 0, content_width, tabstop, true);
        if (iter == line.end) {
            if (cur_b_view_line >= buffer_->LineCnt() - 1) {
                break;
            }
            cur_b_view_line++;
            line = buffer_->GetLineView(cur_b_view_line);
            iter = line.begin;
        }
    }
    return {{b_view_->line, start_byte_offset},
            {cur_b_view_line, iter.offset() - line.begin.offset()}};
}

void TextArea::UpdateSyntax() {
    if (parser_) parser_->ParseSyntaxAfterEdit(buffer_);
}

void TextArea::AfterModify(const Pos& cursor_pos) {
    cursor_->pos = cursor_pos;
    cursor_->DontHoldColWant();
    UpdateSyntax();
}

bool TextArea::SizeValid(size_t sidebar_width) {
    return sidebar_width < width_ && height_ > 0;
}

std::vector<Highlight> TextArea::PrepareSearchHighlight(
    BufferSearchReplaceContext* search_context) {
    std::vector<Highlight> hl;
    if (!(search_context && search_context->EnsureSearched(buffer_))) {
        return hl;
    }

    hl.reserve(search_context->search_result.size());
    // TODO: Only highlight ranges in the screen
    for (size_t i = 0; i < search_context->search_result.size(); i++) {
        hl.push_back({search_context->search_result[i],
                      search_context->current_search == static_cast<int64_t>(i)
                          ? kSearchCurrent
                          : kSearch});
    }
    return hl;
}

std::vector<Highlight> TextArea::PrepareSelectionHighlight() {
    std::vector<Highlight> hl;
    if (!IsSelectionActive()) {
        return hl;
    }

    hl.resize(1);
    hl[0].range = selection_->ToSelectRange(buffer_);
    hl[0].hl_type = kSelection;
    return hl;
}

std::tuple<std::vector<Highlight>, std::vector<int64_t>>
TextArea::PrepareTrailingBlankHighlight(const Range& render_range) {
    std::vector<Highlight> trailing_white_hl;
    std::vector<int64_t> trailing_white_begin_pre_line;
    if (!GetOpt<bool>(kOptTrailingWhite)) {
        return {trailing_white_hl, trailing_white_begin_pre_line};
    }

    size_t line_cnt = render_range.end.line -
                      static_cast<int64_t>(render_range.begin.line) + 1;
    trailing_white_begin_pre_line.reserve(line_cnt);
    for (size_t l = render_range.begin.line; l <= render_range.end.line; l++) {
        auto line = buffer_->GetLineView(l);
        int64_t i = static_cast<int64_t>(line.Size()) - 1;
        for (auto iter = line.end; i >= 0; i--) {
            iter.PrevByte();
            // Backward codepoint scan is correct and enough.
            // '\t' will break all, ' ' only may after a pretend
            // codepoint, but we render from begin so we will skip it if
            // it's wrong.
            if (iter.ThisByte() != kSpaceChar && iter.ThisByte() != '\t') {
                break;
            }
        }
        i++;
        trailing_white_begin_pre_line.push_back(i);
        if (i != static_cast<int64_t>(line.Size())) {
            trailing_white_hl.push_back(
                {{{l, static_cast<size_t>(i)}, {l, line.Size()}},
                 kTrailingWhite});
        }
    }
    return {trailing_white_hl, trailing_white_begin_pre_line};
}

void TextArea::DrawWarp(const DrawContext& context) {
    auto theme = GetOpt<Theme>(kOptTheme);
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    auto eob_mark = GetOpt<bool>(kOptEndOfBufferMark);

    // An empty sidebar
    char empty_sidebar[kMaxSizeTWidth + 3 + 1];
    memset(empty_sidebar, kSpaceChar, context.sidebar_width);
    empty_sidebar[context.sidebar_width] = '\0';

    // subline indicator sidebar
    char subline_ind_sidebar[kMaxSizeTWidth + 3 + 1];

    size_t line = context.render_range.begin.line;
    auto iter = buffer_->Find(context.render_range.begin);
    auto line_view = buffer_->GetLineView(line);
    int64_t trailing_white_begin =
        context.trailing_white_begin_pre_line.empty()
            ? line_view.Size()
            : context.trailing_white_begin_pre_line[line - context.render_range
                                                               .begin.line];

    CHX_ASSERT(line < buffer_->LineCnt());
    for (size_t i = 0; i < height_; i++) {
        if (line >= buffer_->LineCnt()) {
            if (!eob_mark) break;
            Codepoint codepoint = '~';
            term_->SetCell(context.content_s_col, i + row_, &codepoint, 1,
                           theme[kNormal]);
            line++;
            continue;
        }

        if (iter == line_view.begin) {
            DrawSidebar(row_ + i, line, context.sidebar_width);
        } else if (static_cast<LineNumberType>(GetOpt<int64_t>(
                       kOptLineNumber)) != LineNumberType::kNone) {
            // TODO: Merge this logic to DrawSideBar
            if (i == 0) {
                memset(subline_ind_sidebar, kSpaceChar, context.sidebar_width);
                size_t left_space_size =
                    context.sidebar_width - 1 - kSublineIndicator.size();
                memcpy(subline_ind_sidebar + left_space_size,
                       kSublineIndicator.data(), kSublineIndicator.size());
                // If first row is a subline, we draw a <<< at the
                // sidebar
                term_->Print(0, row_ + i, theme[kSidebar], subline_ind_sidebar);
            } else {
                term_->Print(0, row_ + i, theme[kSidebar], empty_sidebar);
            }
        }
        bool hl_cur_line_for_cursor =
            context.need_hl_cursor_line && context.cursor_line == line;
        auto fallback_attr = theme[kNormal];
        if (hl_cur_line_for_cursor) {
            if (theme[kCursorLine].fg_exist) {
                fallback_attr.fg = theme[kCursorLine].fg;
            }
            if (theme[kCursorLine].bg_exist) {
                fallback_attr.bg = theme[kCursorLine].bg;
            }
        }
        size_t end_view_col;
        std::tie(iter, end_view_col) = DrawLine(
            *term_, line, line_view, iter, 0, context.content_width, i + row_,
            context.content_s_col, &(context.highlights), theme, fallback_attr,
            trailing_white_begin, tabstop, true, hl_cur_line_for_cursor);
        if (iter == line_view.end) {
            if (IsSelectionActive() && end_view_col < context.content_width &&
                context.selection_hl[0].range.PosInMe(
                    {line, line_view.Size()})) {
                // cursor_line don't hl if selection is active, so just use
                // kSelection is ok
                term_->SetCell(context.content_s_col + end_view_col, i + row_,
                               &kSpaceChar, 1, theme[kSelection]);
            }
            line++;
            if (line < buffer_->LineCnt()) {
                line_view = buffer_->GetLineView(line);
                iter = line_view.begin;
                trailing_white_begin =
                    context.trailing_white_begin_pre_line.empty()
                        ? line_view.Size()
                        : context.trailing_white_begin_pre_line
                              [line - context.render_range.begin.line];
            }
        }
    }
}
void TextArea::DrawNoWarp(const DrawContext& context) {
    auto theme = GetOpt<Theme>(kOptTheme);
    auto tabstop = GetOpt<int64_t>(kOptTabStop);
    auto eob_mark = GetOpt<bool>(kOptEndOfBufferMark);

    const size_t line_cnt = buffer_->LineCnt();
    for (size_t win_r = 0; win_r < height_; win_r++) {
        int cur_s_row = win_r + row_;
        size_t line = win_r + b_view_->line;

        if (line >= line_cnt) {
            if (!eob_mark) break;
            Codepoint codepoint = '~';
            term_->SetCell(context.content_s_col, cur_s_row, &codepoint, 1,
                           theme[kNormal]);
            continue;
        }
        DrawSidebar(cur_s_row, line, context.sidebar_width);
        auto line_view = buffer_->GetLineView(line);
        int64_t trailing_white_begin =
            context.trailing_white_begin_pre_line.empty()
                ? line_view.Size()
                : context.trailing_white_begin_pre_line
                      [line - context.render_range.begin.line];
        bool hl_cur_line_for_cursor =
            context.need_hl_cursor_line && context.cursor_line == line;
        auto fallback_attr = theme[kNormal];
        if (hl_cur_line_for_cursor) {
            if (theme[kCursorLine].fg_exist) {
                fallback_attr.fg = theme[kCursorLine].fg;
            }
            if (theme[kCursorLine].bg_exist) {
                fallback_attr.bg = theme[kCursorLine].bg;
            }
        }
        auto [iter, end_view_col] = DrawLine(
            *term_, line, line_view, line_view.begin, b_view_->col,
            context.content_width, cur_s_row, context.content_s_col,
            &context.highlights, theme, fallback_attr, trailing_white_begin,
            tabstop, false, hl_cur_line_for_cursor);
        if (IsSelectionActive() && iter == line_view.end &&
            end_view_col - b_view_->col < context.content_width &&
            context.selection_hl[0].range.PosInMe({line, line_view.Size()})) {
            // cursor_line don't hl if selection is active, so just use
            // kSelection is ok
            term_->SetCell(context.content_s_col + end_view_col - b_view_->col,
                           cur_s_row, &kSpaceChar, 1, theme[kSelection]);
        }
    }
}

}  // namespace charxed
