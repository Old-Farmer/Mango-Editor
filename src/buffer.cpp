#include "buffer.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "coding.h"
#include "cursor.h"
#include "exception.h"
#include "filetype.h"
#include "logging.h"

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
        throw FileCreateException("File %s can't create: %s", path.c_str(),
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

Buffer::Buffer() {}

Buffer::Buffer(std::string path, bool read_only)
    : path_(std::move(path)), read_only_(read_only) {}

Buffer::Buffer(Path path, bool read_only)
    : path_(std::move(path)), read_only_(read_only) {}

void Buffer::Load() {
    try {
        lines_.clear();

        if (path_.Empty()) {
            lines_.push_back({});
            state_ = BufferState::kNotModified;
            return;
        }

        File f(path_.AbsolutePath(), "r");
        MANGO_LOG_DEBUG("file path %s", path_.AbsolutePath().c_str());

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

        filetype_ = DecideFiletype(path_.FileName());

        state_ =
            read_only_ ? BufferState::kReadOnly : BufferState::kNotModified;
    } catch (IOException& e) {
        lines_.push_back({});  // ensure one empty line
        throw;
    }
}

void Buffer::Clear() {
    state_ = BufferState::kNotModified;
    lines_.clear();
    lines_.push_back({});
}

Result Buffer::Write() {
    if (path_.Empty()) {
        return kBufferNoBackupFile;
    }

    if (!IsLoad()) {
        return kBufferCannotLoad;
    }

    std::string swap_file_path = path_.AbsolutePath() + kSwapSuffix;
    File swap_file = File(swap_file_path, "w");

    for (size_t i = 0; i < lines_.size(); i++) {
        if (!lines_[i].line_str.empty()) {
            size_t s = fwrite(lines_[i].line_str.c_str(), 1,
                              lines_[i].line_str.size(), swap_file.file());
            if (s < lines_[i].line_str.size()) {
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
    int ret = rename(swap_file_path.c_str(), path_.AbsolutePath().c_str());
    if (ret == -1) {
        throw IOException("rename error: %s", strerror(errno));
    }

    state_ = BufferState::kNotModified;
    return kOk;
}

Result Buffer::DeleteCharacterInLineBefore(size_t line, size_t byte_offset,
                                           size_t& deleted_bytes) {
    assert(byte_offset != 0);
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    std::vector<uint32_t> charater;
    int len, character_width;
    Result ret = PrevCharacterInUtf8(lines_[line].line_str, byte_offset,
                                     charater, len, character_width);
    assert(ret == kOk);
    lines_[line].line_str.erase(byte_offset - len, len);
    deleted_bytes = len;
    Modified();
    return kOk;
}
Result Buffer::AddStringInLineAfter(size_t line, size_t byte_offset,
                                    std::string str) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    lines_[line].line_str.insert(byte_offset, std::move(str));
    Modified();
    return kOk;
}

Result Buffer::DeletLine(size_t line) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    lines_.erase(lines_.begin() + line);
    Modified();
    return kOk;
}

Result Buffer::NewLine(size_t line, size_t byte_offset) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    std::string new_line = lines_[line].line_str.substr(
        byte_offset, lines_[line].line_str.size() - byte_offset);
    lines_.insert(lines_.begin() + line + 1, std::move(new_line));
    lines_[line].line_str.resize(byte_offset);
    Modified();
    return kOk;
}

Result Buffer::MergeLine(size_t line) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    lines_[line].line_str.append(lines_[line + 1].line_str);
    lines_.erase(lines_.begin() + line + 1);
    Modified();
    return kOk;
}

std::vector<ByteRange> Buffer::Search(const std::string& pattern) {
    std::vector<ByteRange> res;
    for (size_t line = 0; line < lines_.size(); line++) {
        size_t pos = 0;
        while (true) {
            pos = lines_[line].line_str.find(pattern, pos);
            if (pos == std::string::npos) {
                break;
            }
            res.push_back({line, pos, pattern.size()});
            pos += pattern.size();
        }
    }
    return res;
}

void Buffer::AppendToList(Buffer* tail) noexcept {
    tail->prev_->next_ = this;
    prev_ = tail->prev_;
    next_ = tail;
    tail->prev_ = this;
}

void Buffer::RemoveFromList() noexcept {
    // Must have a dummy head and tail_
    next_->prev_ = prev_;
    prev_->next_ = next_;
    next_ = nullptr;
    prev_ = nullptr;
}

bool Buffer::IsLastBuffer() { return next_->next_ == nullptr; }
bool Buffer::IsFirstBuffer() { return prev_->prev_ == nullptr; }

void Buffer::SaveCursorState(Cursor& cursor) {
    cursor_state_line_ = cursor.line;
    cursor_state_byte_offset_ = cursor.byte_offset;
    cursor_state_b_view_col_want_ = cursor.b_view_col_want;
    cursor_state_character_in_line_ = cursor.character_in_line;
}

void Buffer::RestoreCursorState(Cursor& cursor) {
    if (state_ == BufferState::kHaveNotRead) {
        cursor.line = 0;
        cursor.byte_offset = 0;
        cursor.b_view_col_want = 0;
        return;
    }

    cursor.line = std::min<int64_t>(lines_.size() - 1, cursor_state_line_);
    cursor.byte_offset = std::min<int64_t>(lines_[cursor.line].line_str.size(),
                                           cursor_state_byte_offset_);
    cursor.b_view_col_want = cursor_state_b_view_col_want_;
}

void Buffer::Modified() {
    assert(IsLoad() && !read_only());
    state_ = BufferState::kModified;
    version_++;
}

}  // namespace mango