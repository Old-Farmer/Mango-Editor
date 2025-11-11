#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fs.h"
#include "result.h"
#include "state.h"
#include "tree_sitter/api.h"
#include "utils.h"

namespace mango {

constexpr const char* kSwapSuffix = ".mango_swap";

// End of line sequence
constexpr const char* kEOLSeqLF = "\n";
constexpr const char* kEOLSeqCRLF = "\r\n";
constexpr const char* kEOLSeqLFReqStr = "LF";
constexpr const char* kEOLSeqCRLFReqStr = "CRLF";

enum class EOLSeq { kLF, kCRLF };

std::ostream& operator<<(std::ostream& os, EOLSeq eol_seq);

// A File class, just simple wraps a FILE* handler.
class File {
   public:
    // mode is same as C file stdio api
    // "r" for readonly
    // "w" for writeonly
    // create_if_not_exist only meaningful to read
    // throws IOException, FileCreateException
    File(const std::string& path, const char* mode, bool create_if_not_exist);
    ~File();

    MGO_DELETE_COPY(File);
    File(File&&) noexcept;
    File& operator=(File&&) noexcept;

    FILE* file() { return file_; }

    // Read one line from file.
    // EOLSeq will be stored as the eol seq.
    // if meet eof, eol_seq will not be set.
    // throws IOException.
    // return:
    // kOk means ok;
    // kEof means EOF.
    Result ReadLine(std::string& buf, EOLSeq& eol_seq);

    // Dump the raw file contents to a string.
    // throw IOException
    std::string ReadAll();

    // throws IOException
    void Truncate(size_t size);
    // throws IOException
    void Fsync();

   private:
    FILE* file_;
};

struct Line {
    std::string line_str;
    Line() {}
    Line(std::string _line_str) : line_str(std::move(_line_str)) {}
};

class Cursor;
class Options;

// A Pos object represent a position in the line
struct Pos {
    size_t line;
    size_t byte_offset;
};
inline bool operator==(const Pos& pos1, const Pos& pos2) noexcept {
    return pos1.byte_offset == pos2.byte_offset && pos1.line == pos2.line;
}
inline bool operator<(const Pos& pos1, const Pos& pos2) noexcept {
    return pos1.line < pos2.line ||
           (pos1.line == pos2.line && pos1.byte_offset < pos2.byte_offset);
}

// Range represents a text range: [begin, end)
// NOTE:
// e.g. if the first line contains "abc", Use {{0, 0}, {0, 3}} to rep a line,
// Use {{0, 0}, {1, 0}} to rep the whole line with a '\n'.
struct Range {
    Pos begin;
    Pos end;

    bool PosBeforeMe(const Pos& pos) const { return pos < begin; }
    bool PosAfterMe(const Pos& pos) const { return !(pos < end); }

    // Pos is in Range ?
    bool PosInMe(const Pos& pos) const {
        return !(PosAfterMe(pos) || PosBeforeMe(pos));
    }

    bool RangeBeforeMe(const Range& range) {
        return range.end == begin || range.end < begin;
    }
    bool RangeAfterMe(const Range& range) {
        return end == range.begin || end < range.begin;
    }
    bool RangeOverlapMe(const Range& range) {
        return !RangeBeforeMe(range) && !RangeAfterMe(range);
    }
    bool RangeEqualMe(const Range& range) {
        return range.begin == begin && range.end == end;
    }
};

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

// NOTE: for simplicity, we use an array of string to represent a buffer, and
// expose this structure to the outside world.
// TODO: encapsulate the structure in an good api, support modify and read op

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
    Buffer(Options* options);
    Buffer(Options* options, std::string path, bool read_only = false);
    Buffer(Options* options, Path path, bool read_only = false);
    MGO_DELETE_COPY(Buffer);
    MGO_DEFAULT_MOVE(Buffer);

    // throws IOException, FileCreateException
    void Load();

    void Clear();

    // throws IOException
    // return
    // kok
    // kBufferNoBackupFile
    // kBufferCannotRead
    Result Write();

