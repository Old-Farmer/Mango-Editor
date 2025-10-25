#pragma once

#include <string_view>
namespace mango {

#define MANGO_DELETE_COPY(ClassName)      \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

#define MANGO_DELETE_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define MANGO_DEFAULT_COPY(ClassName)      \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default

#define MANGO_DEFAULT_MOVE(ClassName)          \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default

#define MANGO_DEFAULT_CONSTRUCT_DESTRUCT(ClassName) \
    ClassName() = default;                          \
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

}  // namespace mango
