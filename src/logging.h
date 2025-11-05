#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include "utils.h"

namespace mango {

extern FILE* logging_file;

// TODO: build?
constexpr zstring_view kloggingFilePath = "build/mango.log";

void LogInit(const std::string& file);

inline std::string GetTime(const char* format = "%Y-%m-%d %H:%M:%S") {
    std::stringstream ss;
    auto t =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ss << std::put_time(std::localtime(&t), format);
    return ss.str();
}

constexpr std::string_view kDebugPrefix = "[DEBUG]";
constexpr std::string_view kInfoPrefix = "[INFO]";
constexpr std::string_view kWarnPrefix = "[WARN]";
constexpr std::string_view kErrorPrefix = "[ERROR]";
constexpr std::string_view kUnknownPrefix = "[UNKNOWN]";

enum class LogLevel { kDebug, kInfo, kWarn, kError };

#define MANGO_LOG_PREFIX(level, prefix_str)                                 \
    do {                                                                    \
        std::stringstream ss;                                               \
        if constexpr ((level) == LogLevel::kDebug) {                        \
            ss << kDebugPrefix;                                             \
        } else if constexpr ((level) == LogLevel::kInfo) {                  \
            ss << kInfoPrefix;                                              \
        } else if constexpr ((level) == LogLevel::kWarn) {                  \
            ss << kWarnPrefix;                                              \
        } else if constexpr ((level) == LogLevel::kError) {                 \
            ss << kErrorPrefix;                                             \
        } else {                                                            \
            ss << kUnknownPrefix;                                           \
        }                                                                   \
        ss << "(" << __builtin_FILE() << ":" << __LINE__ << ":" << __func__ \
           << ")" << " " << GetTime() << ": ";                              \
        prefix_str = ss.str();                                              \
    } while (0)

#ifdef NDEBUG

#define MANGO_LOG_DEBUG(format, ...) \
    do {                             \
    } while (0)

#else

#define MANGO_LOG_DEBUG(format, ...)                            \
    do {                                                        \
        std::string prefix;                                     \
        MANGO_LOG_PREFIX(LogLevel::kDebug, prefix);             \
        fprintf(logging_file, "%s" format "\n", prefix.c_str(), \
                ##__VA_ARGS__);                                 \
        fflush(logging_file);                                   \
    } while (0)

#endif  // NDEBUG

#define MANGO_LOG_INFO(format, ...)                             \
    do {                                                        \
        std::string prefix;                                     \
        MANGO_LOG_PREFIX(LogLevel::kInfo, prefix);              \
        fprintf(logging_file, "%s" format "\n", prefix.c_str(), \
                ##__VA_ARGS__);                                 \
        fflush(logging_file);                                   \
    } while (0)

#define MANGO_LOG(format, ...) MANGO_LOG_INFO(format, ##__VA_ARGS__)

#define MANGO_LOG_WARN(format, ...)                             \
    do {                                                        \
        std::string prefix;                                     \
        MANGO_LOG_PREFIX(LogLevel::kWarn, prefix);              \
        fprintf(logging_file, "%s" format "\n", prefix.c_str(), \
                ##__VA_ARGS__);                                 \
        fflush(logging_file);                                   \
    } while (0)

#define MANGO_LOG_ERROR(format, ...)                            \
    do {                                                        \
        std::string prefix;                                     \
        MANGO_LOG_PREFIX(LogLevel::kError, prefix);             \
        fprintf(logging_file, "%s" format "\n", prefix.c_str(), \
                ##__VA_ARGS__);                                 \
        fflush(logging_file);                                   \
    } while (0)

}  // namespace mango
