#include "status_line.h"

#include "buffer.h"
#include "character.h"
#include "cursor.h"
#include "filetype.h"
#include "options.h"
#include "window.h"

namespace mango {

StatusLine::StatusLine(Cursor* cursor, GlobalOpts* global_opts, Mode* mode)
    : cursor_(cursor), global_opts_(global_opts), mode_(mode) {}

void StatusLine::Draw() {
    ColorSchemeType t = kStatusLine;

    auto scheme = global_opts_->GetOpt<ColorScheme>(kOptColorScheme);

    // make this line reverse
    term_->Print(0, row_, scheme[t], std::string(width_, kSpaceChar).c_str());

    Buffer* b;
    if (IsPeel(*mode_)) {
        b = cursor_->restore_from_peel->area_.buffer_;
    } else {
        b = cursor_->in_window->area_.buffer_;
    }
    std::string left_str;
    left_str = fmt::format("{:<" MGO_VIM_MODE_WIDTH "} {}{}",
                           kModeString[static_cast<int>(*mode_)], b->Name(),
                           kBufferStateString[static_cast<int>(b->state())]);

    term_->Print(0, row_, scheme[t], left_str.c_str());

    int64_t line, character_in_line;
    if (IsPeel(*mode_)) {
        line = cursor_->restore_from_peel->area_.b_view_->cursor_state.pos.line;
        character_in_line = cursor_->restore_from_peel->area_.b_view_
                                ->cursor_state.character_in_line_;
    } else {
        line = cursor_->pos.line;
        character_in_line = cursor_->character_in_line;
    }

    std::string right_str =
        fmt::format("  {},{}  {}  {}{}  {}", line + 1, character_in_line + 1,
                    FiletypeStrRep(b->filetype()),
                    b->opts().GetOpt<bool>(kOptTabSpace) ? "[s]:" : "[t]:",
                    b->opts().GetOpt<int64_t>(kOptTabStop), b->eol_seq());
    // all is ascii character, so str len == width
    term_->Print(width_ - right_str.length(), row_, scheme[t],
                 right_str.c_str());
}

}  // namespace mango
