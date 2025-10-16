#include "status_line.h"

#include <sstream>

#include "buffer.h"
#include "coding.h"
#include "cursor.h"
#include "options.h"
#include "window.h"

namespace mango {

StatusLine::StatusLine(Cursor* cursor, Options* options)
    : cursor_(cursor), options_(options) {}

void StatusLine::Draw() {
    CharacterType t = kReverse;

    term_->Print(0, row_, options_->attr_table[t],
                 std::string(width_, kSpaceChar).c_str());

    std::string cursor_in_info;
    if (cursor_->in_window != nullptr) {
        Buffer* b = cursor_->in_window->frame_.buffer_;
        cursor_in_info = (b->path().empty() ? "[new file]" : b->path()) +
                         BufferStateString[static_cast<int>(b->state())];
    } else {
        cursor_in_info = "[Mango Peel]";
    }

    Result res = term_->Print(options_->status_line_left_indent, row_,
                              options_->attr_table[t], cursor_in_info.c_str());
    if (res == kTermOutOfBounds) {
        ;
    }

    std::stringstream ss;
    ss << "  " << cursor_->line << "," << cursor_->character_in_line;
    // all is ascii character, so str len == width
    res = term_->Print(
        width_ - ss.str().length() - options_->status_line_right_indent, row_,
        options_->attr_table[t], ss.str().c_str());
    if (res == kTermOutOfBounds) {
        ;
    }
}

}  // namespace mango
