#include "exception.h"

// TODO: support windows
#include <cxxabi.h>

#include <cstdarg>

namespace mango {

void PrintException(const std::exception* e) {
    if (e) {
        int status = 0;
        const char* mangled = typeid(*e).name();
        const char* demangled =
            abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

        fmt::println(stderr,
                     "Caught an instance of '{}'\n"
                     "  what(): {}",
                     (status == 0 && demangled) ? demangled : mangled,
                     e->what());

        free(const_cast<char*>(demangled));
    } else {
        int status = 0;
        const char* mangled =
            __cxxabiv1::__cxa_current_exception_type()->name();
        const char* demangled =
            abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

        fmt::println(stderr, "Caught an instance of '{}'",
                     (status == 0 && demangled) ? demangled : mangled);

        free(const_cast<char*>(demangled));
    }
}

}  // namespace mango