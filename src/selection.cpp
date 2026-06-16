#include "selection.h"

#include "buffer.h"

namespace charxed {

Range NormalSelection::ToSelectRange(const Buffer* buffer) const {
    (void)buffer;
    Range res;
    if (anchor < head) {
        res.begin = anchor;
        res.end = head;
    } else {
        res.begin = head;
        res.end = anchor;
    }
    if (inclusive_end) {
        auto iter = buffer->Find(res.end);
        if (iter != buffer->End()) {
            Character c;
            auto next = NextCharacter(iter, buffer->End(), c);
            if (char ascii_c; c.Ascii(ascii_c) && ascii_c == '\n') {
                res.end.line++;
                res.end.byte_offset = 0;
            } else {
                res.end.byte_offset += next.offset() - iter.offset();
            }
        }
    }
    return res;
}

Range LineSelection::ToSelectRange(const Buffer* buffer) const {
    Range r;
    if (anchor < head) {
        r.begin = {anchor.line, 0};
        r.end = {head.line, buffer->GetLineView(head.line).Size()};
    } else {
        r.begin = {head.line, 0};
        r.end = {anchor.line, buffer->GetLineView(anchor.line).Size()};
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
        r.begin.byte_offset = buffer->GetLineView(r.begin.line).Size();
    }
    return r;
}

}  // namespace charxed
