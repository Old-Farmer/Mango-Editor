#pragma once

#include "buffer.h"

namespace mango {

struct Selection {
    bool active = false;
    Pos anchor;
    Pos head;

    Range ToRange() const {
        if (!active) {
            return {{0, 0}, {0, 0}};
        }
        return anchor < head ? Range{anchor, head} : Range{head, anchor};
    }
};

}  // namespace mango
