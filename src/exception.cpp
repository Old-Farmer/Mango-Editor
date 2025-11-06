#include "exception.h"

// TODO: support windows
#include <cxxabi.h>

#include <cstdarg>

namespace mango {

void PrintException(const std::exception* e) {
    if (e) {
        int status = 0;
        const char* mangled = typeid(*e).name();
        char* demangled =
            abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

        fprintf(stderr,
                "Caught an instance of '%s'\n"
                "  what(): %s\n",
                (status == 0 && demangled) ? demangled : mangled, e->what());

        free(demangled);
    } else {
        int status = 0;
        const char* mangled =
            __cxxabiv1::__cxa_current_exception_type()->name();
        char* demangled =
            abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

        fprintf(stderr, "Caught an instance of '%s'\n",
                (status == 0 && demangled) ? demangled : mangled);

        free(demangled);
    }
}

}  // namespace mango