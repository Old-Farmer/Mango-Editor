#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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

   private:
    static int64_t AllocId() { return cur_buffer_id_++; }

   private:
    std::vector<Line> lines_;

    std::string path_;
    bool read_all_ = false;
    BufferState state_ = BufferState::kHaveNotRead;

    int64_t id_;

    static int64_t cur_buffer_id_;
};

}  // namespace mango
