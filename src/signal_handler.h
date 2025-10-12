#pragma once

#include <csetjmp>

namespace mango {

class SignalHandler {
   public:
    // Register a handler of some known signals for recovering the terminal
    // state back to normal when the program receives some signals and quits,
    // especially for bugs or SIGSTOP
    // Best effort.
    // TODO: SIGSTOP & SIGCONT handling
    // TODO: do it better
    SignalHandler();
};

}  // namespace mango
