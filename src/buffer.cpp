#include "buffer.h"

#include <cstdio>
#include <cstring>
#include <gsl/util>

#include "cursor.h"
#include "exception.h"
#include "filetype.h"
#include "logging.h"
#include "options.h"

namespace mango {

int64_t Buffer::cur_buffer_id_ = 0;
std::vector<bool> Buffer::new_file_alloced_ids_ = {};

Buffer::Buffer(GlobalOpts* global_opts, bool new_file) : opts_(global_opts) {
    if (new_file) {
        for (size_t i = 0; i < new_file_alloced_ids_.size(); i++) {
            if (!new_file_alloced_ids_[i]) {
                new_file_alloced_ids_[i] = true;
                new_file_id_ = i + 1;
                new_file_name_ =
                    "[new-file-" + std::to_string(new_file_id_) + "]";
                break;
            }
        }
        if (new_file_name_.empty()) {
            new_file_alloced_ids_.resize(new_file_alloced_ids_.size() + 1);
        }
        new_file_alloced_ids_.back() = true;
        new_file_id_ = new_file_alloced_ids_.size();
        new_file_name_ = "[new-file-" + std::to_string(new_file_id_) + "]";
    }
}

Buffer::Buffer(GlobalOpts* global_opts, std::string path, bool read_only)
    : path_(std::move(path)), read_only_(read_only), opts_(global_opts) {}

Buffer::Buffer(GlobalOpts* global_opts, Path path, bool read_only)
    : path_(std::move(path)), read_only_(read_only), opts_(global_opts) {}

Buffer::~Buffer() {
    if (!new_file_name_.empty()) {
        new_file_alloced_ids_[new_file_id_ - 1] = false;
    }
}

void Buffer::Load() {
    auto _ = gsl::finally([this] {
        opts_.InitAfterBufferLoad(this);
        if (GetOpt<bool>(kOptBasicWordCompletion)) {
            basic_word_completer_ =
                std::make_unique<BufferBasicWordCompleter>(this);
            basic_word_completer_->Enable();
        }
    });

    try {
        lines_.clear();

        if (path_.Empty()) {
            lines_.push_back({});
            state_ = BufferState::kNotModified;
            return;
        }

        File f(path_.AbsolutePath(), "r", true);
        MGO_LOG_DEBUG("file path %s", path_.AbsolutePath().c_str());

        while (true) {
            std::string buf;
            Result ret = f.ReadLine(buf, eol_seq_);
            if (!CheckUtf8Valid(buf)) {
                lines_.clear();
                lines_.push_back({});
                state_ = BufferState::kCodingInvalid;
                throw CodeingException("%s", "utf8 encoding error");
            }
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
    } catch (FileCreateException& e) {
        lines_.clear();
        lines_.push_back({});  // ensure one empty line
        state_ = BufferState::kCannotCreate;
        throw;
    } catch (IOException& e) {
        lines_.clear();
        lines_.push_back({});  // ensure one empty line
        state_ = BufferState::kCannotRead;
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

    if (read_only()) {
        return kBufferReadOnly;
    }

    std::string swap_file_path = path_.AbsolutePath() + kSwapSuffix;
    File swap_file = File(swap_file_path, "w", true);

    const char* eol_seq_str = eol_seq_ == EOLSeq::kLF ? kEOLSeqLF : kEOLSeqCRLF;
    int eol_seq_str_size = strlen(eol_seq_str);

    for (size_t i = 0; i < lines_.size(); i++) {
        if (!lines_[i].line_str.empty()) {
            size_t s = fwrite(lines_[i].line_str.c_str(), 1,
                              lines_[i].line_str.size(), swap_file.file());
            if (s < lines_[i].line_str.size()) {
                throw IOException("fwrite error: %s", strerror(errno));
            }
        }
        if (i != lines_.size() - 1) {
            size_t s =
                fwrite(eol_seq_str, 1, eol_seq_str_size, swap_file.file());
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

std::string Buffer::GetContent(const Range& range) const {
    MGO_ASSERT(LineCnt() > range.end.line);
    Pos begin = range.begin;
    size_t total_size = 0;
    while (begin.line <= range.end.line) {
        if (begin.line == range.end.line) {
            total_size += range.end.byte_offset - begin.byte_offset;
            break;
        }
        total_size += lines_[begin.line].line_str.size() - begin.byte_offset;
        total_size += 1;  // '\n'
        begin.line++;
        begin.byte_offset = 0;
    }
    std::string ret;
    ret.reserve(total_size);
    begin = range.begin;
    while (begin.line <= range.end.line) {
        if (begin.line == range.end.line) {
            ret.append(lines_[begin.line].line_str, begin.byte_offset,
                       range.end.byte_offset - begin.byte_offset);
            break;
        }
        ret.append(lines_[begin.line].line_str);
        ret.append(1, '\n');
        begin.line++;
        begin.byte_offset = 0;
    }
    return ret;
}

void Buffer::Edit(const BufferEdit& edit, Pos& cursor_pos_hint) {
    if (edit.str.empty()) {
        // delete
        DeleteInner(edit.range, cursor_pos_hint, false, true);
    } else if (edit.range.begin == edit.range.end) {
        // add
        AddInner(edit.range.begin, edit.str, cursor_pos_hint, true);
    } else {
        // replace
        ReplaceInner(edit.range, edit.str, cursor_pos_hint, false);
    }
}

void Buffer::AddInner(const Pos& pos, const std::string& str,
                      Pos& cursor_pos_hint, bool record_ts_edit) {
    auto _ = gsl::finally([this, record_ts_edit, &pos, &cursor_pos_hint, &str] {
        Modified();
        if (record_ts_edit) {
            ts_edit_.start_point.row = pos.line;
            ts_edit_.start_point.column = pos.byte_offset;
            ts_edit_.old_end_point.row = pos.line;
            ts_edit_.old_end_point.column = pos.byte_offset;
            ts_edit_.new_end_point.row = cursor_pos_hint.line;
            ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
            ts_edit_.start_byte = OffsetAndInvalidAfterPos(pos);
            ts_edit_.old_end_byte = ts_edit_.start_byte;
            ts_edit_.new_end_byte = ts_edit_.start_byte + str.size();
        }
    });

    size_t i = 0;
    cursor_pos_hint = pos;
    std::vector<size_t> new_line_offset;
    for (; i < str.size(); i++) {
        if (str[i] == '\n') {
            new_line_offset.push_back(i);
        }
    }
    // No newline, just insert
    if (new_line_offset.empty()) {
        MGO_ASSERT(lines_.size() > cursor_pos_hint.line);
        MGO_ASSERT(lines_[cursor_pos_hint.line].line_str.size() >=
                   cursor_pos_hint.byte_offset);

        lines_[cursor_pos_hint.line].line_str.insert(
            cursor_pos_hint.byte_offset, str);
        cursor_pos_hint.byte_offset += str.size();
        return;
    }

    // Have newline
    MGO_ASSERT(lines_.size() > cursor_pos_hint.line);
    MGO_ASSERT(lines_[cursor_pos_hint.line].line_str.size() >=
               cursor_pos_hint.byte_offset);

    std::string line_after_pos = lines_[cursor_pos_hint.line].line_str.substr(
        cursor_pos_hint.byte_offset);
    lines_[cursor_pos_hint.line].line_str.erase(cursor_pos_hint.byte_offset);
    i = 0;
    for (size_t offset : new_line_offset) {
        if (offset != i) {
            MGO_ASSERT(lines_.size() > cursor_pos_hint.line);

            lines_[cursor_pos_hint.line].line_str.append(str, i, offset - i);
        }
        cursor_pos_hint.line++;
        cursor_pos_hint.byte_offset = 0;

        MGO_ASSERT(lines_.size() >= cursor_pos_hint.line);

        // Use Line() instead of {} to prevent c++ infer as init list
        lines_.insert(lines_.begin() + cursor_pos_hint.line, Line());
        i = offset + 1;
    }
    if (new_line_offset.back() != str.size() - 1) {
        MGO_ASSERT(lines_.size() > cursor_pos_hint.line);

        size_t left_size = str.size() - (new_line_offset.back() + 1);
        lines_[cursor_pos_hint.line].line_str.append(
            str, new_line_offset.back() + 1, left_size);
        cursor_pos_hint.byte_offset =
            lines_[cursor_pos_hint.line].line_str.size();
    }
    lines_[cursor_pos_hint.line].line_str.append(line_after_pos);
}

std::string Buffer::DeleteInner(const Range& range, Pos& cursor_pos_hint,
                                bool record_reverse, bool record_ts_edit) {
    auto _ = gsl::finally([this] { Modified(); });

    std::string old_str;
    size_t old_str_size = 0;

    std::string line_where_end_pos_locate;

    Pos end = range.end;
    MGO_ASSERT(lines_.size() > end.line);
    MGO_ASSERT(range.begin.line < end.line ||
               (range.begin.line == range.end.line &&
                range.begin.byte_offset <= range.end.byte_offset));
    while (range.begin.line <= end.line) {
        if (range.begin.line < end.line) {
            if (end.byte_offset == lines_[end.line].line_str.size()) {
                // whole line deleted
                if (record_reverse) {
                    old_str.insert(
                        0, std::string("\n") + lines_[end.line].line_str);
                }
                old_str_size += 1 + lines_[end.line].line_str.size();

                lines_.erase(lines_.begin() + end.line);

                end.byte_offset = lines_[end.line - 1].line_str.size();
            } else {
                // Delete the part of the line before end.byte_offset
                // and merge with the line where begin pos is located.
                // But we don't do merge here, we just move away this line and
                // merge after deletion
                if (record_reverse) {
                    MGO_ASSERT(end.byte_offset <
                               lines_[end.line].line_str.size());
                    old_str.insert(0, lines_[end.line].line_str, 0,
                                   end.byte_offset);
                    old_str.insert(0, "\n");
                }
                old_str_size += 1 + end.byte_offset;

                line_where_end_pos_locate =
                    std::move(lines_[end.line].line_str);

                end.byte_offset = lines_[end.line - 1].line_str.size();
                lines_.erase(lines_.begin() + end.line);
            }
        } else {
            MGO_ASSERT(lines_[end.line].line_str.size() >= end.byte_offset);
            if (record_reverse)
                old_str.insert(0, lines_[end.line].line_str,
                               range.begin.byte_offset,
                               end.byte_offset - range.begin.byte_offset);
            old_str_size += end.byte_offset - range.begin.byte_offset;

            lines_[end.line].line_str.erase(
                range.begin.byte_offset,
                end.byte_offset - range.begin.byte_offset);
        }

        if (end.line == 0) {
            break;
        }
        end.line--;
    }

    // Maybe merge lines
    if (!line_where_end_pos_locate.empty()) {
        lines_[range.begin.line].line_str.append(
            line_where_end_pos_locate, range.end.byte_offset,
            line_where_end_pos_locate.size() - range.end.byte_offset);
    }

    cursor_pos_hint = range.begin;

    if (record_ts_edit) {
        ts_edit_.start_point.row = range.begin.line;
        ts_edit_.start_point.column = range.begin.byte_offset;
        ts_edit_.old_end_point.row = range.end.line;
        ts_edit_.old_end_point.column = range.end.byte_offset;
        ts_edit_.new_end_point.row = cursor_pos_hint.line;
        ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
        ts_edit_.start_byte = OffsetAndInvalidAfterPos(range.begin);
        ts_edit_.old_end_byte = ts_edit_.start_byte + old_str_size;
        ts_edit_.new_end_byte = ts_edit_.start_byte;
    }

    return old_str;
}

std::string Buffer::ReplaceInner(const Range& range, const std::string& str,
                                 Pos& cursor_pos_hint, bool record_reverse) {
    Pos out_pos;
    std::string old_str = DeleteInner(range, out_pos, record_reverse, true);
    AddInner(out_pos, str, cursor_pos_hint, false);

    // Others record in DeleteInner
    ts_edit_.new_end_point.row = cursor_pos_hint.line;
    ts_edit_.new_end_point.column = cursor_pos_hint.byte_offset;
    ts_edit_.new_end_byte = ts_edit_.start_byte + str.size();

    return old_str;
}

void Buffer::Record(BufferEditHistoryItem item) {
    // Delete all history iterms after cursor(include the item which cursor
    // points to)
    if (edit_history_cursor_ != edit_history_->end()) {
        MGO_ASSERT(!edit_history_->empty());
        edit_history_->erase(edit_history_cursor_, edit_history_->end());
        edit_history_cursor_ = edit_history_->end();
    }

    // No item in history, return fast
    MGO_ASSERT(GetOpt<int64_t>(kOptMaxEditHistory) > 0);
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
        last_item.reverse_pos_hint = item.reverse_pos_hint;
        return;
    } else if (last_item.origin.range.begin == last_item.origin.range.end &&
               item.origin.range.begin == item.origin.range.end &&
               last_item.reverse.range.end == item.reverse.range.begin) {
        // adjacent adds
        last_item.reverse.range.end = item.reverse.range.end;

        last_item.origin.str.append(item.origin.str);
        last_item.origin_pos_hint = item.origin_pos_hint;
        return;
    }
    // THINK IT: adjacent replaces need to be merged?

    if (edit_history_->size() ==
        static_cast<size_t>(GetOpt<int64_t>(kOptMaxEditHistory))) {
        edit_history_->pop_front();
        never_wrap_history_ = false;
    }
    edit_history_->push_back(std::move(item));
}

Result Buffer::Add(const Pos& pos, std::string str, const Pos* cursor_pos,
                   bool use_given_pos_hint, Pos& cursor_pos_hint) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    Pos origin_pos_hint;
    AddInner(pos, str, origin_pos_hint, true);
    if (!use_given_pos_hint) {
        cursor_pos_hint = origin_pos_hint;
    }
    BufferEditHistoryItem item;
    item.origin.range = {pos, pos};
    item.origin.str = std::move(str);
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.range = {pos, origin_pos_hint};
    item.reverse_pos_hint = cursor_pos ? *cursor_pos : pos;
    Record(std::move(item));
    return kOk;
}

Result Buffer::Delete(const Range& range, const Pos* cursor_pos,
                      Pos& cursor_pos_hint) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }
    std::string old_str = DeleteInner(range, cursor_pos_hint, true, true);
    BufferEditHistoryItem item;
    item.origin.range = range;
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.range = {cursor_pos_hint, cursor_pos_hint};
    item.reverse.str = std::move(old_str);
    item.reverse_pos_hint = cursor_pos ? *cursor_pos : range.end;
    Record(std::move(item));
    return kOk;
}

Result Buffer::Replace(const Range& range, std::string str,
                       const Pos* cursor_pos, bool use_given_pos_hint,
                       Pos& cursor_pos_hint) {
    if (!IsLoad()) {
        return kBufferCannotLoad;
    }
    if (read_only()) {
        return kBufferReadOnly;
    }

    Pos origin_pos_hint;
    std::string old_str = ReplaceInner(range, str, origin_pos_hint, true);
    if (!use_given_pos_hint) {
        cursor_pos_hint = origin_pos_hint;
    }

    BufferEditHistoryItem item;
    item.origin.range = range;
    item.origin.str = std::move(str);
    item.origin_pos_hint = cursor_pos_hint;

    item.reverse.range = {range.begin, cursor_pos_hint};
    item.reverse.str = std::move(old_str);
    item.reverse_pos_hint = cursor_pos ? *cursor_pos : range.end;
    Record(std::move(item));
    return kOk;
}

Result Buffer::Redo(Pos& cursor_pos_hint) {
    if (edit_history_->empty()) {
        return kNoHistoryAvailable;
    }
    if (edit_history_cursor_ == edit_history_->end()) {
        return kNoHistoryAvailable;
    }

    Edit(edit_history_cursor_->origin, cursor_pos_hint);
    cursor_pos_hint = edit_history_cursor_->origin_pos_hint;
    edit_history_cursor_++;
    return kOk;
}

Result Buffer::Undo(Pos& cursor_pos_hint) {
    if (edit_history_->empty()) {
        return kNoHistoryAvailable;
    }
    if (edit_history_cursor_ == edit_history_->begin()) {
        return kNoHistoryAvailable;
    }

    edit_history_cursor_--;
    Edit(edit_history_cursor_->reverse, cursor_pos_hint);
    cursor_pos_hint = edit_history_cursor_->reverse_pos_hint;
    if (edit_history_cursor_ == edit_history_->begin() && never_wrap_history_ &&
        state_ == BufferState::kModified) {
        state_ = BufferState::kNotModified;
    }
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

size_t Buffer::OffsetAndInvalidAfterPos(const Pos& pos) {
    if (offset_per_line_.size() > pos.line + 1) {
        offset_per_line_.resize(pos.line + 1);
    } else {
        size_t offset = offset_per_line_.back();
        size_t i = offset_per_line_.size();
        offset_per_line_.resize(pos.line + 1);
        while (i < pos.line + 1) {
            offset += GetLine(i - 1).size() + 1;  // 1 for '\n'
            offset_per_line_[i++] = offset;
        }
    }
    return offset_per_line_.back() + pos.byte_offset;
}

TSInputEdit Buffer::GetEditForTreeSitter() { return ts_edit_; }

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

bool Buffer::IsLastBuffer() const { return next_->next_ == nullptr; }
bool Buffer::IsFirstBuffer() const { return prev_->prev_ == nullptr; }

void Buffer::Modified() {
    MGO_ASSERT(IsLoad() && !read_only());
    state_ = BufferState::kModified;
    version_++;
}

}  // namespace mango