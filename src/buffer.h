#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "completer.h"
#include "file.h"
#include "filetype.h"
#include "fs.h"
#include "history.h"
#include "options.h"
#include "pos.h"
#include "result.h"
#include "state.h"
#include "text_tree.h"
#include "tree_sitter/api.h"
#include "utils.h"

namespace charxed {

constexpr std::string_view kSwapSuffix = ".charxed_swap";

struct Cursor;
struct Options;

// This class reprensents a single-range edit operations to the buffer.
// It can represent 3 OPs:
// 1. insert: range.begin == range.end && str.empty(), '\n's in str represent
// new lines.
// 2. delete: str.empty() and range.begin < range.end.
// 3. replace: !str.empty() and range.begin < range.end.
struct BufferEdit {
    Range range;
    std::string str;
};

// A batch(>=1) of edit operations applied atomically.
// All edit operations are based on the single buffer state.
// Edit operations shouldn't overlap with others, and should be sorted by range
// ascendly.
class BufferEditBatch {
    std::vector<BufferEdit> edits_;
    BufferEdit edit_;  // Because a lot of batches only have one edit, to avoid
                       // dynamic memory allocation, we have a inline one.
    size_t size_ = 0;

   public:
    CHX_DEFAULT_CONSTRUCT_DESTRUCT(BufferEditBatch);
    CHX_DEFAULT_COPY(BufferEditBatch);
    CHX_DEFAULT_MOVE(BufferEditBatch);

    BufferEdit* Data() {
        CHX_ASSERT(size_ != 0);
        if (size_ == 1) {
            return &edit_;
        } else {
            return edits_.data();
        }
    }
    const BufferEdit* Data() const {
        CHX_ASSERT(size_ != 0);
        if (size_ == 1) {
            return &edit_;
        } else {
            return edits_.data();
        }
    }
    size_t Size() const { return size_; }
    void Clear() {
        edits_.clear();
        size_ = 0;
    }
    void PushBack(const BufferEdit& edit) {
        if (size_ == 0) {
            edit_ = edit;
        } else if (size_ == 1) {
            edits_.resize(2);
            edits_[0] = std::move(edit_);
            edits_[1] = edit;
        } else {
            edits_.push_back(edit);
        }
        size_++;
    }
    void PushBack(BufferEdit&& edit) {
        if (size_ == 0) {
            edit_ = std::move(edit);
        } else if (size_ == 1) {
            edits_.resize(2);
            edits_[0] = std::move(edit_);
            edits_[1] = std::move(edit);
        } else {
            edits_.push_back(std::move(edit));
        }
        size_++;
    }
};

// A class which represents a file contents in memory.
// The Buffer may not be backed by a file.
// A file backup buffer can be read only, and a no file backup buffer can't be
// read only.
// Only support Posix now.

// TODO: Windows support
class Buffer {
    struct BufferEditHistoryItem {
        // For redo
        BufferEditBatch origin;
        Pos origin_pos_hint;

        // For undo
        BufferEditBatch reverse;
        Pos reverse_pos_hint;
    };

   public:
    // if new_file == true, will alloc a new_file_id to this buffer, else just a
    // no file-backup buffer.
    Buffer(GlobalOpts* options, bool new_file = true);
    // A new file backup buffer
    Buffer(GlobalOpts* options, const std::string& path,
           bool read_only = false);
    // A new file backup buffer
    Buffer(GlobalOpts* options, const Path& path, bool read_only = false);
    CHX_DELETE_COPY(Buffer);
    CHX_DEFAULT_MOVE(Buffer);
    ~Buffer();

    // throws IOException, CodingException, FSException
    // if it is a no file backup buffer, any of above exceptions won't throw.
    void Load();

    // throws IOException, FileExistException, CodingException, FSException
    // two args is similar to Add/Delete/Replace
    void Reload(Pos* cursor_pos, Pos& cursor_pos_hint);

    void Clear();

