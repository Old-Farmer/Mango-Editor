#include "buffer.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <gsl/util>

#include "coding.h"
#include "cursor.h"
#include "exception.h"
#include "filetype.h"
#include "logging.h"
#include "options.h"

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

std::string File::ReadAll() {
    constexpr size_t size = 4096;
    char buf[size];
    std::string ret;
    while (true) {
        size_t s = fread(buf, 1, size, file_);
        ret.append(buf, s);
        if (s < size) {
            if (feof(file_)) {
                return ret;
            } else if (feof(file_)) {
                throw IOException("%s", strerror(errno));
            } else {
                assert(false);
            }
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

int64_t Buffer::cur_buffer_id_ = 0;

Buffer::Buffer(Options* options) : options_(options) {}

Buffer::Buffer(Options* options, std::string path, bool read_only)
    : path_(std::move(path)), read_only_(read_only), options_(options) {}

Buffer::Buffer(Options* options, Path path, bool read_only)
    : path_(std::move(path)), read_only_(read_only), options_(options) {}

void Buffer::Load() {
    try {
        lines_.clear();

        if (path_.Empty()) {
            lines_.push_back({});
            state_ = BufferState::kNotModified;
            return;
        }

        File f(path_.AbsolutePath(), "r", true);
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
    File swap_file = File(swap_file_path, "w", true);

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

void Buffer::Edit(const BufferEdit& edit, Pos& pos) {
    if (edit.str.empty()) {
        // delete
        DeleteInner(edit.range, pos, false, true);
    } else if (edit.range.begin == edit.range.end) {
        // add
        AddInner(edit.range.begin, edit.str, pos, true);
    } else {
        // replace
        ReplaceInner(edit.range, edit.str, pos, false);
    }
}

void Buffer::AddInner(const Pos& pos, const std::string& str, Pos& out_pos,
                      bool record_ts_edit) {
    auto _ = gsl::finally([this, record_ts_edit, &pos, &out_pos] {
        Modified();
        if (record_ts_edit) {
            ts_edit_.start_point.row = pos.line;
            ts_edit_.start_point.column = pos.byte_offset;
            ts_edit_.old_end_point.row = pos.line;
            ts_edit_.old_end_point.column = pos.byte_offset;
            ts_edit_.new_end_point.row = out_pos.line;
            ts_edit_.new_end_point.column = out_pos.byte_offset;
        }
    });

    size_t i = 0;
    out_pos = pos;
    std::vector<size_t> new_line_offset;
    for (; i < str.size(); i++) {
        if (str[i] == kNewLineChar) {
            new_line_offset.push_back(i);
        }
    }
    // No newline, just insert
    if (new_line_offset.empty()) {
        assert(lines_.size() > out_pos.line);
        assert(lines_[out_pos.line].line_str.size() >= out_pos.byte_offset);

        lines_[out_pos.line].line_str.insert(out_pos.byte_offset, str);
        out_pos.byte_offset += str.size();
        return;
    }

    // Have newline
    assert(lines_.size() > out_pos.line);
    assert(lines_[out_pos.line].line_str.size() >= out_pos.byte_offset);

    std::string line_after_pos =
        lines_[out_pos.line].line_str.substr(out_pos.byte_offset);
    lines_[out_pos.line].line_str.erase(out_pos.byte_offset);
    i = 0;
    for (size_t offset : new_line_offset) {
        if (offset != i) {
            assert(lines_.size() > out_pos.line);

            lines_[out_pos.line].line_str.append(str, i, offset - i);
        }
        out_pos.line++;
        out_pos.byte_offset = 0;

        assert(lines_.size() >= out_pos.line);

        // Use Line() instead of {} to prevent c++ infer as init list
        lines_.insert(lines_.begin() + out_pos.line, Line());
        i = offset + 1;
    }
    if (new_line_offset.back() != str.size() - 1) {
        assert(lines_.size() > out_pos.line);

        size_t left_size = str.size() - (new_line_offset.back() + 1);
        lines_[out_pos.line].line_str.append(str, new_line_offset.back() + 1,
                                             left_size);
        out_pos.byte_offset = lines_[out_pos.line].line_str.size();
    }
    lines_[out_pos.line].line_str.append(line_after_pos);
}

std::string Buffer::DeleteInner(const Range& range, Pos& pos, bool record,
                                bool record_ts_edit) {
    auto _ = gsl::finally([this] { Modified(); });

    std::string old_str;

    Pos end = range.end;
    while (range.begin.line <= end.line) {
        if (range.begin.line < end.line) {
            if (end.byte_offset == lines_[end.line].line_str.size()) {
                // whole line deleted
                assert(lines_.size() > end.line);
                if (record)
                    old_str.insert(
                        0, std::string(kNewLine) + lines_[end.line].line_str);

                lines_.erase(lines_.begin() + end.line);

                end.byte_offset = lines_[end.line - 1].line_str.size();
            } else if (end.byte_offset == 0) {
                // Merge two lines
                assert(lines_.size() > end.line);
                if (record) old_str.insert(0, kNewLine);

                end.byte_offset = lines_[end.line - 1].line_str.size();

                lines_[end.line - 1].line_str.append(lines_[end.line].line_str);
                lines_.erase(lines_.begin() + end.line);
            } else {
                // Delete the part of the line before end.byte_offset
                if (record)
                    old_str.insert(0, lines_[end.line].line_str, 0,
                                   end.byte_offset);

                assert(lines_.size() > end.line);
                assert(lines_[end.line].line_str.size() >= end.byte_offset);
                lines_[end.line].line_str.erase(0, end.byte_offset);

                end.byte_offset = lines_[end.line - 1].line_str.size();
            }
        } else {
            assert(lines_.size() > end.line);
            assert(lines_[end.line].line_str.size() >= end.byte_offset);
            if (record)
                old_str.insert(0, lines_[end.line].line_str,
                               range.begin.byte_offset,
                               end.byte_offset - range.begin.byte_offset);

            lines_[end.line].line_str.erase(
                range.begin.byte_offset,
                end.byte_offset - range.begin.byte_offset);
        }

        if (end.line == 0) {
            break;
        }
        end.line--;
    }

    pos = range.begin;

    if (record_ts_edit) {
        ts_edit_.start_point.row = range.begin.line;
        ts_edit_.start_point.column = range.begin.byte_offset;
        ts_edit_.old_end_point.row = range.end.line;
        ts_edit_.old_end_point.column = range.end.byte_offset;
        ts_edit_.new_end_point.row = pos.line;
        ts_edit_.new_end_point.column = pos.byte_offset;
    }

    return old_str;
}

std::string Buffer::ReplaceInner(const Range& range, const std::string& str,
                                 Pos& pos, bool record) {
    Pos out_pos;
    std::string old_str = DeleteInner(range, out_pos, record, false);
    AddInner(out_pos, str, pos, false);

    ts_edit_.start_point.row = range.begin.line;
    ts_edit_.start_point.column = range.begin.byte_offset;
    ts_edit_.old_end_point.row = range.end.line;
    ts_edit_.old_end_point.column = range.end.byte_offset;
    ts_edit_.new_end_point.row = pos.line;
    ts_edit_.new_end_point.column = pos.byte_offset;

    return old_str;
}

void Buffer::Record(BufferEditHistoryItem item) {
    // Delete all history iterms after cursor(include the item which cursor
    // points to)
    if (edit_history_cursor_ != edit_history_->end()) {
        assert(!edit_history_->empty());
        edit_history_->erase(edit_history_cursor_, edit_history_->end());
        edit_history_cursor_ = edit_history_->end();
    }

    // No item in history, return fast
    assert(options_->buffer_hitstory_max_item_cnt > 0);
    if (edit_history_->size() == 0) {
        edit_history_->push_back(std::move(item));
        return;
    }

    // Can we merge adjacent edits?
    BufferEditHistoryItem& last_item = edit_history_->back();
    if (last_item.origin.str.empty() && item.origin.str.empty() &&
        last_item.origin.range.begin == item.origin.range.end) {
        // adjacent deletes
        last_item.origin.range.begin = item.origin.range.begin;
        last_item.reverse.range.begin = item.reverse.range.begin;
        last_item.reverse.range.end = item.reverse.range.end;
        last_item.reverse.str.insert(0, item.reverse.str);
        return;
    } else if (last_item.origin.range.begin == last_item.origin.range.end &&
               item.origin.range.begin == item.origin.range.end &&
               last_item.reverse.range.end == item.reverse.range.begin) {
        // adjacent adds
        last_item.reverse.range.end = item.reverse.range.end;
        last_item.origin.str.append(item.origin.str);
        return;
    }
    // THINK IT: adjacent replaces need to be merged?

    if (edit_history_->size() == options_->buffer_hitstory_max_item_cnt) {
        edit_history_->pop_front();
    }
    edit_history_->push_back(std::move(item));
}

Result Buffer::Add(const Pos& pos, std::string str, Pos& out_pos) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    AddInner(pos, str, out_pos, true);
    BufferEditHistoryItem item;
    item.origin.range = {pos, pos};
    item.origin.str = std::move(str);
    item.reverse.range = {pos, out_pos};
    Record(std::move(item));
    return kOk;
}

Result Buffer::Delete(const Range& range, Pos& pos) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    std::string old_str = DeleteInner(range, pos, true, true);
    BufferEditHistoryItem item;
    item.origin.range = range;
    item.reverse.range = {pos, pos};
    item.reverse.str = std::move(old_str);
    Record(std::move(item));
    return kOk;
}

Result Buffer::Replace(const Range& range, std::string str, Pos& pos) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    std::string old_str = ReplaceInner(range, str, pos, true);
    BufferEditHistoryItem item;
    item.origin.range = range;
    item.origin.str = std::move(str);
    item.reverse.range = {range.begin, pos};
    item.reverse.str = std::move(old_str);
    Record(std::move(item));
    return kOk;
}

Result Buffer::Redo(Pos& pos) {
    if (edit_history_->empty()) {
        return kNoHistoryAvailable;
    }
    if (edit_history_cursor_ == edit_history_->end()) {
        return kNoHistoryAvailable;
    }

    Edit(edit_history_cursor_->origin, pos);
    edit_history_cursor_++;
    return kOk;
}

Result Buffer::Undo(Pos& pos) {
    if (edit_history_->empty()) {
        return kNoHistoryAvailable;
    }
    if (edit_history_cursor_ == edit_history_->begin()) {
        return kNoHistoryAvailable;
    }

    if (edit_history_cursor_ == edit_history_->end()) {
        edit_history_cursor_ = --edit_history_->end();
    } else {
        edit_history_cursor_--;
    }

    Edit(edit_history_cursor_->reverse, pos);
    return kOk;
}

std::vector<Range> Buffer::Search(const std::string& pattern) {
    std::vector<Range> res;
    for (size_t line = 0; line < lines_.size(); line++) {
        size_t pos = 0;
        while (true) {
            pos = lines_[line].line_str.find(pattern, pos);
            if (pos == std::string::npos) {
                break;
            }
            res.push_back({{line, pos}, {line, pos + pattern.size()}});
            pos += pattern.size();
        }
    }
    return res;
}

size_t Buffer::Offset(const Pos& pos) const {
    size_t offset = 0;
    for (size_t i = 0; i < lines_.size(); i++) {
        if (i != pos.line) {
            offset += lines_[i].line_str.size() + 1;  // 1 is for '\n'
        } else {
            // ==
            offset += pos.byte_offset;
            break;
        }
    }
    return offset;
}

TSInputEdit Buffer::GetEditForTreeSitter() {
    ts_edit_.start_byte =
        Offset({ts_edit_.start_point.row, ts_edit_.start_point.column});

    if (ts_edit_.start_point.row == ts_edit_.old_end_point.row &&
        ts_edit_.start_point.column == ts_edit_.old_end_point.column) {
        ts_edit_.old_end_byte = ts_edit_.start_byte;
    } else {
        ts_edit_.old_end_byte =
            Offset({ts_edit_.old_end_point.row, ts_edit_.old_end_point.column});
    }

    if (ts_edit_.start_point.row == ts_edit_.new_end_point.row &&
        ts_edit_.start_point.column == ts_edit_.new_end_point.column) {
        ts_edit_.new_end_byte = ts_edit_.start_byte;
    } else {
        ts_edit_.new_end_byte =
            Offset({ts_edit_.new_end_point.row, ts_edit_.new_end_point.column});
    }

    return ts_edit_;
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