#pragma once

#include <cstdio>
#include <exception>
#include <memory>

namespace mango {

class Exception : public std::exception {
   public:
    Exception() = default;
    template <typename... Args>
    Exception(const char* format, Args... args) {
        int size = snprintf(nullptr, 0, format, args...);
        if (size > 0) {
            msg_ = std::make_unique<char[]>(size + 1);
            snprintf(msg_.get(), size + 1, format, args...);
        }
    }

    virtual const char* what() const noexcept override { return msg_.get(); }

   protected:
    std::unique_ptr<char[]> msg_;
};

class TermException : public Exception {
   public:
    template <typename... Args>
    TermException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class IOException : public Exception {
   public:
    template <typename... Args>
    IOException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class FileCreateException : public IOException {
   public:
    template <typename... Args>
    FileCreateException(const char* format, Args... args)
        : IOException(format, args...) {}
};

class FSException : public Exception {
   public:
    template <typename... Args>
    FSException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class LoggingException : public Exception {
   public:
    template <typename... Args>
    LoggingException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class SignalRegisterException : public Exception {
   public:
    template <typename... Args>
    SignalRegisterException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class KeyNotPredefinedException : public Exception {
   public:
    template <typename... Args>
    KeyNotPredefinedException(const char* format, Args... args)
        : Exception(format, args...) {}
};

}  // namespace mango