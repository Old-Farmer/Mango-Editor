#pragma once

#include "pos.h"

namespace mango {

class Buffer;

struct Selection {
    Pos anchor;
    Pos head;

    Selection() {}
    Selection(Pos _anchor, Pos _head) : anchor(_anchor), head(_head) {}

    virtual Range ToSelectRange(const Buffer* buffer) const = 0;
    virtual Range ToDeleteRange(const Buffer* buffer) const {
        return ToSelectRange(buffer);
    }
    virtual bool LineSemantic() const { return false; }
    virtual ~Selection() {}  // Just quiet compiler warning
};

// normal selection
struct EditSelection : Selection {
    EditSelection() {}
    EditSelection(Pos _anchor, Pos _head) : Selection(_anchor, _head) {}

    Range ToSelectRange(const Buffer* buffer) const override {
        (void)buffer;
        return anchor < head ? Range{anchor, head} : Range{head, anchor};
    }
};

// vim visual mode selection
struct VimSelection : Selection {
    VimSelection(Pos _anchor, Pos _head) : Selection(_anchor, _head) {}

    Range ToSelectRange(const Buffer* buffer) const override;
};

// vim visual-line mode selection
struct VimLineSelection : Selection {
    VimLineSelection(Pos _anchor, Pos _head) : Selection(_anchor, _head) {}

    Range ToSelectRange(const Buffer* buffer) const override;
    Range ToDeleteRange(const Buffer* buffer) const override;
    bool LineSemantic() const override { return true; }
};

}  // namespace mango
