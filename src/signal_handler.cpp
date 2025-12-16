#include "signal_handler.h"

#include <cerrno>
#include <cstring>

#include "exception.h"
#include "signal.h"
#include "term.h"

namespace mango {

static constexpr int kBadSignals[] = {SIGABRT, SIGSEGV, SIGBUS};

// throw SignalRegisterException
static sighandler_t Signal(int signum, sighandler_t handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);  // not block any sigs
    action.sa_flags = SA_RESTART;  // restart syscalls if possible

    if (sigaction(signum, &action, &old_action) < 0) {
        throw SignalRegisterException("{}", strerror(errno));
    }
    return (old_action.sa_handler);
}

static void BadSignalHandler(int signum) {
    Terminal::GetInstance().Shutdown();
    try {
        Signal(signum, SIG_DFL);
    } catch (...) {
    }
    raise(signum);
}

SignalHandler::SignalHandler() {
    for (int signum : kBadSignals) {
        Signal(signum, BadSignalHandler);
    }
}

}  // namespace mango
