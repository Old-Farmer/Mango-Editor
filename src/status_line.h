#pragma once

#include "utils.h"
#include "window.h"

namespace mango {

// the status line fix in the bottom of the window
class StatusLine {
   public:
    StatusLine(Cursor* cursor, Options* options);
    ~StatusLine() = default;
    MANGO_DELETE_COPY(StatusLine);
    MANGO_DEFAULT_MOVE(StatusLine);

    void Draw();

   public:
    int width_ = 0;
    int row_ = 0;  // top left corner x related to the whole screen

   private:
    Cursor* cursor_;
    std::vector<Terminal::AttrPair>* attr_table = nullptr;

    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango
