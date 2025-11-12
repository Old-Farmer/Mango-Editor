#pragma once

#include <cassert>
#include <string_view>

#include "result.h"

namespace mango {

#define MGO_DELETE_COPY(ClassName)        \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

#define MGO_DELETE_MOVE(ClassName)   \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define MGO_DEFAULT_COPY(ClassName)        \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default

#define MGO_DEFAULT_MOVE(ClassName)            \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default

#define MGO_DEFAULT_CONSTRUCT_DESTRUCT(ClassName) \
    ClassName() = default;                        \
    ~ClassName() = default

// a string_view must terminated by a '\0'
using zstring_view = std::string_view;

inline const char* zstring_view_c_str(const zstring_view& str) {
    return str.data();
}

template <typename T>
struct List {
    T head;
    T tail;
};

void AssertFail(const char* __assertion, const char* __file,
                unsigned int __line, const char* __function);

#ifdef NDEBUG
#define MGO_ASSERT(expr) (static_cast<void>(0))
#else
#define MGO_ASSERT(expr)     \
    (static_cast<bool>(expr) \
         ? void(0)           \
         : AssertFail(#expr, __ASSERT_FILE, __ASSERT_LINE, __ASSERT_FUNCTION))
#endif  // !NDEBUG

// Thorw OSException when som syscalls fail.
// Return kOk if ok, kOuterCommandExecuteFail if outer command fail before or
// when execvp.
// When check == true, LOG level will be info instead of error.
// For parmeters file, argv, read execvp for more info. argv[] should be null
// terminated.
Result Exec(const char* file, char* const argv[], const std::string* stdin_data,
            std::string* stdout_data, std::string* stderr_data, int& exit_code,
            bool check = false);

}  // namespace mango
