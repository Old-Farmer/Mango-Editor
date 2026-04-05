#include "search.h"

#include "buffer.h"

namespace mango {

BufferSearchContext::BufferSearchContext(const std::string& pattern,
                                         const Buffer* buffer) {
    if (pattern.empty()) {
        return;
    }
    search_pattern = pattern;
    search_result = buffer->Search(search_pattern);
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
        search_result = buffer->Search(search_pattern);
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
