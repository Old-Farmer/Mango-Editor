#include "utils.h"

#include <cstdio>
#include <cstdlib>

#include "term.h"

namespace mango {}

// override this function for assert
void __assert_fail(const char* __assertion, const char* __file,
                   unsigned int __line, const char* __function) {
    mango::Terminal::GetInstance().Shutdown();
    // TODO: add executable file name at beginning
    fprintf(stderr, "%s:%u: %s: Assertion `%s` failed.\n", __file, __line,
            __function, __assertion);

    abort();
}