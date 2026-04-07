#pragma once

#include "term.h"

namespace mango {

class Window;
class StatusLine;
class MangoPeel;

class LayoutManager {
   public:
    LayoutManager(Window* window, StatusLine* status_line, MangoPeel* peel);

    void EnsureLayout();
    void ArrangeLayout();

   private:
    void ArrangeLayoutInner(size_t peel_need_height);

   private:
    Window* window_;
    StatusLine* status_line_;
    MangoPeel* peel_;
    Terminal* term_ = &Terminal::GetInstance();

    size_t peel_height_;
    int64_t peel_buffer_version_;
};

}  // namespace mango
