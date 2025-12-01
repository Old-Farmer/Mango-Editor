#include <cstdio>

#include "cmd_parse.h"
#include "editor.h"
#include "logging.h"
#include "signal_handler.h"
#include "term.h"

namespace mango {
void TerminateHandler() {
    Terminal::GetInstance().Shutdown();
    std::exception_ptr exptr = std::current_exception();
    if (exptr) {
        try {
            std::rethrow_exception(exptr);
        } catch (const std::exception& e) {
            PrintException(&e);
        } catch (...) {
            PrintException(nullptr);
        }
    }

    std::abort();
}
}  // namespace mango

using namespace mango;

int main(int argc, char* argv[]) {
    // Always use this locale.
    setlocale(LC_CTYPE, "en_US.UTF-8");

    SignalHandler signal_handler;

    std::set_terminate(TerminateHandler);

    Path::GetCwdSys();
    Path::GetAppRootSys();

    LogInit(kloggingFilePath);

    MGO_LOG_DEBUG("cwd %s", Path::GetCwd().c_str());
    MGO_LOG_DEBUG("app root %s", Path::GetAppRoot().c_str());

    auto global_opts = std::make_unique<GlobalOpts>();
    auto init_opts = std::make_unique<InitOpts>();
    ParseCmdArgs(argc, argv, global_opts.get(), init_opts.get());
    Editor& editor = Editor::GetInstance();
    editor.Init(std::move(global_opts), std::move(init_opts));
    editor.Loop();
}
