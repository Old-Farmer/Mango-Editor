#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace mango {

enum Result {
    kOk = 0,
    kError,
    kTermOutOfBounds,
    kBufferNoBackupFile,
    kBufferCannotLoad,
    kBufferReadOnly,
    kEof,
    kKeymapError,
    kKeymapDone,
    kKeymapMatched,
    kCommandNotExist,
    kCommandInvalidArgs,
    kCmpNoSuggest,
    kNoHistoryAvailable,
};

constexpr const char* kResultString[] = {"Ok", "Error"};

inline const char* ResultString(Result result) { return kResultString[result]; }

inline void PrintError(std::string_view context, std::string_view result) {
    fprintf(stderr, "%s: %s\n", context.data(), result.data());
}

inline void PrintResult(std::string_view context, Result result) {
    fprintf(stderr, "%s: %s\n", context.data(), ResultString(result));
}

inline void PrintResultExit(std::string_view context, Result result) {
    PrintResult(context, result);
    exit(EXIT_FAILURE);
}

}  // namespace mango
