#include "options.h"

#include "buffer.h"
#include "file.h"
#include "filetype.h"
#include "fs.h"
#include "json.h"

namespace mango {

static constexpr const char* kDefaultConfigPath = "resource/config.json";
static const std::string kUserConfigPath =
    std::string(Path::GetConfig()) + "mango.json";

static const std::unordered_map<std::string_view, OptKey> kStrRepToOptKey{
    {"poll_event_timeout", kOptPollEventTimeout},
    {"scroll_rows", kOptScrollRows},
    {"esc_timeout", kOptEscTimeout},
    {"cursor_start_holding_interval", kOptCursorStartHoldingInterval},
    {"tab_stop", kOptTabStop},
    {"tab_space", kOptTabSpace},
    {"auto_indent", kOptAutoIndent},
    {"auto_pair", kOptAutoPair},
    {"line_number", kOptLineNumber},
    {"status_line_sep_width", kOptStatusLineSepWidth},
    {"cmp_menu_max_height", kOptCmpMenuMaxHeight},
    {"cmp_menu_max_width", kOptCmpMenuMaxWidth},
    {"edit_history_max_item", kOptEditHistoryMaxItem},
    {"basic_word_completion", kOptBasicWordCompletion},
};

static void OptStaticInit(const OptInfo*& opt_info) {
    static const auto static_opt_info = [] {
        auto static_opt_info = new OptInfo[__kOptKeyCount];
        static_opt_info[kOptPollEventTimeout] = {OptScope::kGlobal,
                                                 Type::kInteger};
        static_opt_info[kOptScrollRows] = {OptScope::kGlobal, Type::kInteger};
        static_opt_info[kOptEscTimeout] = {OptScope::kGlobal, Type::kInteger};
        static_opt_info[kOptCursorStartHoldingInterval] = {OptScope::kGlobal,
                                                           Type::kInteger};
        static_opt_info[kOptStatusLineSepWidth] = {OptScope::kGlobal,
                                                   Type::kInteger};
        static_opt_info[kOptCmpMenuMaxWidth] = {OptScope::kGlobal,
                                                Type::kInteger};
        static_opt_info[kOptCmpMenuMaxHeight] = {OptScope::kGlobal,
                                                 Type::kInteger};
        static_opt_info[kOptEditHistoryMaxItem] = {OptScope::kGlobal,
                                                   Type::kInteger};
        static_opt_info[kOptBasicWordCompletion] = {OptScope::kGlobal,
                                                    Type::kBool};

        static_opt_info[kOptLineNumber] = {OptScope::kWindow, Type::kInteger};

        static_opt_info[kOptTabStop] = {OptScope::kBuffer, Type::kInteger};
        static_opt_info[kOptTabSpace] = {OptScope::kBuffer, Type::kBool};
        static_opt_info[kOptAutoIndent] = {OptScope::kBuffer, Type::kBool};
        static_opt_info[kOptAutoPair] = {OptScope::kBuffer, Type::kBool};

        static_opt_info[kOptColorScheme] = {OptScope::kGlobal, Type::kPtr};

        return const_cast<OptInfo*>(static_opt_info);
    }();
    opt_info = static_opt_info;
}

GlobalOpts::GlobalOpts() {
    OptStaticInit(opt_info_);

    std::string default_config_str =
        File(std::string(Path::GetAppRoot() + kDefaultConfigPath), "r", false)
            .ReadAll();

    bool user_config_now_valid = true;
    std::string user_config_str;
    if (File::FileReadable(kUserConfigPath)) {
        try {
            user_config_str = File(kUserConfigPath, "r", false).ReadAll();
        } catch (IOException& e) {
            MGO_LOG_ERROR("Can't read user config file: %s", e.what());
            user_config_now_valid = false;
        }
    }

    Json config = Json::parse(default_config_str);
    if (!user_config_str.empty()) {
        try {
            Json user_config = Json::parse(user_config_str);

            // Validate the user config
            for (const auto& [k, v] : user_config.items()) {
                if (IsFiletype(k)) {
                    for (const auto& [inner_k, inner_v] : v.items()) {
                        auto iter = kStrRepToOptKey.find(inner_k);
                        if (iter == kStrRepToOptKey.end()) {
                            continue;
                        }
                        OptKey opt_key = iter->second;
                        const OptInfo& opt_info = opt_info_[opt_key];
                        if (opt_info.scope != OptScope::kBuffer) {
                            continue;
                        }
                        if (opt_info.type == Type::kBool &&
                            inner_v.is_boolean()) {
                            ;
                        } else if (opt_info.type == Type::kInteger &&
                                   inner_v.is_number_integer()) {
                            ;
                        } else {
                            user_config_now_valid = false;
                            break;
                        }
                    }
                    if (!user_config_now_valid) {
                        break;
                    }
                    continue;
                }

                auto iter = kStrRepToOptKey.find(k);
                if (iter == kStrRepToOptKey.end()) {
                    continue;
                }
                OptKey opt_key = iter->second;
                const OptInfo& opt_info = opt_info_[opt_key];
                if (opt_info.type == Type::kBool && v.is_boolean()) {
                    ;
                } else if (opt_info.type == Type::kInteger &&
                           v.is_number_integer()) {
                    ;
                } else {
                    user_config_now_valid = false;
                    break;
                }
            }

            if (user_config_now_valid) {
                config.update(Json::parse(user_config_str), true);
            }
        } catch (Json::exception& e) {
            MGO_LOG_ERROR(
                "Json error when parse user config and merge with default "
                "config: %s",
                e.what());
            user_config_now_valid = false;
        }
    }

    for (const auto& [k, v] : config.items()) {
        if (IsFiletype(k)) {
            filetype_opts_.insert({k, {}});
            for (const auto& [inner_k, inner_v] : v.items()) {
                auto iter = kStrRepToOptKey.find(inner_k);
                if (iter == kStrRepToOptKey.end()) {
                    continue;
                }
                OptKey opt_key = iter->second;
                const OptInfo& opt_info = opt_info_[opt_key];
                if (opt_info.scope != OptScope::kBuffer) {
                    continue;
                }

                if (opt_info.type == Type::kBool && inner_v.is_boolean()) {
                    filetype_opts_[k][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<bool>());
                } else if (opt_info.type == Type::kInteger &&
                           inner_v.is_number_integer()) {
                    filetype_opts_[k][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<int64_t>());
                } else {
                    throw TypeMismatchException(
                        "value type wrong: key: %s, v type: %d %d",
                        ("/" + k + "/" + inner_k).c_str(), opt_info.type,
                        inner_v.type());  // flatten rep of key
                }
            }
            continue;
        }

        auto iter = kStrRepToOptKey.find(k);
        if (iter == kStrRepToOptKey.end()) {
            continue;
        }
        OptKey opt_key = iter->second;
        const OptInfo& opt_info = opt_info_[opt_key];
        if (opt_info.type == Type::kBool && v.is_boolean()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<bool>());
        } else if (opt_info.type == Type::kInteger && v.is_number_integer()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<int64_t>());
        } else {
            throw TypeMismatchException("value type wrong: key %s", k.c_str());
        }
    }

    // TODO: read from config.
    // NOTE: Change carefully
    // See Terminall::Init
    // TODO: better color control
    auto colorscheme = new ColorSchemeElement[__kCharacterTypeCount];
    opts_[kOptColorScheme] = colorscheme;
    colorscheme[kNormal] = {Terminal::kDefault, Terminal::kDefault};
    colorscheme[kReverse] = {Terminal::kDefault | Terminal::kReverse,
                             Terminal::kDefault | Terminal::kReverse};
    colorscheme[kSelection] = {Terminal::kDefault | Terminal::kReverse,
                               Terminal::kDefault | Terminal::kReverse};
    colorscheme[kMenu] = {Terminal::kDefault,
                          Terminal::kBlack | Terminal::kBright};
    colorscheme[kLineNumber] = {Terminal::kYellow, Terminal::kDefault};

    colorscheme[kKeyword] = {Terminal::kBlue, Terminal::kDefault};
    colorscheme[kTypeBuiltin] = {Terminal::kBlue, Terminal::kDefault};
    colorscheme[kOperator] = {Terminal::kBlue, Terminal::kDefault};
    colorscheme[kString] = {Terminal::kGreen, Terminal::kDefault};
    colorscheme[kComment] = {Terminal::kCyan | Terminal::kItalic,
                             Terminal::kDefault};
    colorscheme[kNumber] = {Terminal::kYellow, Terminal::kDefault};
    colorscheme[kConstant] = {Terminal::kYellow, Terminal::kDefault};
    colorscheme[kFunction] = {Terminal::kDefault, Terminal::kDefault};
    colorscheme[kType] = {Terminal::kMagenta, Terminal::kDefault};
    colorscheme[kVariable] = {Terminal::kDefault, Terminal::kDefault};
    colorscheme[kDelimiter] = {Terminal::kDefault, Terminal::kDefault};
    colorscheme[kProperty] = {Terminal::kDefault, Terminal::kDefault};
    colorscheme[kLabel] = {Terminal::kDefault, Terminal::kDefault};

    user_config_valid_ = user_config_now_valid;
}

GlobalOpts::~GlobalOpts() {
    delete[] reinterpret_cast<ColorScheme*>(opts_[kOptColorScheme]);
}

Opts::Opts(GlobalOpts* global_options) : global_opts_(global_options) {}

void Opts::InitAfterBufferLoad(const Buffer* buffer) {
    // check filetype
    zstring_view ft = buffer->filetype();
    auto iter = global_opts_->filetype_opts_.find(ft);
    if (iter == global_opts_->filetype_opts_.end()) {
        return;
    }

    opts_ = iter->second;
}

}  // namespace mango