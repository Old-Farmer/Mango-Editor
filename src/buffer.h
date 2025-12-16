#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "completer.h"
#include "file.h"
#include "fs.h"
#include "options.h"
#include "pos.h"
#include "result.h"
#include "state.h"
#include "tree_sitter/api.h"
#include "utils.h"

namespace mango {

constexpr const char* kSwapSuffix = ".mango_swap";

struct Line {
    std::string line_str;
    Line() {}
    Line(std::string _line_str) : line_str(std::move(_line_str)) {}
};

struct Cursor;
struct Options;

// This class reprensents edit operations to the buffer.
// It can represent 3 OPs:
// 1. insert: range.begin == range.end && str.empty(), '\n's in str represent
// new lines.
// 2. delete: str.empty() and range.begin < range.end.
// 3. replace: !str.empty() and range.begin < range.end.
struct BufferEdit {
    Range range;
    std::string str;
};

// A class which represents a file contents in memory.
// The Buffer may not be backed by a file.
// A file backup buffer can be read only, and a no file backup buffer can't be
// read only.
// Only support Posix now.

// NOTE: For simplicity, we use an array of string to represent a buffer.
// Maybe chage the internal data structure, So the api will not be stable
// now.

// TODO: Windows support
class Buffer {
    struct BufferEditHistoryItem {
        // For redo
        BufferEdit origin;
        Pos origin_pos_hint;

        // For undo
        BufferEdit reverse;
        Pos reverse_pos_hint;
    };

   public:
    // if new_file == true, will alloc a new_file_id to this buffer, else just a
    // no file-backup buffer.
    Buffer(GlobalOpts* options, bool new_file = true);
    Buffer(GlobalOpts* options, const std::string& path, bool read_only = false);
    Buffer(GlobalOpts* options, const Path& path, bool read_only = false);
    MGO_DELETE_COPY(Buffer);
    MGO_DEFAULT_MOVE(Buffer);
    ~Buffer();

    // throws IOException, FileCreateException, CodingException
    // if it is a no file backup buffer, any of above exceptions won't throw.
    void Load();

    void Clear();

    // throws IOException
    // return
    // kok
    // kBufferNoBackupFile
    // kBufferCannotRead
    Result Write();

    // Get content operations
    // Make sure that line, Range or Pos is valid, otherwise behavir
    // is undefined.

    const std::string& GetLine(size_t line) const {
        MGO_ASSERT(LineCnt() > line);
        return lines_[line].line_str;
    }

    // This method is for some op to get a '\0' terminated sub_str but don't
    // want to copy.
    // They must modify a byte to '\0' and then modified back.
    std::string& GetLineNonConst(size_t line) {
        MGO_ASSERT(LineCnt() > line);
        return lines_[line].line_str;
    }

    // GetConent will copy out a string in range.
    std::string GetContent(const Range& range) const;

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

    bool TryRecordMerge(const BufferEditHistoryItem& item);
    void Record(BufferEditHistoryItem&& item);

    // return an global offset of a pos
    // TODO: optimize it.
    size_t OffsetAndInvalidAfterPos(Pos pos);

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
    // We use string_view here because:
    // 1. we always want to copy the string and don't care about whether it is a
    // rvalue.
    // 2. Usually we want to insert a char[] to a buffer, use string_view can
    // eliminate a string ctor.
    Result Add(Pos pos, std::string_view str, const Pos* cursor_pos,
               bool use_given_pos_hint, Pos& cursor_pos_hint);
    Result Delete(const Range& range, const Pos* cursor_pos,
                  Pos& cursor_pos_hint);
    Result Replace(const Range& range, std::string_view str,
                   const Pos* cursor_pos, bool use_given_pos_hint,
                   Pos& cursor_pos_hint);

    // return kNoHistoryAvailable if no action can be done
    // else return kOk
    // cursor_pos_hint will be set to the suggest cursor pos
    Result Redo(Pos& cursor_pos_hint);
    Result Undo(Pos& cursor_pos_hint);

    // Not support regex, only plain text
    std::vector<Range> Search(const std::string& pattern);

    int64_t id() const noexcept { return id_; }
    size_t LineCnt() const noexcept { return lines_.size(); }
    BufferState& state() { return state_; };
    bool IsLoad() const noexcept {
        return state_ == BufferState::kModified ||
               state_ == BufferState::kNotModified ||
               state_ == BufferState::kReadOnly;
    }
    bool read_only() const noexcept { return read_only_; }
    int64_t version() const noexcept { return version_; }
    // -1 means not stored
    zstring_view filetype() const noexcept { return filetype_; }
    EOLSeq eol_seq() const noexcept { return eol_seq_; }
    const Opts& opts() { return opts_; }
    Completer* completer() { return basic_word_completer_.get(); }
    bool lsp_attached() { return lsp_attached_; }

    zstring_view Name() noexcept {
        return path_.Empty() ? new_file_info_->name : path_.ThisPath();
    }

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
    std::vector<Line> lines_;

    Path path_;
    struct NewFileInfo {
        std::string name;
        int64_t id;
    };
    std::unique_ptr<NewFileInfo> new_file_info_;
    zstring_view filetype_;
    BufferState state_ = BufferState::kHaveNotRead;
    EOLSeq eol_seq_ = EOLSeq::kLF;  // Default LF
    bool read_only_ = false;
    // When a buffer is modified, version_ will be bumpped up.
    int64_t version_;

    using HistoryList = std::list<BufferEditHistoryItem>;
    using HistoryListIter = HistoryList::iterator;
    // Use unique ptr to avoid a issue when std::list is moved, its iterator
    // will be become invalid
    std::unique_ptr<HistoryList> edit_history_ =
        std::make_unique<HistoryList>();
    HistoryListIter edit_history_cursor_ = edit_history_->end();
    bool never_wrap_history_ = true;

    // Just for tree-sitter
    TSInputEdit ts_edit_;
    // bool after_get_edit_modified = false;

    // A prefiex offset cache, for fast offset calculation.
    std::vector<size_t> offset_per_line_ = {0};

    std::unique_ptr<BufferBasicWordCompleter> basic_word_completer_;

    // lsp
    bool lsp_attached_ = false;

    Opts opts_;

    int64_t id_ = AllocId();

    static int64_t cur_buffer_id_;

    static std::vector<bool> new_file_alloced_ids_;
};

}  // namespace mango
