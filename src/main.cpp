#include <cstdio>

#include "character.h"
#include "cmd_parse.h"
#include "editor.h"
#include "exception.h"
#include "logging.h"
#include "signal_handler.h"
#include "term.h"

using namespace mango;

int main(int argc, char* argv[]) {
    try {
        SignalHandler signal_handler;

        Path::GetCwdSys();
        Path::GetAppRootSys();

        LogInit(std::string(std::string(Path::GetAppRoot()) + kSlash +
                            zstring_view_c_str(kloggingFilePath)));

        MANGO_LOG_DEBUG("cwd %s", Path::GetCwd().c_str());
        MANGO_LOG_DEBUG("app root %s", Path::GetAppRoot().c_str());

        auto options = std::make_unique<Options>();
        auto init_options = std::make_unique<InitOptions>();
        ParseCmdArgs(argc, argv, options.get(), init_options.get());
        Editor& editor = Editor::GetInstance();
        editor.Loop(std::move(options), std::move(init_options));
    } catch (LoggingException& e) {
        fputs(e.what(), stderr);
    } catch (FSException& e) {
        fputs(e.what(), stderr);
    } catch (Exception& e) {
        MANGO_LOG_ERROR("%s", e.what());
        Terminal::GetInstance().Shutdown();
        fputs(e.what(), stderr);
    } catch (std::exception& e) {
        Terminal::GetInstance().Shutdown();
        fputs(e.what(), stderr);
    }
}
