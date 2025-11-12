#pragma once

#include <cstddef>

namespace mango {

// A Pos object represent a position in the line
struct Pos {
    size_t line;
    size_t byte_offset;
};
inline bool operator==(const Pos& pos1, const Pos& pos2) noexcept {
    return pos1.byte_offset == pos2.byte_offset && pos1.line == pos2.line;
}
inline bool operator<(const Pos& pos1, const Pos& pos2) noexcept {
    return pos1.line < pos2.line ||
           (pos1.line == pos2.line && pos1.byte_offset < pos2.byte_offset);
}

// Range represents a text range: [begin, end)
// NOTE:
// e.g. if the first line contains "abc", Use {{0, 0}, {0, 3}} to rep a line,
// Use {{0, 0}, {1, 0}} to rep the whole line with a '\n'.
struct Range {
    Pos begin;
    Pos end;

    bool PosBeforeMe(const Pos& pos) const { return pos < begin; }
    bool PosAfterMe(const Pos& pos) const { return !(pos < end); }

    // Pos is in Range ?
    bool PosInMe(const Pos& pos) const {
        return !(PosAfterMe(pos) || PosBeforeMe(pos));
    }

    bool RangeBeforeMe(const Range& range) {
        return range.end == begin || range.end < begin;
    }
    bool RangeAfterMe(const Range& range) {
        return end == range.begin || end < range.begin;
    }
    bool RangeOverlapMe(const Range& range) {
        return !RangeBeforeMe(range) && !RangeAfterMe(range);
    }
    bool RangeEqualMe(const Range& range) {
        return range.begin == begin && range.end == end;
    }
};

}