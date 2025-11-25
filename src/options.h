#pragma once

#include <unordered_map>
#include <vector>

#include "cursor.h"
#include "json.h"
#include "term.h"

namespace mango {

class Buffer;
class Window;

enum class Type {
    kBool,     // bool
    kInteger,  // int64_t
    kString,   // std::string
    kPtr,      // void*
};

enum ColorSchemeType : int {
    kNormal = 0,
    kSelection,
    kMenu,
    kLineNumber,
    kStatusLine,

    kKeyword,
    kTypeBuiltin,
    kOperator,
    kString,
    kComment,
    kNumber,
    kConstant,
    kFunction,
    kType,
    kVariable,
    kDelimiter,
    kProperty,
    kLabel,

    __kColorSchemeTypeCount,
};

enum class LineNumberType {
    kNone = 0,
    kAboslute,
    // kRelative, // Not support now
};

// NOTE: The unit of time related opts is ms.
enum OptKey {
    kOptPollEventTimeout,
    kOptScrollRows,
    kOptCursorStartHoldingInterval,
    kOptTabStop,
    kOptTabSpace,
    kOptAutoIndent,
    kOptAutoPair,
    kOptLineNumber,
    kOptStatusLineSepWidth,
    kOptCmpMenuMaxHeight,
    kOptCmpMenuMaxWidth,
    kOptEditHistoryMaxItem,
    kOptBasicWordCompletion,
    kOptTrueColor,
    kOptLogVerbose,
    kOptVi,
    kOptCursorBlinking,
    kOptCursorBlinkingShowInterval,
    kOptCursorBlinkingHideInterval,

    kOptColorScheme,

    __kOptKeyCount,
};

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

using ColorScheme = Terminal::AttrPair*;
using ColorSchemeElement = Terminal::AttrPair;

class Opts;

#define MGO_IF_TYPE_MISMATCH_THROW(expr) \
    if (!(expr)) throw TypeMismatchException("%s", #expr)

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

    template <typename T>
    T GetOpt(OptKey key) const {
        if constexpr (std::is_same_v<T, bool>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kBool);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kInteger);
        } else if constexpr (std::is_pointer_v<T>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kPtr);
        } else {
            static_assert(false,
                          "GetOpt<T> only supports T = bool, int64_t, or "
                          "pointer types");
        }

        if constexpr (std::is_same_v<T, bool>) {
            return static_cast<bool>(opts_[key]);
        } else {
            return reinterpret_cast<T>(opts_[key]);
        }
    }

    template <typename T>
    void SetOpt(OptKey key, T value) {
        if constexpr (std::is_same_v<T, bool>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kBool);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kInteger);
        } else if constexpr (std::is_pointer_v<T>) {
            MGO_IF_TYPE_MISMATCH_THROW(opt_info_[key].type == Type::kPtr);
        } else {
            static_assert(false,
                          "GetOpt<T> only supports T = bool, int64_t, or "
                          "pointer types");
        }

        opts_[key] = reinterpret_cast<void*>(value);
    }

   private:
    // throw OptionLoadException
    void TryApply(const Json& config, const Json& colorscheme_config);

   private:
    void* opts_[__kOptKeyCount];
    std::unordered_map<zstring_view, std::unordered_map<OptKey, void*>>
        filetype_opts_;

    bool user_config_valid_ = false;
    std::string user_config_error_reason_;

   public:
    const OptInfo* opt_info_;
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
        return global_opts_->opt_info_[key].scope;
    }

    template <typename T>
    T GetOpt(OptKey key) const {
        if constexpr (std::is_same_v<T, bool>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kBool);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kInteger);
        } else if constexpr (std::is_pointer_v<T>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kPtr);
        } else {
            static_assert(false,
                          "GetOpt<T> only supports T = bool, int64_t, or "
                          "pointer types");
        }

        MGO_ASSERT(OptScope::kGlobal != GetScope(key));

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
        if constexpr (std::is_same_v<T, bool>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kBool);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kInteger);
        } else if constexpr (std::is_pointer_v<T>) {
            MGO_IF_TYPE_MISMATCH_THROW(global_opts_->opt_info_[key].type ==
                                       Type::kPtr);
        } else {
            static_assert(false,
                          "SetOpt<T> only supports T = bool, int64_t, or "
                          "pointer types");
        }

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

}  // namespace mango
