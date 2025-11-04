#include "cmp_menu.h"

#include "character.h"
#include "cursor.h"
#include "options.h"

namespace mango {

CmpMenu::CmpMenu(Cursor* cursor, Options* options)
    : options_(options), cursor_(cursor) {}

void CmpMenu::SetEntries(std::vector<std::string> entries) {
    entries_ = std::move(entries);
    menu_cursor_ = 0;
    menu_view_line_ = 0;
}

void CmpMenu::DecideLocAndSize() {
    assert(entries_.size() > menu_view_line_);
    size_t shown_enties_cnt = std::min(options_->cmp_menu_max_height,
                                       entries_.size() - menu_view_line_);

    // Decide the menu above or below the cursor?
    // Prefer below cursor.
    // TODO: CmpMenu can be everywhere, is it ok?
    bool below_cursor = false;
    if (term_->Height() >= cursor_->s_row + 1 + shown_enties_cnt) {
        // the space below cursor is bigger than the place above cursor
        below_cursor = true;
    }

    // Decide height, row, col
    if (below_cursor) {
        height_ =
            std::min(term_->Height() - (cursor_->s_row + 1), shown_enties_cnt);
        row_ = cursor_->s_row + 1;
    } else {
        height_ = std::min<size_t>(cursor_->s_row, shown_enties_cnt);
        row_ = cursor_->s_row - height_;
    }
    col_ = cursor_->s_col;
    assert(row_ < term_->Height());
    assert(col_ < term_->Width());

    // Decide width
    entries_width_.resize(height_);
    size_t max_width = 0;
    for (size_t i = menu_view_line_; i < shown_enties_cnt + menu_view_line_;
         i++) {
        size_t w = Terminal::StringWidth(entries_[i]);
        entries_width_[i - menu_view_line_] = w;
        max_width = std::max(w, max_width);
    }
    // should not be zero
    assert(max_width != 0);
    width_ = std::min(std::min(max_width, term_->Width() - col_),
                      options_->cmp_menu_max_width);
}

void CmpMenu::Draw() {
    if (!visible_) {
        return;
    }

    DecideLocAndSize();

    MANGO_LOG_DEBUG("cmp height %zu", height_);
    for (size_t r = 0; r < height_; r++) {
        const Terminal::AttrPair& attr = r + menu_view_line_ == menu_cursor_
                                             ? options_->attr_table[kSelection]
                                             : options_->attr_table[kMenu];

        const std::string& str = entries_[menu_view_line_ + r];
        size_t offset = 0;
        size_t menu_col = 0;
        std::vector<uint32_t> character;
        while (offset < str.size()) {
            int character_width;
            int byte_len;
            Result res = NextCharacterInUtf8(str, offset, character, byte_len,
                                             character_width);
            assert(res == kOk);
            // TODO: use entries_width
            // for show ... when space is not enough for long width entries
            if (menu_col + character_width <= width_) {
                Result res =
                    term_->SetCell(menu_col + col_, r + row_, character.data(),
                                   character.size(), attr);
                if (res == kTermOutOfBounds) {
                    // User resize the screen now, just skip the
                    // left cols in this row
                    break;
                }
            } else {
                break;
            }
            menu_col += character_width;
            offset += byte_len;
        }
        // make paddings because menu have different bg color
        while (menu_col < width_) {
            uint32_t space = kSpaceChar;
            Result res =
                term_->SetCell(menu_col + col_, r + row_, &space, 1, attr);
            if (res == kTermOutOfBounds) {
                // User resize the screen now, just skip the
                // left cols in this row
                break;
            }
            menu_col++;
        }
    }
    MANGO_LOG_DEBUG("cmp draw finish");
}

void CmpMenu::SelectNext() {
    if (menu_cursor_ == entries_.size() - 1) {
        menu_cursor_ = 0;
        return;
    }
    menu_cursor_++;
    if (menu_cursor_ >= menu_view_line_ + height_) {
        menu_view_line_ = menu_cursor_ - height_ + 1;
    }
}

void CmpMenu::SelectPrev() {
    if (menu_cursor_ == 0) {
        menu_cursor_ = entries_.size() - 1;
        return;
    }
    menu_cursor_--;
    if (menu_cursor_ < menu_view_line_) {
        menu_view_line_ = menu_cursor_;
    }
}

size_t CmpMenu::Accept() {
    auto ret = menu_cursor_;
    menu_cursor_ = 0;
    entries_.clear();
    entries_width_.clear();
    visible_ = false;
    return ret;
}

void CmpMenu::Clear() {
    menu_cursor_ = 0;
    entries_.clear();
    entries_width_.clear();
    visible_ = false;
}

}  // namespace mango