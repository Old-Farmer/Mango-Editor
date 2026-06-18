#include "options.h"

#include <unordered_set>

#include "buffer.h"
#include "file.h"
#include "filetype.h"
#include "fs.h"

namespace charxed {

namespace {
constexpr const char* kDefaultConfigPath = "resource/config/config.json";
constexpr const char* kDefaultThemePath = "resource/config/theme.json";

// clang-format off
static const std::unordered_map<std::string_view, OptKey> kStrRepToOptKey{
#define X(t, str, ...) {#str, t},
    CHX_BUFFER_OPT_TABLE
    CHX_WINDOW_OPT_TABLE
    CHX_GLOBAL_OPT_TABLE
#undef X
};

static std::unordered_map<std::string_view, ThemeType>
    kStrToThemeType{
#define X(t, str) {#str, t},
    CHX_THEME_TABLE
#undef X
};
// clang-format on

const std::unordered_map<std::string_view, Terminal::Color> kBasedColors = {
    {"default", Terminal::kDefault}, {"black", Terminal::kBlack},
    {"red", Terminal::kRed},         {"green", Terminal::kGreen},
    {"yellow", Terminal::kYellow},   {"blue", Terminal::kBlue},
    {"magenta", Terminal::kMagenta}, {"cyan", Terminal::kCyan},
    {"white", Terminal::kWhite},
};

const std::unordered_map<std::string_view, Terminal::Effect> kEffects = {
    {"bold", Terminal::kBold},
    {"underline", Terminal::kUnderline},
    {"reverse", Terminal::kReverse},
    {"italic", Terminal::kItalic},
    {"blink", Terminal::kBlink},
    {"brighter", Terminal::kBright},
    {"dim", Terminal::kDim},
    {"strikeout", Terminal::kStrikeOut},
    {"underline2", Terminal::kUnderline2},
    {"overline", Terminal::kOverline},
    {"invisible", Terminal::kInvisible},
};

const std::unordered_set<ThemeType> kThemeTypeFgBgMustAllExist = {
    kNormal,
    kStatusLine,
    kMenu,
    kSidebar,
};

constexpr Terminal::Attr kTruecolorBegin = 0x000000;
constexpr Terminal::Attr kTruecolorEnd = 0xFFFFFF;

void GetTheme(bool truecolor, const Json& theme_json, ThemeElement* theme) {
    // Every theme type should have fg and bg.
    // Fg and bg should have a color and >=0 effects.
    // We ignore unknown keys and values.
    int theme_type_cnt = 0;
    for (const auto& [k, v] : theme_json.items()) {
        const auto iter = kStrToThemeType.find(k);
        if (iter == kStrToThemeType.end()) {
            continue;
        }
        theme_type_cnt++;

        ThemeType type = iter->second;

        std::string_view attr_locs[2] = {"fg", "bg"};
        for (std::string_view attr_loc : attr_locs) {
            auto iter_attr = v.find(attr_loc);
            if (iter_attr == v.end()) {
                // allow don't have fg or bg in some theme type
                continue;
            }

            if (!iter_attr->is_array()) {
                throw OptionLoadException(
                    "Option theme{}/{}/{} is not "
                    "array",
                    truecolor ? "_truecolor" : "", k, attr_loc);
            }

            int colors_cnt = 0;
            for (const auto& attr : iter_attr.value()) {
                const std::string& str = attr.get_ref<const std::string&>();
                if (kEffects.find(str) != kEffects.end()) {
                    if (attr_loc == "fg") {
                        theme[type].fg |= kEffects.find(str)->second;
                    } else {
                        theme[type].bg |= kEffects.find(str)->second;
                    }
                } else if (kBasedColors.find(str) != kBasedColors.end() &&
                           !truecolor) {
                    if (attr_loc == "fg") {
                        theme[type].fg |= kBasedColors.find(str)->second;
                    } else {
                        theme[type].bg |= kBasedColors.find(str)->second;
                    }
                    colors_cnt++;
                } else if (truecolor) {
                    // skip #
                    Terminal::Attr color =
                        strtoll(str.c_str() + 1, nullptr, 16);
                    if (color >= kTruecolorBegin && color <= kTruecolorEnd) {
                        colors_cnt++;
                        if (color ==
                            kTruecolorBegin) {  // See Termbox2 tb_set_output
                            color = Terminal::kHiBlack;
                        }
                        if (attr_loc == "fg") {
                            theme[type].fg |= color;
                        } else {
                            theme[type].bg |= color;
                        }
                    }
                }
            }
            if (colors_cnt != 1) {
                throw OptionLoadException(
                    "{}", "In theme, Color cnt wrong, expect one");
            }
            if (attr_loc == "fg") {
                theme[type].fg_exist = true;
            } else {
                theme[type].bg_exist = true;
            }
        }

        if (kThemeTypeFgBgMustAllExist.find(type) !=
            kThemeTypeFgBgMustAllExist.end()) {
            if (!theme[type].bg_exist || !theme[type].fg_exist) {
                throw OptionLoadException(
                    "Option theme{}/{} must both has \"bg\" and \"fg\" "
                    "color",
                    truecolor ? "_truecolor" : "", k);
            }
        }
    }

    theme[kNormalFg] = theme[kNormal];
    theme[kNormalFg].bg_exist = false;
    theme_type_cnt++;

    if (theme_type_cnt != _kThemeTypeCount) {
        throw OptionLoadException("{}", "Theme type cnt wrong");
    }
}

}  // namespace

OptInfo GlobalOpts::GetOptInfo(OptKey key) {
    switch (key) {
#define X(k, str, type) \
    case k:             \
        return {OptScope::kBuffer, Type::type};
        CHX_BUFFER_OPT_TABLE
#undef X
#define X(k, str, type) \
    case k:             \
        return {OptScope::kWindow, Type::type};
        CHX_WINDOW_OPT_TABLE
#undef X
#define X(k, str, type) \
    case k:             \
        return {OptScope::kGlobal, Type::type};
        CHX_GLOBAL_OPT_TABLE
#undef X
        default:
            CHX_ASSERT(false);
            return {};
    }
}

void GlobalOpts::TryApply(const Json& config, const Json& theme_config) {
    for (const auto& [k, v] : config.items()) {
        auto filetype = InnerStrRepToFileType(k);
        if (filetype.has_value()) {
            for (const auto& [inner_k, inner_v] : v.items()) {
                auto iter = kStrRepToOptKey.find(inner_k);
                if (iter == kStrRepToOptKey.end()) {
                    continue;
                }
                OptKey opt_key = iter->second;
                const OptInfo& opt_info = GetOptInfo(opt_key);
                if (opt_info.scope != OptScope::kBuffer) {
                    continue;
                }

                if (opt_info.type == Type::kBool && inner_v.is_boolean()) {
                    filetype_opts_[static_cast<int>(*filetype)][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<bool>());
                } else if (opt_info.type == Type::kInteger &&
                           inner_v.is_number_integer()) {
                    filetype_opts_[static_cast<int>(*filetype)][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<int64_t>());
                } else {
                    throw OptionLoadException(
                        "value type wrong: key: {}, v type: {} {}",
                        "/" + k + "/" + inner_k,
                        static_cast<int>(opt_info.type),
                        static_cast<int>(
                            inner_v.type()));  // flatten rep of key
                }
            }
            continue;
        }

        auto iter = kStrRepToOptKey.find(k);
        if (iter == kStrRepToOptKey.end()) {
            continue;
        }
        OptKey opt_key = iter->second;
        const OptInfo& opt_info = GetOptInfo(opt_key);
        if (opt_info.type == Type::kBool && v.is_boolean()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<bool>());
        } else if (opt_info.type == Type::kInteger && v.is_number_integer()) {
            opts_[opt_key] = reinterpret_cast<void*>(v.get<int64_t>());
        } else {
            throw OptionLoadException("value type wrong: key {}", k);
        }
    }

