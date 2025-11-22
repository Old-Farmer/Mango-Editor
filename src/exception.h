#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>

namespace mango {

// Print an exception info.
// if e != nullptr, the type of e and e.what() will be shown;
// else, only show the current exception's type.
// NOTE: This function should only be called in terminate handler.
void PrintException(const std::exception* e);

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

class CodeingException : public Exception {
   public:
    template <typename... Args>
    CodeingException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class FSException : public Exception {
   public:
    template <typename... Args>
    FSException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class LogInitException : public Exception {
   public:
    template <typename... Args>
    LogInitException(const char* format, Args... args)
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

class TSQueryPredicateDirectiveNotSupportException : public Exception {
   public:
    template <typename... Args>
    TSQueryPredicateDirectiveNotSupportException(const char* format,
                                                 Args... args)
        : Exception(format, args...) {}
};

class RegexCompileException : public Exception {
   public:
    template <typename... Args>
    RegexCompileException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class OSException : public Exception {
   public:
    template <typename... Args>
    OSException(int error_code, const char* format, Args... args)
        : Exception(format, args...), error_code_(error_code) {}
    int error_code() { return error_code_; }

   private:
    int error_code_;
};

class TypeMismatchException : public Exception {
   public:
    template <typename... Args>
    TypeMismatchException(const char* format, Args... args)
        : Exception(format, args...) {}
};

class OptionLoadException : public Exception {
   public:
    template <typename... Args>
    OptionLoadException(const char* format, Args... args)
        : Exception(format, args...) {}
};

}  // namespace mango