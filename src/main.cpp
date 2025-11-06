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
    SignalHandler signal_handler;

    std::set_terminate(TerminateHandler);

    Path::GetCwdSys();
    Path::GetAppRootSys();

    LogInit(std::string(Path::GetAppRoot()) +
            zstring_view_c_str(kloggingFilePath));

    MANGO_LOG_DEBUG("cwd %s", Path::GetCwd().c_str());
    MANGO_LOG_DEBUG("app root %s", Path::GetAppRoot().c_str());

    auto options = std::make_unique<Options>();
    auto init_options = std::make_unique<InitOptions>();
    ParseCmdArgs(argc, argv, options.get(), init_options.get());
    Editor& editor = Editor::GetInstance();
    editor.Loop(std::move(options), std::move(init_options));
}
