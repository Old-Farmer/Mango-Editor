#include "selection.h"

#include "buffer.h"

namespace mango {

// Vim selection is [begin, end], different with mango, so we tweak the range
// here.
Range VimSelection::ToSelectRange(const Buffer* buffer) const {
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

Range VimLineSelection::ToSelectRange(const Buffer* buffer) const {
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

Range VimLineSelection::ToDeleteRange(const Buffer* buffer) const {
    Range r = ToSelectRange(buffer);
    if (r.end.line < buffer->LineCnt() - 1) {
        r.end.line++;
        r.end.byte_offset = 0;
    }
    return r;
}

}  // namespace mango
