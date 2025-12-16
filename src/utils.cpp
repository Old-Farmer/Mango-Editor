#include "utils.h"

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>

#ifndef NDEBUG
#include "term.h"
#endif // !NDEBUG

namespace mango {

#ifndef NDEBUG
void AssertFail(const char* __assertion, const char* __file,
                unsigned int __line, const char* __function) {
    Terminal::GetInstance().Shutdown();
    __assert_fail(__assertion, __file, __line, __function);
}
#endif // !NDEBUG

}  // namespace mango