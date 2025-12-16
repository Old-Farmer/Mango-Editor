#include "logging.h"

#include <cstdio>
#include <cstring>
#include "fs.h"

#include "exception.h"

namespace mango {
FILE* logging_file;

std::string GetLoggingFilePath() {
    return Path::GetCache() + "mango.log";
}

void LogInit(const std::string& file) {
    logging_file = fopen(file.c_str(), "a+");
    if (logging_file == nullptr) {
        throw LogInitException("Log file can't open: {}", strerror(errno));
    }
}

void LogDeinit() {
    fclose(logging_file);
}
}  // namespace mango