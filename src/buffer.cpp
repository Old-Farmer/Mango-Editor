#include "buffer.h"

#include <cassert>
#include <cstring>
#include <cstdio>

#include "coding.h"
#include "exception.h"
#include "term.h"

namespace mango {

File::File(const std::string& path) {
    file_ = fopen(path.c_str(), "a+");
    if (file_ == nullptr) {
        throw IOException("File %s can't open: %s", path.c_str(),
                          strerror(errno));
    }
    int ret = fseek(file_, 0, SEEK_SET);
    if (ret == -1) {
        fclose(file_);
        throw IOException("%s", strerror(errno));
    }
}

File::~File() {
    if (file_ != nullptr) {
        int ret = fclose(file_);
        assert(ret != EOF);
    }
}

File::File(File&& other) noexcept : file_(other.file_) {
    other.file_ = nullptr;
}
File& File::operator=(File&& other) noexcept {
    if (&other == this) {
        return *this;
    }

    if (file_ != nullptr) {
        int ret = fclose(file_);
        assert(ret != EOF);
    }
    file_ = other.file_;
    other.file_ = nullptr;
    return *this;
}


void File::Truncate(size_t size) {
    int ret = ftruncate64(fileno(file_), size);
    if (ret == -1) {
        throw IOException("ftruncate64 error: %s", strerror(errno));
    }
}


void File::Fsync() {
    int ret = fsync(fileno(file_));
    if (ret == -1) {
        throw IOException("fsync error: %s", strerror(errno));
    }
}

// void Line::RenderLine(std::vector<uint32_t>* codepoints) {
//     if (modified) {
//         render_line.clear();
//     }
//     int cur_col = 0;
//     for (int i = 0; i < line.size();) {
//         uint32_t codepoint;
//         int len = Utf8ToUnicode(&line[i], codepoint);
//         if (len < 0) {
//             len = -len;
//             codepoint = kReplacementChar;
//             assert(false);
//         }
//         int width = Terminal::WCWidth(codepoint);
//         if (width == -1) {
//             codepoint = kReplacementChar;
//             width = 1;
//             assert(false);
//         } else if (width == 0) {
//             codepoint = kReplacementChar;
//             width = 1;
//             assert(false);
//         }
//         if (modified) {
//             render_line.push_back({cur_col, i});
//         }

//         if (codepoints) {
//             codepoints->push_back(codepoint);
//         }

//         i += len;
//         cur_col += width;
//     }
//     if (modified) {
//         render_line.push_back({cur_col, static_cast<int>(line.size())});
//     }
//     modified = false;
// }

int64_t Buffer::cur_buffer_id_ = 0;

Buffer::Buffer() : id_(AllocId()) {
    lines_.push_back({});
    read_all_ = true;
}

Buffer::Buffer(std::string path)
    : path_(std::move(path)), file_(path_), id_(AllocId()) {}

void Buffer::ReadAll() {
    lines_.clear();
    while (true) {
        auto line = ReadLine();
        if (line.empty()) {  // EOF
            break;
        }
        lines_.emplace_back(std::move(line));
    }
    if (lines_.empty()) {
        lines_.push_back({});
    }

    read_all_ = true;
}

std::string Buffer::ReadLine() {
    std::string line;

    int c;
    while (true) {
        c = fgetc(file_.file());
        if (c == EOF) {
            if (feof(file_.file()) != 0) {
                break;
            }
            throw IOException("%s", strerror(errno));
        }
        if (c == '\n') {
            break;
        } else {
            line.push_back(c);
        }
    }
    return line;
}

Result Buffer::WriteAll() {
    if (file_.file() == nullptr) {
        return kBufferNoBackupFile;
    }

    if (swap_file_.file() == nullptr) {
        swap_file_ = File(path_ + kSwapSuffix);
    } else {
        swap_file_.Truncate(0);
    }

    for (const Line& line: lines_) {
        size_t s = fwrite(line.line.c_str(), 1, line.line.size(), swap_file_.file());        
        if (s < line.line.size()) {
            throw IOException("fwrite error: %s", strerror(errno));
        }
        s = fwrite("\n", 1, 1, swap_file_.file());
        if (s < 1) {
            throw IOException("fwrite error: %s", strerror(errno));
        }
    }
    swap_file_.Fsync();
    int ret = rename((path_ + kSwapSuffix).c_str(), path_.c_str());
    if (ret == -1) {
        throw IOException("rename error: %s", strerror(errno));
    }
    return kOk;
}


}  // namespace mango