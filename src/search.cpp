#include "search.h"

#include <regex.h>

#include "buffer.h"

namespace mango {

std::vector<Range> BufferSearch(const Buffer* buffer,
                                const std::string& pattern, bool ignore_case) {
    Character c;
    int byte_len;
    char asc;
    for (size_t i = 0; i < pattern.size(); i += byte_len) {
        ThisCharacter(pattern, i, c, byte_len);
        if (c.Ascii(asc) && asc >= 'A' && asc <= 'Z') {
            ignore_case = false;
            break;
        }
    }
    std::vector<Range> res;
    regex_t regex;
    int ret = regcomp(&regex, pattern.c_str(),
                      REG_EXTENDED | (ignore_case ? REG_ICASE : 0));
    if (ret != 0) {
        return {};
    }

    size_t line_cnt = buffer->LineCnt();
    for (size_t line = 0; line < line_cnt; line++) {
        const auto& line_str = buffer->GetLine(line);
        regmatch_t m;
        for (size_t pos = 0; pos < line_str.size();) {
            m.rm_so = pos;
            m.rm_eo = line_str.size();
            int ret = regexec(&regex, line_str.data(), 1, &m, REG_STARTEND);
            // No match or empty match
            // pattern like "a*" can have empty match, if empty match occur, no
            // more match in this line because of the leftmost longest strategy
            // of posix regex engine.
            // https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap09.html
            if (ret == REG_NOMATCH || m.rm_eo == m.rm_so) {
                break;
            }

            // We guarentee grapheme boundry for users because Posix and
            // almost all full-featured regex engine only care about codepoints.
            // But we'd better let users know the limitation of the regex
            // engine.
            if (CharacterBoundaryValid(line_str, m.rm_so) &&
                CharacterBoundaryValid(line_str, m.rm_eo)) {
                res.push_back({{line, static_cast<size_t>(m.rm_so)},
                               {line, static_cast<size_t>(m.rm_eo)}});
            }
            pos = m.rm_eo;
        }
    }
    regfree(&regex);
    return res;
}

BufferSearchContext::BufferSearchContext(const std::string& pattern,
                                         const Buffer* buffer) {
    if (pattern.empty()) {
        return;
    }
    search_pattern = pattern;
    search_result = BufferSearch(
        buffer, search_pattern,
        buffer->opts().global_opts_->GetOpt<bool>(kOptSearchIgnoreCase));
    search_buffer_version = buffer->version();
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
            buffer, search_pattern,
            buffer->opts().global_opts_->GetOpt<bool>(kOptSearchIgnoreCase));
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
    MGO_ASSERT(count != 0);
    bool has_result = EnsureSearched(buffer);
    if (!has_result) {
        return false;
    }

    // Search an insert pos
    int64_t left = 0, right = search_result.size() - 1;
    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        if (pos == search_result[mid].begin) {
            left = mid;
            right = left - 1;
        } else if (pos < search_result[mid].begin) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    if (next) {
        if (static_cast<size_t>(left) == search_result.size()) {
            left = 0;
        } else if (pos == search_result[left].begin &&
                   !(keep_current_if_one && count == 1)) {
            left = (left + 1) % search_result.size();
        }
        left = (left + count - 1) % search_result.size();
    } else {
        if (static_cast<size_t>(left) == search_result.size()) {
            left--;
        } else if (search_result[left].begin == pos &&
                   (keep_current_if_one && count == 1)) {
            ;
        } else if (static_cast<size_t>(left) == 0) {
            left = search_result.size() - 1;
        } else {
            left--;
        }
        count = (count - 1) % search_result.size();
        if (count <= static_cast<size_t>(left)) {
            left -= count;
        } else {
            left = search_result.size() - (count - left);
        }
    }
    pos = search_result[left].begin;
    current_search = left;
    return true;
}
}  // namespace mango
