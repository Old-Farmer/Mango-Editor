#include "search.h"

#include "buffer.h"

namespace charxed {

bool BuildRegContext(const std::string& pattern, bool ignore_case,
                     regex_t& regex) {
    Character c;
    // Because current reg engine only recognize code points, we also iter it
    // by code point.
    for (size_t i = 0; i < pattern.size(); i++) {
        if (isupper(pattern[i])) {
            ignore_case = false;
            break;
        }
    }
    int ret = regcomp(&regex, pattern.c_str(),
                      REG_EXTENDED | (ignore_case ? REG_ICASE : 0));
    // TODO: log?
    return ret == 0;
}

// Search in line
// str_begin_byte_offset is the byte_offset of the first byte of the str,
// line is the line index
__always_inline void LineSearch(const regex_t& regex, std::string_view str,
                                size_t line, size_t str_begin_byte_offset,
                                bool ensure_grapheme_cluster_boundary,
                                std::vector<Range>& res) {
    regmatch_t m;
    for (size_t pos = 0; pos < str.size();) {
        m.rm_so = pos;
        m.rm_eo = str.size();
        int ret = regexec(&regex, str.data(), 1, &m, REG_STARTEND);
        // No match or empty match
        // pattern like "a*" can have empty match, if empty match occur, no
        // more match in this line because of the leftmost longest strategy
        // of posix regex engine.
        // https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap09.html
        if (ret == REG_NOMATCH || m.rm_eo == m.rm_so) {
            break;
        }

        bool is_res = true;
        if (ensure_grapheme_cluster_boundary) {
            // We guarentee grapheme boundry for users because Posix and
            // almost all full-featured regex engine only care about
            // codepoints. But we'd better let users know the limitation of
            // the regex engine.
            if (!CharacterBoundaryValid(str, m.rm_so) ||
                !CharacterBoundaryValid(str, m.rm_eo)) {
                is_res = false;
            }
        }
        if (is_res) {
            res.push_back(
                {{line, static_cast<size_t>(m.rm_so) + str_begin_byte_offset},
                 {line, static_cast<size_t>(m.rm_eo) + str_begin_byte_offset}});
        }
        pos = m.rm_eo;
    }
}

std::vector<Range> BufferSearch(const Buffer* buffer, const Range* range,
                                const std::string& pattern, bool ignore_case,
                                bool ensure_grapheme_cluster_boundary) {
    regex_t regex;
    if (!BuildRegContext(pattern, ignore_case, regex)) {
        return {};
    }

    std::vector<Range> res;
    std::string buf;
    if (range == nullptr) {
        size_t line_cnt = buffer->LineCnt();
        for (size_t line = 0; line < line_cnt; line++) {
            const auto& line_str = buffer->GetLine(line, buf);
            LineSearch(regex, line_str, line, 0,
                       ensure_grapheme_cluster_boundary, res);
        }
    } else {
        for (size_t line = range->begin.line; line <= range->end.line; line++) {
            const auto& line_str = buffer->GetLine(line, buf);
            size_t line_begin_byte_offset = 0;
            auto fixed_line_str = line_str;
            if (line == range->begin.line) {
                line_begin_byte_offset = range->begin.byte_offset;
                if (line_begin_byte_offset >= line_str.size()) {
                    continue;
                }
                fixed_line_str = fixed_line_str.substr(line_begin_byte_offset);
            }
            if (line == range->end.line) {
                // should take care that range->begin & range->end is the same
                // line
                fixed_line_str = fixed_line_str.substr(
                    0, range->end.byte_offset -
                           (range->end.line == range->begin.line
                                ? range->begin.byte_offset
                                : 0));
            }
            LineSearch(regex, fixed_line_str, line, line_begin_byte_offset,
                       ensure_grapheme_cluster_boundary, res);
        }
    }
    regfree(&regex);
    return res;
}

BufferSearchContext::BufferSearchContext(const std::string& pattern,
                                         const Buffer* buffer,
                                         const Range* range) {
    if (pattern.empty()) {
        return;
    }
    search_pattern = pattern;
    search_result = BufferSearch(
        buffer, range, search_pattern,
        buffer->opts().global_opts_->GetOpt<bool>(kOptSearchIgnoreCase), true);
    search_buffer_version = buffer->version();
    if (range != nullptr) {
        this->range = *range;
    }
}

void BufferSearchContext::Destroy() {
    search_pattern.clear();
    search_result.clear();
    search_buffer_version = -1;
    search_buffer_id = -1;
}

bool BufferSearchContext::EnsureSearched(const Buffer* buffer) {
    if (search_buffer_version == -1) {
        return false;
    }

    if (buffer->id() != search_buffer_id ||
        buffer->version() != search_buffer_version) {
        // Another buffer or the buffer has changed, we do search again.
        search_result = BufferSearch(
            buffer, OptionalToPtr(range), search_pattern,
            buffer->opts().global_opts_->GetOpt<bool>(kOptSearchIgnoreCase),
            true);
        search_buffer_version = buffer->version();
        search_buffer_id = buffer->id();
        current_search = -1;
    }

    if (search_result.size() == 0) {
        return false;
    }
    return true;
}

bool BufferSearchContext::NearestSearchPos(Pos pos, const Buffer* buffer,
                                           bool next, size_t count,
                                           bool keep_current_if_one) {
    CHX_ASSERT(count != 0);
    bool has_result = EnsureSearched(buffer);
    if (!has_result) {
        return false;
    }

    // Search an insert pos
    size_t insert_i =
        std::lower_bound(
            search_result.begin(), search_result.end(), pos,
            [](Range& range, Pos pos) { return range.begin < pos; }) -
        search_result.begin();

    CHX_ASSERT(search_result.size() != 0);
    if (next) {
        if (insert_i == search_result.size()) {
            insert_i = 0;
        } else if (pos == search_result[insert_i].begin &&
                   !(keep_current_if_one && count == 1)) {
            insert_i = (insert_i + 1) % search_result.size();
        }
        insert_i = (insert_i + count - 1) % search_result.size();
    } else {
        if (insert_i == search_result.size()) {
            insert_i--;
        } else if (search_result[insert_i].begin == pos &&
                   (keep_current_if_one && count == 1)) {
            ;
        } else if (insert_i == 0) {
            insert_i = search_result.size() - 1;
        } else {
            insert_i--;
        }
        count = (count - 1) % search_result.size();
        if (count <= insert_i) {
            insert_i -= count;
        } else {
            insert_i = search_result.size() - (count - insert_i);
        }
    }
    current_search = insert_i;
    return true;
}
}  // namespace charxed
