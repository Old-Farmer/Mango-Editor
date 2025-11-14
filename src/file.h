#pragma once

#include "utils.h"

namespace mango {

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

    // Detect Whether the file related with the path is readable
    static bool FileReadable(const std::string& path) noexcept;

   private:
    FILE* file_;
};

}  // namespace mango