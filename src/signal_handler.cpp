#include "signal_handler.h"

#include <cerrno>
#include <cstring>

#include "exception.h"
#include "signal.h"
#include "term.h"

namespace mango {

static sigjmp_buf sigbuf;

static constexpr int kBadSignals[] = {SIGABRT, SIGSEGV, SIGBUS};

// throw SignalException
static sighandler_t Signal(int signum, sighandler_t handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);  // block sigs of type being handled
    action.sa_flags = SA_RESTART;  // restart syscalls if possible

    if (sigaction(signum, &action, &old_action) < 0) {
        throw SignalRegisterException("%s", strerror(errno));
    }
    return (old_action.sa_handler);
}

static void BadSignalHandler(int signum) { siglongjmp(sigbuf, signum); }

SignalHandler::SignalHandler() {
    return;  // disable it now

    int ret = sigsetjmp(sigbuf, 0);
    if (ret != 0) {
        Terminal::GetInstance().Shutdown();
        exit(EXIT_FAILURE);
    }

    for (int signum : kBadSignals) {
        Signal(signum, BadSignalHandler);
    }
}

}  // namespace mango
