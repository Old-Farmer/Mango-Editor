#include "status_line.h"

#include <sstream>

#include "term.h"

namespace mango {

StatusLine::StatusLine(Cursor* cursor, Options* options)
    : cursor_(cursor), attr_table(&options->attr_table) {}

void StatusLine::Draw() {
    Buffer* b = cursor_->in_window->buffer_;
    std::string file_and_state =
        (b->path().empty() ? "[new file]" : b->path()) +
        BufferStateString[static_cast<int>(b->state())];
    Result res =
        term_->Print(2, row_, (*attr_table)[kNormal], file_and_state.c_str());
    if (res == kTermOutOfBounds) {
        ;
    }

    std::stringstream ss;
    ss << "  " << cursor_->line << "," << cursor_->character_in_line;
    // all is ascii character, so str len == width
    res = term_->Print(width_ - ss.str().length() - 5, row_,
                       (*attr_table)[kNormal], ss.str().c_str());
    if (res == kTermOutOfBounds) {
        ;
    }
}

}  // namespace mango
