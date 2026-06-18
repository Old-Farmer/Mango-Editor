#pragma once

#include <vector>

#include "cursor.h"
#include "draw.h"
#include "options.h"
#include "search.h"

namespace charxed {

// A UI only class that renders a List.
template <typename T>
class ListArea {
   public:
    using ListEntryStrForRenderFunc =
        std::function<std::tuple<std::string_view, size_t>(const T&,
                                                           std::string&)>;

    ListArea(GlobalOpts* global_opts, Cursor* cursor,
             const std::vector<T>* list, int64_t* list_version,
             const ListEntryStrForRenderFunc& func)
        : list_(list),
          list_version_(list_version),
          cursor_(cursor),
          list_entry_str_for_render_(func),
          global_opts_(global_opts) {}
    CHX_DELETE_COPY(ListArea);
    CHX_DEFAULT_MOVE(ListArea);
    ~ListArea() = default;

    void Draw(bool highlight_search) {
        auto theme = GetOpt<Theme>(kOptTheme);
        auto hl_seach = GetOpt<bool>(kOptHighlightOnSearch) && highlight_search;

        std::string buf;
        std::vector<const std::vector<Highlight>*> highlights;
        std::vector<Highlight> search_highlight;

        int64_t search_i;
        if (hl_seach) {
            if (!search_context_.EnsureSearched(*list_, *list_version_)) {
                hl_seach = false;
            } else {
                search_i = std::lower_bound(
                               search_context_.search_result.begin(),
                               search_context_.search_result.end(), view_begin_,
                               [](const ListEntrySearchResult& r,
                                  size_t index) { return r.index < index; }) -
                           search_context_.search_result.begin();
                highlights.push_back(&search_highlight);
                search_highlight.emplace_back();
            }
        }

        for (size_t i = 0; i < height_; i++) {
            if (list_->size() <= view_begin_ + i) {
                break;
            }
            size_t row = row_ + i;
            auto& e = (*list_)[view_begin_ + i];
            auto [str, real_content_begin] = list_entry_str_for_render_(e, buf);

            bool list_current_highlight =
                view_begin_ + i ==
                (stored_cursor_ == -1 ? cursor_->pos.line : stored_cursor_);
            ThemeElement fallback_attr = theme[kNormal];
            if (list_current_highlight) {
                theme[kListCurrent].MergeTo(fallback_attr);
            }
            bool hl_seach_this_line = false;
            if (hl_seach &&
                search_i < static_cast<int64_t>(
                               search_context_.search_result.size()) &&
                view_begin_ + i ==
                    search_context_.search_result[search_i].index) {
                search_highlight.back().hl_type =
                    search_i == search_context_.current_search ? kSearchCurrent
                                                               : kSearch;
                search_highlight.back().range = {
                    {0, search_context_.search_result[search_i]
                                .range.offset_begin +
                            real_content_begin},
                    {0,
                     search_context_.search_result[search_i].range.offset_end +
                         real_content_begin}};
                search_i++;
                hl_seach_this_line = true;
            }
            DrawLine(*term_, str, {0, 0}, 0, width_, row, col_,
                     hl_seach_this_line ? &highlights : nullptr, theme,
                     fallback_attr, INT64_MAX, 0, false,
                     list_current_highlight);
        }
    }
    void MakeCursorVisible() {
        if (!make_cursor_visible_) {
            return;
        }

        // Ref TextArea code

        size_t scroll_off = GetOpt<int64_t>(kOptScrollOff);
        size_t top_scroll_off;
        size_t bottom_scroll_off;
        if (scroll_off * 2 + 1 <= height_) {
            top_scroll_off = bottom_scroll_off = scroll_off;
        } else {
            CHX_ASSERT(height_ >= 1);
            top_scroll_off = (height_ - 1) / 2;
            bottom_scroll_off = (height_ - 1) - top_scroll_off;
        }

        if (cursor_->pos.line < view_begin_ + top_scroll_off) {
            view_begin_ = cursor_->pos.line > top_scroll_off
                              ? cursor_->pos.line - top_scroll_off
                              : 0;
        } else if (cursor_->pos.line - view_begin_ >=
                   height_ - bottom_scroll_off) {
            view_begin_ = cursor_->pos.line + 1 - height_ + bottom_scroll_off;
        }
    }

