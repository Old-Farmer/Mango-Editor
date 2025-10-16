#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "filetype.h"
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
    std::string line;
    Line() {}
    Line(std::string _line) : line(std::move(_line)) {}
};

class Cursor;

// A class which represents a file contents in memory
// the Buffer may not be backed by a file
// Only Posix
// TODO: Windows support
class Buffer {
   public:
    Buffer();
    Buffer(std::string path);
    MANGO_DELETE_COPY(Buffer);
    MANGO_DEFAULT_MOVE(Buffer);

    // throws IOException, FileCreateException
    void ReadAll();

    // throws IOException
    // return
    // kok
    // kBufferNoBackupFile
    // kBufferCannotRead
    Result WriteAll();

    int64_t id() const { return id_; }
    std::vector<Line>& lines() { return lines_; }
    const std::string& path() { return path_; }
    BufferState& state() { return state_; };
    bool IsReadAll() {
        return state_ == BufferState::kModified ||
               state_ == BufferState::kNotModified;
    }

    // Buffer list op
    void AppendToList(Buffer*& tail) noexcept;
    void RemoveFromList() noexcept;

    void SaveCursorState(Cursor& cursor);
    void RestoreCursorState(Cursor& cursor);

   private:
    static int64_t AllocId() { return cur_buffer_id_++; }

   public:
    Buffer* next_ = nullptr;
    Buffer* prev_ = nullptr;

    int64_t cursor_state_line_ = 0;
    int64_t cursor_state_byte_offset_ = 0;
    int64_t cursor_state_b_view_col_want_ = 0;

   private:
    std::vector<Line> lines_;

    std::string path_;
    std::string file_name_; // TODO: use it
    Filetype filetype_;
    bool read_all_ = false;
    BufferState state_ = BufferState::kHaveNotRead;

    int64_t id_;

    static int64_t cur_buffer_id_;
};

}  // namespace mango
