#include "selection.h"

#include "buffer.h"

namespace mango {

// Vim selection is [begin, end], different with mango, so we tweak the range
// here.
Range VimSelection::ToRange(const Buffer* buffer) const {
    Range r;
    if (anchor < head) {
        r.begin = anchor;
        r.end = head;
    } else {
        r.begin = head;
        r.end = anchor;
    }
    const std::string& line = buffer->GetLine(r.end.line);
    if (r.end.byte_offset == line.size()) {
        if (buffer->LineCnt() - 1 == r.end.line) {
            return r;
        }
        r.end.line++;
        r.end.byte_offset = 0;
        return r;
    }
    Character c;
    int byte_len;
    ThisCharacter(line, r.end.byte_offset, c, byte_len);
    r.end.byte_offset += byte_len;
    return r;
}

Range VimLineSelection::ToRange(const Buffer* buffer) const {
    Range r;
    if (anchor < head) {
        r.begin = {anchor.line, 0};
        r.end = {head.line + 1, 0};
    } else {
        r.begin = {head.line, 0};
        r.end = {anchor.line + 1, 0};
    }
    if (r.end.line == buffer->LineCnt()) {
        r.end = {r.end.line - 1, buffer->GetLine(r.end.line - 1).size()};
    }
    return r;
}
}  // namespace mango
