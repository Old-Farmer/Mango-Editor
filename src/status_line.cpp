#include "status_line.h"

#include <sstream>

#include "buffer.h"
#include "coding.h"
#include "cursor.h"
#include "options.h"
#include "window.h"

namespace mango {

StatusLine::StatusLine(Cursor* cursor, Options* options, Mode* mode)
    : cursor_(cursor), options_(options), mode_(mode) {}

void StatusLine::Draw() {
    CharacterType t = kReverse;

    term_->Print(0, row_, options_->attr_table[t],
                 std::string(width_, kSpaceChar).c_str());

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

    Result res = term_->Print(options_->status_line_left_indent, row_,
                              options_->attr_table[t], cursor_in_info.c_str());
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
    ss << "  " << line << "," << character_in_line;
    // all is ascii character, so str len == width
    res = term_->Print(
        width_ - ss.str().length() - options_->status_line_right_indent, row_,
        options_->attr_table[t], ss.str().c_str());
    if (res == kTermOutOfBounds) {
        ;
    }
}

}  // namespace mango
