#pragma once

#include <regex.h>

#include <optional>
#include <string>
#include <vector>

#include "pos.h"
#include "utils.h"

namespace charxed {

struct SearchState {
    size_t i = 0;  // from 1 instead of zero
    size_t total = 0;
};

class Buffer;

// return true if success
bool BuildRegContext(const std::string& pattern, bool ignore_case,
                     regex_t& regex);

std::vector<Range> BufferSearch(const Buffer* buffer, const Range* range,
                                const std::string& pattern, bool ignore_case,
                                bool ensure_grapheme_cluster_boundary);

struct BufferSearchContext {
    std::vector<Range> search_result;
    int64_t current_search = -1;
    std::string search_pattern;
    int64_t search_buffer_version = -1;
    int64_t search_buffer_id = -1;
    Buffer* b;
    std::optional<Range> range;

    BufferSearchContext() = default;
    // if the pattern is a empty string, context will in empty state
    BufferSearchContext(const std::string& pattern, const Buffer* buffer,
                        const Range* range);
    void Destroy();
    bool EnsureSearched(const Buffer* buffer);
    bool NearestSearchPos(Pos pos, const Buffer* buffer, bool next,
                          size_t count, bool keep_current_if_one);
};

struct ListEntrySearchResult {
    size_t index;  // index in list
    struct {
        // Although multiple ranges in one list entry can be matched by a
        // pattern, we only care the first one.
        size_t offset_begin;
        size_t offset_end;
    } range;
};
// T should have a Str() function and return std::string_view
template <typename T>
std::vector<ListEntrySearchResult> ListSearch(const std::vector<T>& list,
                                              const std::string& pattern,
                                              bool ignore_case) {
    regex_t regex;
    if (!BuildRegContext(pattern, ignore_case, regex)) {
        return {};
    }

    std::vector<ListEntrySearchResult> res;
    for (size_t i = 0; i < list.size(); i++) {
        std::string_view str;
        if constexpr (std::is_pointer<T>()) {
            str = list[i]->Str();
        } else {
            str = list[i].Str();
        }
        regmatch_t m;
        m.rm_so = 0;
        m.rm_eo = str.size();
        int ret = regexec(&regex, str.data(), 1, &m, REG_STARTEND);
        // See search.cpp BufferSearch
        if (ret == REG_NOMATCH || m.rm_eo == m.rm_so) {
            continue;
        }
        // Unlike Buffer Search, List don't support replace to the search range,
        // so grapheme boundry is not important here.
        res.push_back(
            {i, {static_cast<size_t>(m.rm_so), static_cast<size_t>(m.rm_eo)}});
    }
    regfree(&regex);
    return res;
}

// T should have a Str() function and return std::string_view
// Ref BufferSearchContext
template <typename T>
struct ListSearchContext {
    std::vector<ListEntrySearchResult> search_result;
    int64_t current_search = -1;
    std::string search_pattern;
    int64_t search_version = -1;

    ListSearchContext() = default;
    // if the pattern is a empty string, context will in empty state
    ListSearchContext(const std::string& pattern, const std::vector<T>& list,
                      int64_t cur_verion) {
        if (pattern.empty()) {
            return;
        }
        search_pattern = pattern;
        search_result = ListSearch(list, pattern, true);
        search_version = cur_verion;
    }
    void Destroy() {
        search_pattern.clear();
        search_result.clear();
        search_version = -1;
    }
    bool EnsureSearched(const std::vector<T>& list, int64_t cur_version) {
        if (search_version == -1) {
            return false;
        }

        if (search_version != cur_version) {
            search_version = cur_version;
            search_result = ListSearch(list, search_pattern, true);
        }
        return !search_result.empty();
    }
    bool NearestSearchPos(size_t index, const std::vector<T>& list,
                          int64_t cur_version, bool next, size_t count,
                          bool keep_current_if_one) {
        CHX_ASSERT(count != 0);
        bool has_result = EnsureSearched(list, cur_version);
        if (!has_result) {
            return false;
        }

        // Search an insert pos
        size_t insert_i =
            std::lower_bound(search_result.begin(), search_result.end(), index,
                             [](const ListEntrySearchResult& r, size_t index) {
                                 return r.index < index;
                             }) -
            search_result.begin();

        if (next) {
            if (insert_i == search_result.size()) {
                insert_i = 0;
            } else if (index == search_result[insert_i].index &&
                       !(keep_current_if_one && count == 1)) {
                insert_i = (insert_i + 1) % search_result.size();
            }
            insert_i = (insert_i + count - 1) % search_result.size();
        } else {
            if (insert_i == search_result.size()) {
                insert_i--;
            } else if (search_result[insert_i].index == index &&
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
};
}  // namespace charxed
