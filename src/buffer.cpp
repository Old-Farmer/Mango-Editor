#include "buffer.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "coding.h"
#include "exception.h"
#include "term.h"

namespace mango {

File::File(const std::string& path, const char* mode,
           bool create_if_not_exist) {
    file_ = fopen(path.c_str(), mode);
    if (file_ != nullptr) {
        return;
    }

    if (!create_if_not_exist || errno != ENOENT) {
        throw IOException("File %s can't open: %s", path.c_str(),
                          strerror(errno));
    }

    file_ = fopen(path.c_str(), "w+");
    if (file_ == nullptr) {
        throw FileCreateException("File %s can't open: %s", path.c_str(),
                                  strerror(errno));
    }
}

File::~File() {
    int ret = fclose(file_);
    assert(ret != EOF);
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

Result File::ReadLine(std::string& buf) {
    int c;
    while (true) {
        c = fgetc(file_);
        if (c == EOF) {
            if (feof(file_) != 0) {
                return kEof;
            }
            throw IOException("%s", strerror(errno));
        }
        if (c == '\n') {
            break;
        } else {
            buf.push_back(c);
        }
    }
    return kOk;
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

int64_t Buffer::cur_buffer_id_ = 0;

Buffer::Buffer() : id_(AllocId()) {
    lines_.push_back({});
    read_all_ = true;
}

Buffer::Buffer(std::string path) : path_(std::move(path)), id_(AllocId()) {}

void Buffer::ReadAll() {
    try {
        lines_.clear();

        if (path_.empty()) {
            lines_.push_back({});
            state_ = BufferState::kNotModified;
            return;
        }

        File f(path_, "r");

        while (true) {
            std::string buf;
            Result ret = f.ReadLine(buf);
            lines_.emplace_back(std::move(buf));
            if (ret == kEof) {
                break;
            }
        }
        if (lines_.empty()) {
            lines_.push_back({});
        }

        state_ = BufferState::kNotModified;
    } catch (IOException& e) {
        lines_.push_back({}); // ensure one empty line
        throw;
    }
}

Result Buffer::WriteAll() {
    if (path_.empty()) {
        return kBufferNoBackupFile;
    }

    if (!IsReadAll()) {
        return kBufferCannotRead;
    }

    File swap_file = File(path_ + kSwapSuffix, "w");

    for (size_t i = 0; i < lines_.size(); i++) {
        if (!lines_[i].line.empty()) {
            size_t s = fwrite(lines_[i].line.c_str(), 1, lines_[i].line.size(),
                              swap_file.file());
            if (s < lines_[i].line.size()) {
                throw IOException("fwrite error: %s", strerror(errno));
            }
        }
        if (i != lines_.size() - 1) {
            size_t s = fwrite("\n", 1, 1, swap_file.file());
            if (s < 1) {
                throw IOException("fwrite error: %s", strerror(errno));
            }
        }
    }

    if (fflush(swap_file.file()) == EOF) {
        throw IOException("fflush error: %s", strerror(errno));
    }
    swap_file.Fsync();
    int ret = rename((path_ + kSwapSuffix).c_str(), path_.c_str());
    if (ret == -1) {
        throw IOException("rename error: %s", strerror(errno));
    }

    state_ = BufferState::kNotModified;
    return kOk;
}

}  // namespace mango