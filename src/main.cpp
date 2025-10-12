#include <cstdio>

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
        LogInit();
        static Options options;
        ParseCmdArgs(argc, argv, options);
        Editor& editor = Editor::GetInstance();
        editor.Loop(&options);
    } catch (LoggingException& e) {
        Terminal::GetInstance().Shutdown();
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
