#include "layout_manager.h"

#include "mango_peel.h"
#include "status_line.h"
#include "window.h"

namespace mango {

static constexpr size_t kStatusLineHeight = 1;

LayoutManager::LayoutManager(Window* window, StatusLine* status_line,
                             MangoPeel* peel)
    : window_(window), status_line_(status_line), peel_(peel) {}

void LayoutManager::EnsureLayout() {
    size_t h = peel_->NeedHeight(term_->Width());
    if (peel_->buffer_.version() == peel_buffer_version_ || peel_height_ == h) {
        return;
    }
    ArrangeLayoutInner(h);
}

void LayoutManager::ArrangeLayout() {
    size_t h = peel_->NeedHeight(term_->Width());
    ArrangeLayoutInner(h);
}

void LayoutManager::ArrangeLayoutInner(size_t peel_need_height) {
    size_t width = term_->Width();
    size_t height = term_->Height();

    size_t peel_real_height = std::min(
        peel_need_height, height % 2 == 0 ? height / 2 - 1 : height / 2);

    size_t window_height = height - kStatusLineHeight - peel_real_height;
    window_->area_.col_ = 0;
    window_->area_.row_ = 0;
    window_->area_.width_ = width;
    window_->area_.height_ = window_height;

    status_line_->row_ = window_height;
    status_line_->width_ = width;

    peel_->area_.row_ = height - peel_real_height;
    peel_->area_.width_ = width;
    peel_->area_.height_ = peel_real_height;

    peel_height_ = peel_need_height;
    peel_buffer_version_ = peel_->buffer_.version();
}

}  // namespace mango
