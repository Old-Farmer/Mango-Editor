#include "utils.h"

#include <cstdio>
#include <cstdlib>

#include "term.h"

namespace mango {

void AssertFail(const char* __assertion, const char* __file,
                unsigned int __line, const char* __function) {
    mango::Terminal::GetInstance().Shutdown();
    __assert_fail(__assertion, __file, __line, __function);
}

}  // namespace mango