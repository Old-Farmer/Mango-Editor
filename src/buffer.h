#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fs.h"
#include "result.h"
#include "state.h"
#include "utils.h"

namespace mango {

constexpr const char* kSwapSuffix = ".mango_swap";

// A File class, just simple wraps a FILE* handler.
class File {
   public:
    // mode is same as C file stdio api
    // "r" for readonly
    // "w" for writeonly
    // throws IOException, FileCreateException
    File(const std::string& path, const char* mode,
         bool create_if_not_exist = true);
    ~File();

    MANGO_DELETE_COPY(File);
    File(File&&) noexcept;
    File& operator=(File&&) noexcept;

    FILE* file() { return file_; }

    // throws IOException
    // return
    // kOk means ok
    // kEof means EOF
    Result ReadLine(std::string& buf);

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

struct ByteRange {
    size_t line;
    size_t byte_offset;
    size_t len;
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
   public:
    Buffer();
    Buffer(std::string path, bool read_only = false);
    Buffer(Path path, bool read_only = false);
    MANGO_DELETE_COPY(Buffer);
    MANGO_DEFAULT_MOVE(Buffer);

    // throws IOException, FileCreateException
    void Load();

    void Clear();

    // throws IOException
    // return
    // kok
    // kBufferNoBackupFile
    // kBufferCannotRead
    Result Write();

    // Some Modifications
    // Make sure that line number and byte offset is valid, other wise behavir
    // is undefined
    Result DeleteCharacterInLineBefore(size_t line, size_t byte_offset,
                                       size_t& deleted_bytes);
    Result AddStringInLineAfter(size_t line, size_t byte_offset,
                                std::string str);
    Result DeletLine(size_t line);
    Result NewLine(size_t line, size_t byte_offset);
    Result MergeLine(size_t line);

    // Not support regex, only plain text
    std::vector<ByteRange> Search(const std::string& pattern);

    int64_t id() const { return id_; }
    const std::vector<Line>& lines() { return lines_; }
    size_t LineCnt() { return lines_.size(); }
    Path& path() { return path_; }
    BufferState& state() { return state_; };
    bool IsLoad() {
        return state_ == BufferState::kModified ||
               state_ == BufferState::kNotModified ||
               state_ == BufferState::kReadOnly;
    }
    bool read_only() { return read_only_; }
    int64_t version() { return version_; }

    // Buffer list op
    void AppendToList(Buffer* tail) noexcept;
    void RemoveFromList() noexcept;
    bool IsLastBuffer();
    bool IsFirstBuffer();

    void SaveCursorState(Cursor& cursor);
    void RestoreCursorState(Cursor& cursor);

   private:
    static int64_t AllocId() { return cur_buffer_id_++; }

    void Modified();

   public:
    Buffer* next_ = nullptr;
    Buffer* prev_ = nullptr;

    size_t cursor_state_line_ = 0;
    size_t cursor_state_byte_offset_ = 0;
    std::optional<size_t> cursor_state_b_view_col_want_;
    size_t cursor_state_character_in_line_ =
        0;  // for status line to show the state, no need to restore

   private:
    std::vector<Line> lines_;

    Path path_;
    std::string_view filetype_;
    BufferState state_ = BufferState::kHaveNotRead;
    bool read_only_ = false;
    int64_t version_;

    int64_t id_ = AllocId();

    static int64_t cur_buffer_id_;
};

}  // namespace mango
