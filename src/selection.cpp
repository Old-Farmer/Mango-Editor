#include "selection.h"

#include "buffer.h"

namespace mango {

Range NormalSelection::ToSelectRange(const Buffer* buffer) const {
    (void)buffer;
    if (anchor < head) {
        return Range{anchor, head};
    } else {
        return Range{head, anchor};
    }
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
