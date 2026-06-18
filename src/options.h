#pragma once

#include <unordered_map>
#include <vector>

#include "cursor.h"
#include "filetype.h"
#include "json.h"
#include "term.h"

namespace charxed {

class Buffer;
class TextWindow;

enum class Type {
    kBool,     // bool
    kInteger,  // int64_t
    kString,   // std::string
    kPtr,      // void*
};

#define CHX_THEME_TABLE               \
    X(kNormal, normal)                \
    X(kSelection, selection)          \
    X(kMenu, menu)                    \
    X(kMenuSelection, menu_selection) \
    X(kSidebar, sidebar)              \
    X(kStatusLine, statusline)        \
    X(kSearch, search)                \
    X(kSearchCurrent, search_current) \
    X(kTrailingWhite, trailing_white) \
    X(kCursorLine, cursor_line)       \
    X(kListCurrent, list_current)     \
                                      \
    X(kKeyword, keyword)              \
    X(kTypeBuiltin, typebuiltin)      \
    X(kOperator, operator)            \
    X(kString, string)                \
    X(kComment, comment)              \
    X(kNumber, number)                \
    X(kConstant, constant)            \
    X(kFunction, function)            \
    X(kType, type)                    \
    X(kVariable, variable)            \
    X(kDelimiter, delimiter)          \
    X(kProperty, property)            \
    X(kLabel, label)

// clang-format off
enum ThemeType : int {
#define X(t, str) t,
    CHX_THEME_TABLE
#undef X
    kNormalFg,  // Will be extract from kNormal
    _kThemeTypeCount,
};
// clang-format on

enum class LineNumberType {
    kNone = 0,
    kAboslute,
    kRelative,
};

#define CHX_BUFFER_OPT_TABLE                          \
    X(kOptAutoIndent, auto_indent, kBool)             \
    X(kOptAutoPair, auto_pair, kBool)                 \
    X(kOptMaxEditHistory, max_edit_history, kInteger) \
    X(kOptTabSpace, tab_space, kBool)                 \
    X(kOptTabStop, tab_stop, kInteger)                \
    X(kOptWrap, wrap, kBool)

#define CHX_WINDOW_OPT_TABLE                                 \
    X(kOptEndOfBufferMark, end_of_buffer_mark, kBool)        \
    X(kOptHighlightCursorLine, highlight_cursor_line, kBool) \
    X(kOptLineNumber, line_number, kInteger)                 \
    X(kOptScrollOff, scroll_off, kInteger)                   \
    X(kOptTrailingWhite, trailing_white, kBool)

#define CHX_GLOBAL_OPT_TABLE                                 \
    X(kOptBasicWordCompletion, basic_word_completion, kBool) \
    X(kOptCmpMenuMaxHeight, cmp_menu_max_height, kInteger)   \
    X(kOptCmpMenuMaxWidth, cmp_menu_max_width, kInteger)     \
    X(kOptTheme, _theme, kPtr)                               \
    X(kOptExplorerIndent, explorer_indent, kInteger)         \
    X(kOptHighlightOnSearch, highlight_on_search, kBool)     \
    X(kOptInputIdleTimeout, input_idle_timeout, kInteger)    \
    X(kOptLogVerbose, logverbose, kBool)                     \
    X(kOptMaxJumpHistory, max_jump_history, kInteger)        \
    X(kOptMaxPeelHistory, max_peel_history, kInteger)        \
    X(kOptScrollRows, scroll_rows, kInteger)                 \
    X(kOptSearchIgnoreCase, search_ignore_case, kBool)       \
    X(kOptTrueColor, truecolor, kBool)                       \
    /* private */                                            \
    X(kOptCursorStartHoldingInterval, cursor_start_holding_interval, kInteger)

// NOTE: The unit of time related opts is ms.
// clang-format off
enum OptKey {
#define X(t, ...) t,
    CHX_BUFFER_OPT_TABLE
    CHX_WINDOW_OPT_TABLE
    CHX_GLOBAL_OPT_TABLE
#undef X
    _kOptKeyCount,
};
// clang-format on

// Options have scope.
// global scope options only store in a global table.
// local scopt options stores not only in global table, but also in a local
// table when needed, overiding global value of options.
enum class OptScope {
    kGlobal,
    kWindow,
    kBuffer,
};

// some info of an options.
struct OptInfo {
    OptScope scope;
    Type type;
};

using Theme = Terminal::AttrPair*;
using ThemeElement = Terminal::AttrPair;

class Opts;

#define CHX_IF_TYPE_MISMATCH_THROW(expr) \
    if (!(expr)) throw TypeMismatchException("{}", #expr)

// GlobalOpts is a class that represents all opts.
class GlobalOpts {
    friend Opts;

   public:
    // Throw same as LoadConfig.
    // We loadconfig when init.
    GlobalOpts();
    ~GlobalOpts();

    // throw Json::exception, IOException, OptionLoadException
    // Do not handle them, which always means a bug and should fix
    // default config.
    // TODO: support reload.
    void LoadConfig();

    bool IsUserConfigValid() { return user_config_valid_; }
    std::string GetUserConfigErrorReportStrAndReleaseIt() {
        return std::move(user_config_error_reason_);
    }

    // We can use key as template parameter to eliminate runtime type checking.
    template <typename T>
    constexpr T GetOpt(OptKey key) const {
        CheckKeyType<T>(key);
        if constexpr (std::is_same_v<T, bool>) {
            return static_cast<bool>(opts_[key]);
        } else {
            return reinterpret_cast<T>(opts_[key]);
        }
    }

    template <typename T>
    void SetOpt(OptKey key, T value) {
        CheckKeyType<T>(key);
        opts_[key] = reinterpret_cast<void*>(value);
    }

    static OptInfo GetOptInfo(OptKey key);
    template <typename T>
    static void CheckKeyType(OptKey key) {
        if constexpr (std::is_same_v<T, bool>) {
            CHX_IF_TYPE_MISMATCH_THROW(GetOptInfo(key).type == Type::kBool);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            CHX_IF_TYPE_MISMATCH_THROW(GetOptInfo(key).type == Type::kInteger);
        } else if constexpr (std::is_pointer_v<T>) {
            CHX_IF_TYPE_MISMATCH_THROW(GetOptInfo(key).type == Type::kPtr);
        } else {
            static_assert(kAlwaysFalseV<>,
                          "Get/SetOpt<T> only supports T = bool, int64_t, or "
                          "pointer types");
        }
    }

   private:
    // throw OptionLoadException
    void TryApply(const Json& config, const Json& theme_config);

   private:
    void* opts_[_kOptKeyCount];
    std::unordered_map<OptKey, void*>
        filetype_opts_[static_cast<int>(FileType::_kCount)];

    bool user_config_valid_ = false;
    std::string user_config_error_reason_;

    const std::string kUserConfigPath;
    const std::string kUserThemePath;
};

// Opts is a option table that store some local opts.
// Local opts will override global opts when it exist.
// Now only window, mango_peel, buffer and frame use this.
// Not use it if the current context don't need local opts.
class Opts {
   public:
    Opts(GlobalOpts* global_options);

    void InitAfterBufferLoad(const Buffer* buffer);

    OptScope GetScope(OptKey key) const {
        return GlobalOpts::GetOptInfo(key).scope;
    }

    template <typename T>
    T GetOpt(OptKey key) const {
        GlobalOpts::CheckKeyType<T>(key);
        CHX_ASSERT(OptScope::kGlobal != GetScope(key));

        auto iter = opts_.find(key);
        if (iter == opts_.end()) {
            return global_opts_->GetOpt<T>(key);
        }

        if constexpr (std::is_same_v<T, bool>) {
            return static_cast<bool>(iter->second);
        } else {
            return reinterpret_cast<T>(iter->second);
        }
    }

    template <typename T>
    void SetOpt(OptKey key, T value, bool global = false) {
        GlobalOpts::CheckKeyType<T>(key);

        if (GetScope(key) == OptScope::kGlobal || global) {
            global_opts_->SetOpt(key, value);
            return;
        }

        opts_[key] = reinterpret_cast<void*>(value);
    }

   private:
    std::unordered_map<OptKey, void*> opts_;

   public:
    GlobalOpts* global_opts_;
};

// Some options only useful when the program just starts
struct InitOpts {
    std::vector<const char*> begin_files;
};

}  // namespace charxed
