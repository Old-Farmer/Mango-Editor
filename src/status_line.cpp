#include "status_line.h"

#include <sstream>

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
        b = cursor_->restore_from_peel->frame_.buffer_;
    } else {
        b = cursor_->in_window->frame_.buffer_;
    }
    std::string cursor_in_info =
        (b->path().Empty() ? std::string("[new file]")
                           : std::string(b->path().ThisPath().data(),
                                         b->path().ThisPath().size())) +
        BufferStateString[static_cast<int>(b->state())];

    auto sep_width = global_opts_->GetOpt<int64_t>(kOptStatusLineSepWidth);

    Result res =
        term_->Print(sep_width, row_, scheme[t], cursor_in_info.c_str());
    if (res == kTermOutOfBounds) {
        ;
    }

    std::stringstream ss;
    int64_t line, character_in_line;
    if (IsPeel(*mode_)) {
        line = cursor_->restore_from_peel->frame_.buffer_->cursor_state_line();
        character_in_line = cursor_->restore_from_peel->frame_.buffer_
                                ->cursor_state_character_in_line();
    } else {
        line = cursor_->line;
        character_in_line = cursor_->character_in_line;
    }

    std::string sep(sep_width, kSpaceChar);
    ss << sep << line << "," << character_in_line << sep
       << FiletypeStrRep(b->filetype()) << sep
       << (b->opts().GetOpt<bool>(kOptTabSpace) ? "Spaces:" : "Tab:")
       << b->opts().GetOpt<int64_t>(kOptTabStop) << sep << b->eol_seq();
    // all is ascii character, so str len == width
    res = term_->Print(width_ - ss.str().length() - sep_width, row_, scheme[t],
                       ss.str().c_str());
    if (res == kTermOutOfBounds) {
        ;
    }
}

}  // namespace mango
