#pragma once

#include <vector>

#include "term.h"
#include "utils.h"
namespace mango {

class Options;
class Cursor;

// CmpMenu is a menu that can show some entries. You can select and accept one.
// CmpMenu when draw itself just below or above the cursor.
// It will adjust its width & height & loc when Draw() is called.
// Make sure that cursor's s_col & s_row have already been calculated.
class CmpMenu {
   public:
    CmpMenu(Cursor* cursor, Options* options);
    ~CmpMenu() = default;

    MGO_DELETE_COPY(CmpMenu);
    MGO_DELETE_MOVE(CmpMenu);

    void SetEntries(std::vector<std::string> entries);

    void Draw();
    void SelectNext();
    void SelectPrev();
    size_t Accept();
    void Clear();

    size_t size() const noexcept { return entries_.size(); }
    bool& visible() noexcept { return visible_; }

   private:
    void DecideLocAndSize();

    size_t col_;
    size_t row_;

    size_t width_;
    size_t height_;

    std::vector<std::string> entries_;
    std::vector<size_t> entries_width_;  // from menu_view_line, just record the
                                         // DecideLocAndSize result
    size_t menu_cursor_ = 0;
    size_t menu_view_line_ = 0;  // which entry at the first line of menu

    bool visible_ = false;

    Options* options_;
    Cursor* cursor_;
    Terminal* term_ = &Terminal::GetInstance();
};

}  // namespace mango