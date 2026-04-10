#include "selection.h"

#include "buffer.h"

namespace mango {

// selection is [anchor, head], different with but Range is [begin, end), so we
// tweak the range here.
Range NormalSelection::ToSelectRange(const Buffer* buffer) const {
    Range r;
    if (anchor < head) {
        r.begin = anchor;
        r.end = head;
    } else {
        r.begin = head;
        r.end = anchor;
    }
    const auto& line = buffer->GetLine(r.end.line);
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

Range LineSelection::ToSelectRange(const Buffer* buffer) const {
    Range r;
    if (anchor < head) {
        r.begin = {anchor.line, 0};
        r.end = {head.line, buffer->GetLine(head.line).size()};
    } else {
        r.begin = {head.line, 0};
        r.end = {anchor.line, buffer->GetLine(anchor.line).size()};
    }
    return r;
}

Range LineSelection::ToDeleteRange(const Buffer* buffer) const {
    // Add a '\n' after or before the range if possible
    Range r = ToSelectRange(buffer);
    if (r.end.line < buffer->LineCnt() - 1) {
        r.end.line++;
        r.end.byte_offset = 0;
    } else if (r.begin.line != 0) {
        r.begin.line--;
        r.begin.byte_offset = buffer->GetLine(r.begin.line).size();
    }
    return r;
}

}  // namespace mango
