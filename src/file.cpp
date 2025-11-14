#include "file.h"

#include <unistd.h>

#include <cstring>
#include <ostream>

#include "exception.h"

namespace mango {

std::ostream& operator<<(std::ostream& os, EOLSeq eol_seq) {
    if (eol_seq == EOLSeq::kLF) {
        os << kEOLSeqLFReqStr;
    } else if (eol_seq == EOLSeq::kCRLF) {
        os << kEOLSeqCRLFReqStr;
    } else {
        MGO_ASSERT(false);
    }
    return os;
}

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
        throw FileCreateException("File %s can't create: %s", path.c_str(),
                                  strerror(errno));
    }
}

File::~File() {
    int ret = fclose(file_);
    MGO_ASSERT(ret != EOF);
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
        MGO_ASSERT(ret != EOF);
    }
    file_ = other.file_;
    other.file_ = nullptr;
    return *this;
}

Result File::ReadLine(std::string& buf, EOLSeq& eol_seq) {
    int c;
    Result res = kOk;
    while (true) {
        c = fgetc(file_);
        if (c == EOF) {
            if (feof(file_) != 0) {
                res = kEof;
                break;
            }
            throw IOException("%s", strerror(errno));
        }
        if (c == '\n') {
            res = kOk;
            if (buf.empty() || buf.back() != '\r') {
                eol_seq = EOLSeq::kLF;
            } else {
                buf.pop_back();
                eol_seq = EOLSeq::kCRLF;
            }
            break;
        } else {
            buf.push_back(c);
        }
    }
    return res;
}

std::string File::ReadAll() {
    constexpr size_t size = 4096;
    char buf[size];
    std::string ret;
    while (true) {
        size_t s = fread(buf, 1, size, file_);
        ret.append(buf, s);
        if (s < size) {
            if (feof(file_)) {
                break;
            } else if (feof(file_)) {
                throw IOException("%s", strerror(errno));
            } else {
                MGO_ASSERT(false);
            }
        }
    }

    // Check if the file lines end with crlf. Just detect the first line.
    size_t str_size = ret.size();
    for (size_t i = 0; i < str_size; i++) {
        if (ret[i] == '\n') {
            if (i == 0) {
                return ret;
            }
            if (ret[i - 1] != '\r') {
                return ret;
            }
            break;
        }
    }

    // Is crlf, we delete the '\r' before '\n'
    for (size_t i = 0; i < str_size;) {
        if (ret[i] == '\n') {
            ret.erase(i - 1);  // erase '/r'
        } else {
            i++;
        }
    }
    return ret;
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

bool File::FileReadable(const std::string& path) noexcept {
    FILE* f = fopen(path.c_str(), "r");
    if (f == nullptr) {
        return false;
    }

    fclose(f);
    return true;
}

}  // namespace mango