#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

#include "fmt/format.h"

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
    Exception(const char* format, Args&&... args) {
        msg_ = fmt::format(format, std::forward<Args>(args)...);
    }

    virtual const char* what() const noexcept override { return msg_.c_str(); }

   protected:
    std::string msg_;
};

#define MGO_NORMAL_EXCEPTION(exception)                         \
    class exception : public Exception {                        \
       public:                                                  \
        template <typename... Args>                             \
        exception(const char* format, Args&&... args)           \
            : Exception(format, std::forward<Args>(args)...) {} \
    };

MGO_NORMAL_EXCEPTION(TermException)
MGO_NORMAL_EXCEPTION(IOException)
MGO_NORMAL_EXCEPTION(FileCreateException)
MGO_NORMAL_EXCEPTION(CodingException)
MGO_NORMAL_EXCEPTION(FSException)
MGO_NORMAL_EXCEPTION(LogInitException)
MGO_NORMAL_EXCEPTION(SignalRegisterException)
MGO_NORMAL_EXCEPTION(KeyNotPredefinedException)
MGO_NORMAL_EXCEPTION(TSQueryPredicateDirectiveNotSupportException)
MGO_NORMAL_EXCEPTION(RegexCompileException)

class OSException : public Exception {
   public:
    template <typename... Args>
    OSException(int error_code, const char* format, Args&&... args)
        : Exception(format, std::forward<Args>(args)...),
          error_code_(error_code) {}
    int error_code() { return error_code_; }

   private:
    int error_code_;
};

MGO_NORMAL_EXCEPTION(TypeMismatchException)
MGO_NORMAL_EXCEPTION(OptionLoadException)
MGO_NORMAL_EXCEPTION(ParseMsgException)
}  // namespace mango