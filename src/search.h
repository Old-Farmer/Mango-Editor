#pragma once

#include <string>
#include <vector>

#include "pos.h"

namespace mango {

class Buffer;

struct BufferSearchContext {
    std::vector<Range> search_result;
    int64_t current_search = -1;
    std::string search_pattern;
    int64_t search_buffer_version = -1;
    int64_t search_buffer_id = -1;
    Buffer* b;

    BufferSearchContext() = default;
    // if the pattern is a empty string, context will in empty state
    BufferSearchContext(const std::string& pattern, const Buffer* buffer);
    void Destroy();
    bool EnsureSearched(const Buffer* buffer);
    bool NearestSearchPos(Pos pos, const Buffer* buffer,
                                        bool next, size_t count,
                                        bool keep_current_if_one);
};

struct BufferSearchState {
    size_t i = 0;  // from 1 instead of zero
    size_t total = 0;
};

}  // namespace mango
