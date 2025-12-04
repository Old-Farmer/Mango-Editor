#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace mango {

enum Result {
    kOk = 0,
    kError,
    kFail, // A failure is not an error
    kNotExist, // sth. not exist
    kInvalidCoding,
    kEof,
    kTermOutOfBounds,
    kBufferNoBackupFile,
    kBufferCannotLoad,
    kBufferReadOnly,
    kKeyseqError,
    kKeyseqDone,
    kKeyseqMatched,
    kCommandInvalidArgs,
    kCommandEmpty,
    kNoHistoryAvailable,
    kOuterCommandExecuteFail,  // outer command fail before or when execvp.
    kRetriggerCmp,
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
