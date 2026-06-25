#include "explorer.h"

#include "buffer_manager.h"
#include "layout_manager.h"
#include "text_window.h"

namespace charxed {

namespace {
// NOTE: one cell wide
constexpr char kDirExpandedIndicator = '-';
constexpr char kDirCollapsedIndicator = '+';
};  // namespace

Explorer::Explorer(GlobalOpts* global_opts, Cursor* cursor, Context* context,
                   BufferManager* buffer_manager)
    : cursor_(cursor),
      context_(context),
      buffer_manager_(buffer_manager),
      global_opts_(global_opts),
      area_(global_opts, cursor, &flattern_entries_, &tree_version_,
            [this](Entry* const& e,
                   std::string& buf) -> std::tuple<std::string_view, size_t> {
                return EntryStrForRender(e, buf);
            }) {
    root_ = {{Path::GetCwd(), nullptr, 0}, {}, false};
    // TODO: lazy expand?
    try {
        ExpandDirEntry(0);
    } catch (FSException& e) {
        CHX_LOG_ERROR("expand dir entry error: {}", e.what());
    }
}

void Explorer::Init(LayoutManager* layout_manager) {
    layout_manager_ = layout_manager;
}

Result Explorer::DoubleClick() {
    EnterCurrentEntry();
    return kOk;
}

void Explorer::EnterCurrentEntry() {
    if (IsCurrentEntryDir()) {
        ToggleCurrentDirEntryExpansion();
    } else {
        auto name = CurrentEntryPath();

        // == Editor->QuitExplorer()
        SaveView();
        *context_ = Context::kEditor;
        cursor_->focused = cursor_->t_win;
        layout_manager_->ArrangeLayout();

        Buffer* b = buffer_manager_->FindBuffer(name);
        if (!b) {
            b = buffer_manager_->AddBuffer(Buffer(global_opts_, name));
        }
        cursor_->t_win->AttachBuffer(b);
    }
}

std::string Explorer::CurrentEntryPath() {
    return EntryPath(flattern_entries_[cursor_->pos.line]);
}

bool Explorer::IsCurrentEntryDir() {
    return flattern_entries_[cursor_->pos.line]->IsDir();
}

void Explorer::Refresh() {}

std::string Explorer::EntryPath(const Entry* e) {
    if (e->parent == nullptr) {
        return e->name;
    }

    std::string path(e->name);
    while (e->parent && e->parent != &root_) {
        path.insert(0, e->parent->name);
        e = e->parent;
    }
    return path;
}

size_t Explorer::ExpandDirEntry(size_t flattern_index) {
    CHX_ASSERT(flattern_index < flattern_entries_.size());
    CHX_ASSERT(flattern_entries_[flattern_index]->IsDir());
    CHX_ASSERT(
        !static_cast<DirEntry*>(flattern_entries_[flattern_index])->expanded);

    auto path = EntryPath(flattern_entries_[flattern_index]);
    auto entries = ListUnderDirectory(path);
    auto dir_e = static_cast<DirEntry*>(flattern_entries_[flattern_index]);
    dir_e->entries.reserve(entries.size());
    for (auto& name : entries) {
        CHX_ASSERT(!name.empty());
        std::unique_ptr<Entry> e;
        if (name.back() == kPathSeperator) {
            e = std::make_unique<DirEntry>();
        } else {
            e = std::make_unique<Entry>();
        }
        e->name = std::move(name);
        e->parent = dir_e;
        e->depth = dir_e->depth + 1;
        dir_e->entries.push_back(std::move(e));
    }
    dir_e->expanded = true;
    SortEntries(dir_e->entries);
    std::vector<Entry*> tmp;
    tmp.reserve(dir_e->entries.size());
    for (auto& e : dir_e->entries) {
        tmp.push_back(e.get());
    }
    flattern_entries_.insert(flattern_entries_.begin() + flattern_index + 1,
                             tmp.begin(), tmp.end());
    return tmp.size();
}

void Explorer::ToggleCurrentDirEntryExpansion() {
    CHX_ASSERT(IsCurrentEntryDir());
    if (static_cast<DirEntry*>(flattern_entries_[cursor_->pos.line])
            ->expanded) {
        CollapseCurrentDirEntry();
    } else {
        ExpandCurrentDirEntry();
    }
}

void Explorer::ExpandCurrentDirEntry() { ExpandDirEntry(cursor_->pos.line); }

void Explorer::CollapseCurrentDirEntry() {
    CHX_ASSERT(IsCurrentEntryDir());
    auto dir_e = static_cast<DirEntry*>(flattern_entries_[cursor_->pos.line]);
    flattern_entries_.erase(flattern_entries_.begin() + cursor_->pos.line + 1,
                            flattern_entries_.begin() + cursor_->pos.line + 1 +
                                dir_e->entries.size());
    dir_e->entries.clear();
    dir_e->expanded = false;
}

std::tuple<std::string_view, size_t> Explorer::EntryStrForRender(
    const Entry* e, std::string& buf) {
    auto indent = GetOpt<int64_t>(kOptExplorerIndent);
    auto indent_width = indent * (e->depth + 1);
    buf.clear();
    buf.append(indent_width, kSpaceChar);
    if (e->IsDir()) {
        buf[indent_width - indent] = static_cast<const DirEntry*>(e)->expanded
                                         ? kDirExpandedIndicator
                                         : kDirCollapsedIndicator;
    }
    buf.append(e->name);
    return {buf, indent_width};
}

// Entries is first divided in 2 parts: dirs and files,
// then sort by ascend in each part.
// Last, put sorted files just after sorted dirs.
// TODO: code point aware? worth it?
void Explorer::SortEntries(std::vector<std::unique_ptr<Entry>>& entries) {
    size_t i = 0, j = 0;
    while (j < entries.size()) {
        if (entries[j]->IsDir()) {
            std::swap(entries[i], entries[j]);
            i++;
            j++;
        } else {
            j++;
        }
    }
    auto cmp = [](std::unique_ptr<Entry>& a, std::unique_ptr<Entry>& b) {
        return a->name < b->name;
    };
    std::sort(entries.begin(), entries.begin() + i, cmp);
    std::sort(entries.begin() + i, entries.end(), cmp);
}

}  // namespace charxed
