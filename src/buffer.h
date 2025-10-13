#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "result.h"
#include "utils.h"

namespace mango {

constexpr const char* kSwapSuffix = ".mango_swap";

// A File class, just simple wraps a FILE* handler.
class File {
   public:
    File() {}
    // throws IOException
    File(const std::string& path);
    ~File();

    MANGO_DEFAULT_COPY(File);
    File(File&&) noexcept;
    File& operator=(File&&) noexcept;

    FILE* file() { return file_; }

    // throws IOException
    void Truncate(size_t size);
    // throws IOException
    void Fsync();

   private:
    FILE* file_ = nullptr;
};

// struct RenderChar {
//     int col;  // col of this character if the row is rendered in terminal
//     from
//               // start
//     int index_in_line;  // This charater's related start byte in line
// };

struct Line {
    std::string line;
    // bool modified = true;
    // std::vector<RenderChar> render_line;

    Line() {}
    Line(std::string _line) : line(std::move(_line)) {}

    // // Render line and return codepoints if not null.
    // void RenderLine(std::vector<uint32_t>* codepoints);
};

// A class which represents a file contents in memory
// the Buffer may not be backed by a file
// Only Posix
// TODO: Windows support
class Buffer {
   public:
    Buffer();
    // throws IOException
    Buffer(std::string path);
    MANGO_DELETE_COPY(Buffer);
    MANGO_DEFAULT_MOVE(Buffer);

    // throws IOException
    void ReadAll();

    // throws IOException
    // return kok
    // kBufferNoBackupFile
    Result WriteAll();

    int64_t id() const { return id_; }
    std::vector<Line>& lines() { return lines_; }

    bool IsReadAll() const { return read_all_; }

   private:
    // throws IOException
    // return 
    // kOk means ok
    // kEof means EOF
    Result ReadLine(std::string& buf);

    static int64_t AllocId() { return cur_buffer_id_++; }

   private:
    // If no line in file, just leave one empty line
    std::vector<Line> lines_;

    std::string path_;
    File file_;
    File swap_file_;
    bool read_all_ = false;

    int64_t id_;

    static int64_t cur_buffer_id_;
};

}  // namespace mango
