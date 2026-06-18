#pragma once

#include <gsl/span>
#include <memory>
#include <string>
#include <vector>

#include "fs.h"
#include "list_area.h"
#include "options.h"
#include "utils.h"
#include "window.h"

namespace charxed {

class GlobalOpts;
struct Cursor;
enum class Context;
class BufferManager;
class LayoutManager;

// Explorer is a file tree explorer
// Explorer' root is now always be Cwd.
// so all paths in explorer will be relative path, except cwd.
class Explorer : public Window {
   public:
    Explorer(GlobalOpts* global_opts, Cursor* cursor, Context* context,
             BufferManager* buffer_manager);
    virtual ~Explorer() = default;
    CHX_DELETE_COPY(Explorer);
    CHX_DEFAULT_MOVE(Explorer);

    void Init(LayoutManager* layout_manager);

    void MakeCursorVisible() { area_.MakeCursorVisible(); }

    void Draw(bool highlight_search) { area_.Draw(highlight_search); }

    // throws FSException if expand dir entry error
    void EnterCurrentEntry();

    // return absolute path if it's cwd,
    // otherwise return relative path.
    std::string CurrentEntryPath();

    bool IsCurrentEntryDir();

    void CursorGoDown(size_t count) override { area_.CursorGoDown(count); }
    void CursorGoUp(size_t count) override { area_.CursorGoUp(count); }

    void CursorGoHalfPageUp(size_t count) override {
        area_.CursorGoUp(count * area_.height_ / 2);
    }
    void CursorGoHalfPageDown(size_t count) override {
        area_.CursorGoDown(count * area_.height_ / 2);
    }
    void CursorGoPageUp(size_t count) override {
        area_.CursorGoUp(count * area_.height_);
    }
    void CursorGoPageDown(size_t count) override {
        area_.CursorGoDown(count * area_.height_);
    }

    bool In(size_t s_col, size_t s_row) { return area_.In(s_col, s_row); }

    void SetCursorHint(size_t s_row, size_t s_col) override {
        area_.SetCursorHint(s_row, s_col);
    }

    Result DoubleClick() override;

    void ScrollRows(int64_t count) override { area_.ScrollRows(count); }
    void ScrollCols(int64_t count) override { area_.ScrollCols(count); }

    void SaveView() override { area_.SaveView(); }
    void RestoreView() override { area_.RestoreView(); }

    void BuildSearchContext(const std::string& pattern) override {
        area_.BuildSearchContext(pattern);
    }
    void DestorySearchContext() { area_.DestorySearchContext(); }
    const std::string& GetSearchPattern() override {
        return area_.GetSearchPattern();
    }
    SearchState CursorGoSearchResult(bool next, size_t count,
                                     bool keep_current_if_one) override {
        return area_.CursorGoSearchResult(next, count, keep_current_if_one);
    }
    bool ViewGoSearchResult(bool next, size_t count,
                            bool keep_current_if_one) override {
        return area_.ViewGoSearchResult(next, count, keep_current_if_one);
    }

   private:
    struct Entry;
    // return absolute path if it's cwd,
    // otherwise return relative path.
    std::string EntryPath(const Entry* e);

    // Expand collapsed flatten_index DirEntry
    // return expanded count.
    // throw FSException
    // NOTE: should fix list_area view_begin and cursor_ if necessary.
    size_t ExpandDirEntry(size_t flattern_index);

    // Current Entry must be DirEntry
    void ToggleCurrentDirEntryExpansion();
    void ExpandCurrentDirEntry();
    void CollapseCurrentDirEntry();

    std::tuple<std::string_view, size_t> EntryStrForRender(const Entry* e,
                                                           std::string& buf);

    // A special sort function that makes entries arranged clearly in users'
    // perspecitve.
    static void SortEntries(std::vector<std::unique_ptr<Entry>>& entries);

    struct DirEntry;
    struct Entry {
        std::string name;
        DirEntry* parent;
        size_t depth;

        bool IsDir() const {
            CHX_ASSERT(!name.empty());
            return name.back() == kPathSeperator;
        }

        // For search
        std::string_view Str() { return name; }

        bool operator<(const Entry& other) noexcept {
            return name < other.name;
        }
    };

    struct DirEntry : Entry {
        std::vector<std::unique_ptr<Entry>>
            entries;  // In ascending order by name
        // size_t recursive_entry_size;
        bool expanded = false;
    };

    template <typename T>
    T GetOpt(OptKey key) {
        return global_opts_->GetOpt<T>(key);
    }

    DirEntry root_;
    std::vector<Entry*> flattern_entries_ = {&root_};
    int64_t tree_version_ = 0;

    Cursor* cursor_;
    Context* context_;
    LayoutManager* layout_manager_;
    BufferManager* buffer_manager_;

    GlobalOpts* global_opts_;

   public:
    ListArea<Entry*> area_;
};

}  // namespace charxed
