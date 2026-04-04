#pragma once

#include <cstddef>

namespace mango {

enum class InputStateVim {
    kNone,
    kCount,  // count
};
enum class OperatorVim {
    kYank,
    kDelete,
};

// Editor core state of vim mode
struct EditorStateVim {
    size_t op_pending_stored_count = 0;

    InputStateVim input_state = InputStateVim::kNone;
    OperatorVim pending_operator;

    bool search_foward = true;

    EditorStateVim() {}
};

}  // namespace mango
