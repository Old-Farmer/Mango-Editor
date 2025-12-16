#pragma once

#include <chrono>
#include <cstdio>
#include <string>

#include "fmt/chrono.h"  // IWYU pragma: keep

namespace mango {

extern FILE* logging_file;

std::string GetLoggingFilePath();

// throws LoggingException
void LogInit(const std::string& file = GetLoggingFilePath());

// Deinit the resource, normally shouldn't be called
void LogDeinit();

constexpr std::string_view kDebugPrefix = "[DEBUG]";
constexpr std::string_view kInfoPrefix = "[INFO]";
constexpr std::string_view kWarnPrefix = "[WARN]";
constexpr std::string_view kErrorPrefix = "[ERROR]";

enum class LogLevel { kDebug, kInfo, kWarn, kError };

#define MGO_LOG_FILE __builtin_FILE()
#define MGO_LOG_LINE __LINE__
#define MGO_LOG_FUNC __func__

template <LogLevel level, typename... Args>
void Log(const char* file, unsigned int line, const char* func,
         const char* format, Args&&... args) {
    std::string_view level_prefix;
    if constexpr (level == LogLevel::kDebug) {
        level_prefix = kDebugPrefix;
    } else if constexpr (level == LogLevel::kInfo) {
        level_prefix = kInfoPrefix;
    } else if constexpr (level == LogLevel::kWarn) {
        level_prefix = kWarnPrefix;
    } else {  // level == LogLevel::kError
        level_prefix = kErrorPrefix;
    }
    auto t =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    fmt::println(logging_file, "{}({}:{}:{}) {:%Y-%m-%d %H:%M:%S}: {}",
                 level_prefix, file, line, func, *std::localtime(&t),
                 fmt::format(format, std::forward<Args>(args)...));
    if constexpr (level != LogLevel::kInfo) {
        fflush(logging_file);
    }
}

#ifdef NDEBUG

#define MGO_LOG_DEBUG(format, ...) \
    do {                           \
    } while (0)

#else

#define MGO_LOG_DEBUG(format, ...)                                          \
    Log<LogLevel::kDebug>(MGO_LOG_FILE, MGO_LOG_LINE, MGO_LOG_FUNC, format, \
                          ##__VA_ARGS__)

#endif  // NDEBUG

#define MGO_LOG_INFO(format, ...)                                          \
    Log<LogLevel::kInfo>(MGO_LOG_FILE, MGO_LOG_LINE, MGO_LOG_FUNC, format, \
                         ##__VA_ARGS__)

#define MGO_LOG MGO_LOG_INFO

#define MGO_LOG_WARN(format, ...)                                          \
    Log<LogLevel::kWarn>(MGO_LOG_FILE, MGO_LOG_LINE, MGO_LOG_FUNC, format, \
                         ##__VA_ARGS__)

#define MGO_LOG_ERROR(format, ...)                                          \
    Log<LogLevel::kError>(MGO_LOG_FILE, MGO_LOG_LINE, MGO_LOG_FUNC, format, \
                          ##__VA_ARGS__)

}  // namespace mango