    // throws IOException
    // return
    // kok
    // kBufferNoBackupFile
    // kBufferCannotLoad
    // kBufferReadOnly
    Result Save();

    // Save the buffer content to a new path.
    // path shouldn't be empty.
    // If success, the buffer will use the new path.
    // else, no effect occur.
    // throws IOException
    // return
    // kok
    // kBufferCannotLoad
    // kBufferReadOnly
    Result SaveAs(const Path& path);

    // Get content operations
    // Make sure that line, Range or Pos is valid, otherwise behavir
    // is undefined.

    std::string_view GetLine(size_t line, std::string& buf) const {
        CHX_ASSERT(LineCnt() > line);
        auto line_view = tree_.GetLine(line);
        auto line_str = line_view.ToStringView(buf);
        return line_str;
    }

    using TextView = TextTree::TextView;
    TextView GetLineView(size_t line) const {
        CHX_ASSERT(LineCnt() > line);
        return tree_.GetLine(line);
    }

    TextView GetContentView(const Range& range) const {
        CHX_ASSERT(LineCnt() > range.end.line);
        return {tree_.Find(range.begin), tree_.Find(range.end)};
    }

    // GetConent will copy out a string in range.
    std::string GetContent(const Range& range) const {
        return GetContentView(range).ToString();
    }

    // Please refer TextTree API
    using Iterator = TextTree::Iterator;
    Iterator Find(Pos pos) const { return tree_.Find(pos); }
    Iterator Find(size_t offset) const { return tree_.Find(offset); }
    Pos OffsetToPos(size_t offset) const { return tree_.OffsetToPos(offset); }

    Iterator LineEnd(size_t line) {
        CHX_ASSERT(LineCnt() > line);
        if (line == LineCnt() - 1) {
            return tree_.End();
        } else {
            auto iter = Find({line + 1, 0});
            iter.PrevByte();
            return iter;
        }
    }

    Iterator Begin() const { return tree_.Begin(); }
    Iterator End() const { return tree_.End(); }

    // Edit operations
   private:
    // Some operations used inner
    // if record is true, some info will be kept so reverse op can be done.
    // if record_ts_edit is true, ts edit will be kept for treesittier
    void Edit(const BufferEdit& edit, Pos& curosr_pos_hint);
    void AddInner(Pos pos, std::string_view str, Pos& cursor_pos_hint,
                  bool record_ts_edit);
    std::string DeleteInner(const Range& range, Pos& cursor_pos_hint,
                            bool record_reverse, bool record_ts_edit);
    std::string ReplaceInner(const Range& range, std::string_view str,
                             Pos& cursor_pos_hint, bool record_reverse);

    // Make sure that edit_history_ Size != 0
    bool TryRecordMerge(const BufferEditHistoryItem& item);
    // Caller should check whefher kMaxEditHistory <= 0
    void Record(BufferEditHistoryItem&& item);

    template <typename T>
    T GetOpt(OptKey key) {
        if (opts_.GetScope(key) == OptScope::kGlobal) {
            return opts_.global_opts_->GetOpt<T>(key);
        }
        return opts_.GetOpt<T>(key);
    }

   public:
    // Make sure that Range or Pos is valid, otherwise behavir
    // is undefined(allow Range is empty).
    // Also, make sure that all edit op will not corrupt buffer coding
    // correctness, otherwise behavior is undefined.
    // One error,
    // return kBufferCannotLoad, kBufferReadOnly; On ok, return kOk, and
    // cursor_pos_hint will be set to the suggest cursor pos if
    // use_given_pos_hint is false or no such parameter
    // NOTE:
    // 1. We use string_view here because:
    //      1) we always want to copy the string and don't care about whether it
    //      is a rvalue. 2) Usually we want to insert a char[] to a buffer, use
    //      string_view can eliminate a string ctor.
    // 2. cursor_pos and cursor_pos_hint should point to the same address if
    // use_given_pos_hint == true, otherwise behavior is undefined.
    Result Add(Pos pos, std::string_view str, const Pos* cursor_pos,
               bool use_given_pos_hint, Pos& cursor_pos_hint);
    Result Delete(const Range& range, const Pos* cursor_pos,
                  bool use_given_pos_hint, Pos& cursor_pos_hint);
    Result Replace(const Range& range, std::string_view str,
                   const Pos* cursor_pos, bool use_given_pos_hint,
                   Pos& cursor_pos_hint);