    // Theme
    const Json* theme_json = nullptr;
    bool truecolor = GetOpt<bool>(kOptTrueColor);
    std::string theme_str = config.at("theme");
    if (theme_str != "default") {
        theme_json = &theme_config.at(theme_str);
    } else {
        theme_json = &theme_config.at(std::string("default") +
                                      (truecolor ? "_truecolor" : "8"));
    }

    auto theme = new ThemeElement[_kThemeTypeCount];
    bzero(theme,
          sizeof(ThemeElement) * _kThemeTypeCount);  // For attr bit wise or
    GetTheme(truecolor, *theme_json, theme);
    opts_[kOptTheme] = theme;
}

// We try to first load users config and merge with default.
// If any exception is throwed, then we just use our default config.
// We don't wrap our default config loading in catch.
// If errs occur on out default config, just let it crash.
void GlobalOpts::LoadConfig() {
    EOLSeq eol_seq;
    std::string default_config_str =
        File(std::string(Path::GetAppRoot() + kDefaultConfigPath), "r")
            .ReadAll(eol_seq);
    std::string default_theme_str =
        File(std::string(Path::GetAppRoot() + kDefaultThemePath), "r")
            .ReadAll(eol_seq);

    try {
        std::string user_config_str;
        if (File::FileReadable(kUserConfigPath)) {
            user_config_str = File(kUserConfigPath, "r").ReadAll(eol_seq);
        }
        std::string user_theme_str;
        if (File::FileReadable(kUserThemePath)) {
            user_theme_str = File(kUserThemePath, "r").ReadAll(eol_seq);
        }

        // We merge with user config
        Json config = Json::parse(default_config_str);
        Json theme = Json::parse(default_theme_str);
        if (!user_config_str.empty()) {
            Json user_config = Json::parse(user_config_str);
            config.update(user_config, true);
        }
        if (!user_theme_str.empty()) {
            Json user_theme = Json::parse(user_theme_str);
            theme.update(user_theme, true);
        }
        TryApply(config, theme);
        user_config_valid_ = true;
        return;
    } catch (Exception& e) {
        user_config_valid_ = false;
        user_config_error_reason_ = e.what();
    } catch (Json::exception& e) {
        user_config_valid_ = false;
        user_config_error_reason_ = e.what();
    }

    Json config = Json::parse(default_config_str);
    Json theme = Json::parse(default_theme_str);
    TryApply(config, theme);
}

GlobalOpts::GlobalOpts()
    : kUserConfigPath(Path::GetXDGPath(XDGPath::kConfig) + "config.json"),
      kUserThemePath(Path::GetXDGPath(XDGPath::kConfig) + "theme.json") {
    LoadConfig();
}

GlobalOpts::~GlobalOpts() {
    delete[] reinterpret_cast<Theme*>(opts_[kOptTheme]);
}

Opts::Opts(GlobalOpts* global_options) : global_opts_(global_options) {}

void Opts::InitAfterBufferLoad(const Buffer* buffer) {
    opts_ = global_opts_->filetype_opts_[static_cast<int>(buffer->filetype())];
}

}  // namespace charxed
