#pragma once

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

namespace mango {

extern FILE* logging_file;

void LogInit();

inline std::string GetTime(const char* format = "%Y-%m-%d %H:%M:%S") {
    std::stringstream ss;
    auto t =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ss << std::put_time(std::localtime(&t), format);
    return ss.str();
}

#define MANGO_DEBUG_PREFIX "[DEBUG]"
#define MANGO_INFO_PREFIX "[INFO]"
#define MANGO_WARN_PREFIX "[WARN]"
#define MANGO_ERROR_PREFIX "[ERROR]"
#define MANGO_UNKNOWN_PREFIX "[UNKNOWN]"

enum class LogLevel { kDebug, kInfo, kWarn, kError };

#define MANGO_LOG_PREFIX(level, prefix_str)                                 \
    do {                                                                    \
        std::stringstream ss;                                               \
        if constexpr ((level) == LogLevel::kDebug) {                        \
            ss << MANGO_DEBUG_PREFIX;                                       \
        } else if constexpr ((level) == LogLevel::kInfo) {                  \
            ss << MANGO_INFO_PREFIX;                                        \
        } else if constexpr ((level) == LogLevel::kWarn) {                  \
            ss << MANGO_WARN_PREFIX;                                        \
        } else if constexpr ((level) == LogLevel::kError) {                 \
            ss << MANGO_ERROR_PREFIX;                                       \
        } else {                                                            \
            ss << MANGO_UNKNOWN_PREFIX;                                     \
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