    // Get content operations
    // Make sure that Range or Pos is valid, otherwise behavir
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
    void AddInner(const Pos& pos, const std::string& str, Pos& cursor_pos_hint,
                  bool record_ts_edit);
    std::string DeleteInner(const Range& range, Pos& cursor_pos_hint,
                            bool record_reverse, bool record_ts_edit);
    std::string ReplaceInner(const Range& range, const std::string& str,
                             Pos& cursor_pos_hint, bool record_reverse);

    void Record(BufferEditHistoryItem item);

   public:
    // Make sure that Range or Pos is valid, otherwise behavir
    // is undefined.
    // error return kBufferCannotLoad, kBufferReadOnly
    // ok return kOk
    // cursor_pos_hint will be set to the suggest cursor pos if
    // use_given_pos_hint is false or no such parameter
    Result Add(const Pos& pos, std::string str, bool use_given_pos_hint,
               Pos& cursor_pos_hint);
    Result Delete(const Range& range, Pos* cursor_pos, Pos& cursor_pos_hint);
    Result Replace(const Range& range, std::string str, Pos* cursor_pos,
                   bool use_given_pos_hint, Pos& cursor_pos_hint);

    // return kNoHistoryAvailable if no action can be done
    // else return kOk
    // cursor_pos_hint will be set to the suggest cursor pos
    Result Redo(Pos& cursor_pos_hint);
    Result Undo(Pos& cursor_pos_hint);

    // Not support regex, only plain text
    std::vector<Range> Search(const std::string& pattern);

    // return an global offset of a pos
    size_t Offset(const Pos& pos) const;

    int64_t id() const noexcept { return id_; }
    const std::vector<Line>& lines() const { return lines_; }
    size_t LineCnt() const noexcept { return lines_.size(); }
    Path& path() { return path_; }
    BufferState& state() { return state_; };
    bool IsLoad() const noexcept {
        return state_ == BufferState::kModified ||
               state_ == BufferState::kNotModified ||
               state_ == BufferState::kReadOnly;
    }
    bool read_only() const noexcept { return read_only_; }
    int64_t version() const noexcept { return version_; }
    size_t cursor_state_line() const noexcept { return cursor_state_line_; }
    size_t cursor_state_character_in_line() const noexcept {
        return cursor_state_character_in_line_;
    }
    zstring_view filetype() const noexcept { return filetype_; }
    EOLSeq eol_seq() const noexcept { return eol_seq_; }

    // Buffer list op
    void AppendToList(Buffer* tail) noexcept;
    void RemoveFromList() noexcept;
    bool IsLastBuffer();
    bool IsFirstBuffer();

    void SaveCursorState(Cursor& cursor);
    void RestoreCursorState(Cursor& cursor);

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
    zstring_view filetype_;
    EOLSeq eol_seq_ = EOLSeq::kLF;  // Default LF
    BufferState state_ = BufferState::kHaveNotRead;
    bool read_only_ = false;
    int64_t
        version_;  // When a buffer is modified, version_ will be bumpped up.

    size_t cursor_state_line_ = 0;
    size_t cursor_state_byte_offset_ = 0;
    std::optional<size_t> cursor_state_b_view_col_want_;
    size_t cursor_state_character_in_line_ =
        0;  // for status line to show the state, no need to restore

    using HistoryList = std::list<BufferEditHistoryItem>;
    using HistoryListIter = HistoryList::iterator;
    // Use unique ptr to avoid a issue when std::list is moved, its iterator
    // will be become invalid
    std::unique_ptr<HistoryList> edit_history_ =
        std::make_unique<HistoryList>();
    HistoryListIter edit_history_cursor_ = edit_history_->end();

    // Just for tree-sitter
    TSInputEdit ts_edit_;
    bool after_get_edit_modified = false;

    Options* options_;

    int64_t id_ = AllocId();

    static int64_t cur_buffer_id_;
};

}  // namespace mango
