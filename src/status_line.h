#pragma once

#include "term.h"
#include "utils.h"

namespace mango {

class Cursor;
class Options;
enum class Mode;

// the status line stay above the mango peel.
// Only one row.
class StatusLine {
   public:
    StatusLine(Cursor* cursor, Options* options, Mode* mode);
    ~StatusLine() = default;
    MGO_DELETE_COPY(StatusLine);
    MGO_DEFAULT_MOVE(StatusLine);

    void Draw();

   public:
    size_t width_ = 0;
    size_t row_ = 0;  // top left corner x related to the whole screen

   private:
    Cursor* cursor_;
    Options* options_;
    Mode* mode_;

    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