    // Requirement: batch size != 0
    Result BatchEdit(const BufferEditBatch& edit_batch, const Pos* cursor_pos,
                     bool use_given_pos_hint, Pos& cursor_pos_hint);

    // return kNoHistoryAvailable if no action can be done
    // else return kOk
    // cursor_pos_hint will be set to the suggest cursor pos
    Result Redo(Pos& cursor_pos_hint);
    Result Undo(Pos& cursor_pos_hint);

    // Sometimes we can't calc cursor_pos_hint before edit, we should call this
    // to fix it.
    // NOTE: not used now, maybe useful
    // TODO: Implement it
    void FixLastHistoryItemCursorPosHint(Pos cursor_pos_hint);

    int64_t id() const noexcept { return id_; }
    // Must always >= 1
    size_t LineCnt() const noexcept { return tree_.LineCnt(); }
    size_t Size() const noexcept { return tree_.Size(); }
    BufferState& state() { return state_; };
    bool IsLoad() const noexcept {
        return state_ == BufferState::kModified ||
               state_ == BufferState::kNotModified ||
               state_ == BufferState::kReadOnly;
    }
    bool read_only() const noexcept { return read_only_; }
    bool& read_only() noexcept { return read_only_; }
    int64_t version() const noexcept { return version_; }
    // -1 means not stored
    FileType filetype() const noexcept { return filetype_; }
    EOLSeq eol_seq() const noexcept { return eol_seq_; }
    Opts& opts() { return opts_; }
    const Opts& opts() const { return opts_; }
    Completer* completer() { return basic_word_completer_.get(); }
    bool lsp_attached() { return lsp_attached_; }

    zstring_view Name() noexcept {
        return path_.Empty() ? new_file_info_->name : path_.ThisPath();
    }
    Path& path() noexcept { return path_; }

    TSTree*& ts_tree() noexcept { return ts_tree_; }
    const TSTree* ts_tree() const noexcept { return ts_tree_; }

    // Buffer list op
    void AppendToList(Buffer* tail) noexcept;
    void RemoveFromList() noexcept;
    bool IsLastBuffer() const;
    bool IsFirstBuffer() const;

    TSInputEdit GetEditForTreeSitter();

   private:
    static int64_t AllocId() { return cur_buffer_id_++; }

    void Modified();

   public:
    Buffer* next_ = nullptr;
    Buffer* prev_ = nullptr;

   private:
    TextTree tree_;

    Path path_;
    struct NewFileInfo {
        std::string name;
        int64_t id;
    };
    std::unique_ptr<NewFileInfo> new_file_info_;
    FileType filetype_ = FileType::kNone;
    BufferState state_ = BufferState::kHaveNotRead;
    EOLSeq eol_seq_ = EOLSeq::kLF;  // Default LF
    bool read_only_ = false;
    // When a buffer is modified, version_ will be bumpped up.
    int64_t version_;

    History<BufferEditHistoryItem> edit_history_;
    bool havent_wrap_history_ = true;

    // Just for tree-sitter
    TSInputEdit ts_edit_;
    TSTree* ts_tree_ = nullptr;
    // bool after_get_edit_modified = false;

    std::unique_ptr<BufferBasicWordCompleter> basic_word_completer_;

    // lsp
    bool lsp_attached_ = false;

    Opts opts_;

    int64_t id_ = AllocId();

    static int64_t cur_buffer_id_;

    static std::vector<bool> new_file_alloced_ids_;
};

}  // namespace charxed
