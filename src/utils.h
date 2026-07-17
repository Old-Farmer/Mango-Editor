#pragma once

#include <cassert>
#include <optional>
#include <string_view>

namespace charxed {

#define CHX_DELETE_COPY(ClassName)        \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

#define CHX_DELETE_MOVE(ClassName)   \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define CHX_DEFAULT_COPY(ClassName)        \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default

#define CHX_DEFAULT_MOVE(ClassName)            \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default

#define CHX_DEFAULT_CONSTRUCT_DESTRUCT(ClassName) \
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

#ifdef NDEBUG
#define CHX_ASSERT(expr) (static_cast<void>(0))
#else
void AssertFail(const char* __assertion, const char* __file,
                unsigned int __line, const char* __function);
#define CHX_ASSERT(expr)     \
    (static_cast<bool>(expr) \
         ? void(0)           \
         : AssertFail(#expr, __ASSERT_FILE, __ASSERT_LINE, __ASSERT_FUNCTION))
#endif  // !NDEBUG

// Calc width of a number, e.g. 100 -> 3, 0 -> 1
inline size_t NumberWidth(size_t num) {
    size_t width = 0;
    do {
        num /= 10;
        width++;
    } while (num);
    return width;
}

// Useful for old compilers like gcc 9.3.0 when using static_assert
template <typename... T>
inline constexpr bool kAlwaysFalseV = false;

// An iterator adapter that wrap a smart pointer iterator.
// Its * and -> now return just raw pointer.
template <typename Iter>
class PointerIterator {
    Iter it_;

   public:
    PointerIterator(Iter i) : it_(i) {}

    auto operator*() const { return it_->get(); }
    auto operator->() const { return it_->get(); }

    PointerIterator& operator++() {
        ++it_;
        return *this;
    }
    PointerIterator operator++(int) {
        auto tmp = *this;
        ++it_;
        return tmp;
    }

    bool operator==(const PointerIterator& other) const {
        return it_ == other.it_;
    }
    bool operator!=(const PointerIterator& other) const {
        return it_ != other.it_;
    }
};

template <typename T>
inline const T* OptionalToPtr(const std::optional<T>& opt) {
    return opt.has_value() ? &*opt : nullptr;
}

template <typename T>
inline T* OptionalToPtr(std::optional<T>& opt) {
    return opt.has_value() ? &*opt : nullptr;
}

}  // namespace charxed
