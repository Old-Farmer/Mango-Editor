#include "logging.h"

#include <cstdio>
#include <cstring>

#include "exception.h"

namespace mango {
    FILE* logging_file;

    constexpr const char* kloggingFilePath = "mango.log";

    void LogInit() {
        logging_file = fopen(kloggingFilePath, "a+");
        if (logging_file == nullptr) {
            throw LoggingException("Log file can't open: %s", strerror(errno));
        }
    }
}