    bool In(size_t s_col, size_t s_row) {
        return s_col >= col_ && s_col < s_col + width_ && s_row >= row_ &&
               s_row < row_ + height_;
    }

    void SetCursorHint(size_t s_row, size_t s_col) {
        (void)s_col;
        make_cursor_visible_ = true;
        size_t flatten_index = s_row - row_ + view_begin_;
        cursor_->pos.line =
            flatten_index >= list_->size() ? list_->size() - 1 : flatten_index;
    }

    void CursorGoDown(size_t count) {
        CHX_ASSERT(count != 0);
        make_cursor_visible_ = true;
        cursor_->pos.line =
            std::min(count + cursor_->pos.line, list_->size() - 1);
    }
    void CursorGoUp(size_t count) {
        CHX_ASSERT(count != 0);
        make_cursor_visible_ = true;
        cursor_->pos.line =
            count > cursor_->pos.line ? 0 : cursor_->pos.line - count;
    }

    void ScrollRows(int64_t count) {
        make_cursor_visible_ = false;
        if (count > 0) {
            view_begin_ = std::min(view_begin_ + count, list_->size() - 1);
        } else {
            view_begin_ =
                std::max<int64_t>(static_cast<int64_t>(view_begin_) + count,
                                  0);  // cast is necessary here
        }
    }
    void ScrollCols(int64_t count) { (void)count; }

    void SaveView() {
        stored_view_begin_ = view_begin_;
        stored_cursor_ = cursor_->pos.line;
    }
    void RestoreView() {
        view_begin_ = stored_view_begin_;
        cursor_->pos.line = stored_cursor_;
        stored_cursor_ = -1;
        cursor_->SetScreenPos(-1, -1);
    }

    void BuildSearchContext(const std::string& pattern) {
        search_context_ = ListSearchContext<T>{pattern, *list_, *list_version_};
    }
    void DestorySearchContext() { search_context_.Destroy(); }
    const std::string& GetSearchPattern() {
        return search_context_.search_pattern;
    }
    SearchState CursorGoSearchResult(bool next, size_t count,
                                     bool keep_current_if_one) {
        if (!search_context_.NearestSearchPos(cursor_->pos.line, *list_,
                                              *list_version_, next, count,
                                              keep_current_if_one)) {
            return {};
        }
        cursor_->pos.line =
            search_context_.search_result[search_context_.current_search].index;
        return {static_cast<size_t>(search_context_.current_search) + 1,
                search_context_.search_result.size()};
    }

    // TODO: TextWindow ViewGoSearchResult and this ViewGoSearchResult hehavior
    // are diffrent, align to one the them.
    bool ViewGoSearchResult(bool next, size_t count, bool keep_current_if_one) {
        if (!search_context_.NearestSearchPos(stored_cursor_, *list_,
                                              *list_version_, next, count,
                                              keep_current_if_one)) {
            return false;
        }
        size_t old_index = cursor_->pos.line;
        cursor_->pos.line =
            search_context_.search_result[search_context_.current_search].index;
        MakeCursorVisible();
        cursor_->pos.line = old_index;
        return true;
    }

    template <typename KeyT>
    KeyT GetOpt(OptKey key) {
        return global_opts_->GetOpt<KeyT>(key);
    }

    const std::vector<T>* list_;
    const int64_t* list_version_;

    ListSearchContext<T> search_context_;

    Cursor* cursor_;
    Terminal* term_ = &Terminal::GetInstance();

    int64_t stored_view_begin_ = 0;
    int64_t stored_cursor_ = 0;

    ListEntryStrForRenderFunc list_entry_str_for_render_;

    GlobalOpts* global_opts_;

   public:
    bool make_cursor_visible_ = true;
    size_t view_begin_ = 0;

    size_t width_ = 0;
    size_t height_ = 0;
    size_t row_ = 0;
    size_t col_ = 0;
};
}  // namespace charxed
