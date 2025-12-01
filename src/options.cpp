#include "options.h"

#include "buffer.h"
#include "file.h"
#include "filetype.h"
#include "fs.h"

namespace mango {

static constexpr const char* kDefaultConfigPath = "resource/config/config.json";
static constexpr const char* kDefaultColorschemePath =
    "resource/config/colorscheme.json";
static const std::string kUserConfigPath =
    std::string(Path::GetConfig()) + "mango-editor/config.json";
static const std::string kUserColorschemePath =
    std::string(Path::GetConfig()) + "mango-editor/colorscheme.json";

static const std::unordered_map<std::string_view, OptKey> kStrRepToOptKey{
    {"scroll_rows", kOptScrollRows},
    {"cursor_start_holding_interval", kOptCursorStartHoldingInterval},
    {"tab_stop", kOptTabStop},
    {"tab_space", kOptTabSpace},
    {"auto_indent", kOptAutoIndent},
    {"auto_pair", kOptAutoPair},
    {"wrap", kOptWrap},
    {"line_number", kOptLineNumber},
    {"status_line_sep_width", kOptStatusLineSepWidth},
    {"cmp_menu_max_height", kOptCmpMenuMaxHeight},
    {"cmp_menu_max_width", kOptCmpMenuMaxWidth},
    {"edit_history_max_item", kOptEditHistoryMaxItem},
    {"basic_word_completion", kOptBasicWordCompletion},
    {"truecolor", kOptTrueColor},
    {"logverbose", kOptLogVerbose},
    {"vi", kOptVi},
    {"cursor_blinking", kOptCursorBlinking},
    {"cursor_blinking_show_interval", kOptCursorBlinkingShowInterval},
    {"cursor_blinking_hide_interval", kOptCursorBlinkingHideInterval},
    {"auto_cmp_timeout", kOptAutoCmpTimeout},
};

static void OptStaticInit(const OptInfo*& opt_info) {
    static const auto static_opt_info = [] {
        auto static_opt_info = new OptInfo[__kOptKeyCount];
        static_opt_info[kOptScrollRows] = {OptScope::kGlobal, Type::kInteger};
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
        static_opt_info[kOptTrueColor] = {OptScope::kGlobal, Type::kBool};
        static_opt_info[kOptLogVerbose] = {OptScope::kGlobal, Type::kBool};
        static_opt_info[kOptVi] = {OptScope::kGlobal, Type::kBool};
        static_opt_info[kOptCursorBlinking] = {OptScope::kGlobal, Type::kBool};
        static_opt_info[kOptCursorBlinkingShowInterval] = {OptScope::kGlobal,
                                                           Type::kInteger};
        static_opt_info[kOptCursorBlinkingHideInterval] = {OptScope::kGlobal,
                                                           Type::kInteger};
        static_opt_info[kOptAutoCmpTimeout] = {OptScope::kGlobal,
                                               Type::kInteger};

        static_opt_info[kOptLineNumber] = {OptScope::kWindow, Type::kInteger};

        static_opt_info[kOptTabStop] = {OptScope::kBuffer, Type::kInteger};
        static_opt_info[kOptTabSpace] = {OptScope::kBuffer, Type::kBool};
        static_opt_info[kOptAutoIndent] = {OptScope::kBuffer, Type::kBool};
        static_opt_info[kOptAutoPair] = {OptScope::kBuffer, Type::kBool};
        static_opt_info[kOptWrap] = {OptScope::kBuffer, Type::kBool};

        static_opt_info[kOptColorScheme] = {OptScope::kGlobal, Type::kPtr};

        return const_cast<OptInfo*>(static_opt_info);
    }();
    opt_info = static_opt_info;
}

static std::unordered_map<std::string_view, ColorSchemeType>
    kStrToColorSchemeType{
        {"normal", kNormal},
        {"selection", kSelection},
        {"menu", kMenu},
        {"linenumber", kLineNumber},
        {"statusline", kStatusLine},

        {"keyword", kKeyword},
        {"typebuiltin", kTypeBuiltin},
        {"operator", kOperator},
        {"string", kString},
        {"comment", kComment},
        {"number", kNumber},
        {"constant", kConstant},
        {"function", kFunction},
        {"type", kType},
        {"variable", kVariable},
        {"delimiter", kDelimiter},
        {"property", kProperty},
        {"label", kLabel},
    };

static const std::unordered_map<std::string_view, Terminal::Color>
    kBasedColors = {
        {"default", Terminal::kDefault}, {"black", Terminal::kBlack},
        {"red", Terminal::kRed},         {"green", Terminal::kGreen},
        {"yellow", Terminal::kYellow},   {"blue", Terminal::kBlue},
        {"magenta", Terminal::kMagenta}, {"cyan", Terminal::kCyan},
        {"white", Terminal::kWhite},
};
static const std::unordered_map<std::string_view, Terminal::Effect> kEffects = {
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

const Terminal::Attr kTruecolorBegin = 0x000000;
const Terminal::Attr kTruecolorEnd = 0xFFFFFF;

static void GetColorScheme(bool truecolor, const Json& colorscheme_json,
                           ColorSchemeElement* colorscheme) {
    // Every colorscheme type should have fg and bg.
    // Fg and bg should have a color and >=0 effects.
    // We ignore unknown keys and values.
    int colorscheme_type_cnt = 0;
    for (const auto& [k, v] : colorscheme_json.items()) {
        const auto iter = kStrToColorSchemeType.find(k);
        if (iter == kStrToColorSchemeType.end()) {
            continue;
        }
        colorscheme_type_cnt++;

        ColorSchemeType type = iter->second;

        std::string_view attr_locs[2] = {"fg", "bg"};
        for (std::string_view attr_loc : attr_locs) {
            auto iter_attr = v.find(attr_loc);
            if (iter_attr == v.end()) {
                throw OptionLoadException(
                    "Option colorscheme%s/%s/%s not "
                    "exist",
                    truecolor ? "_truecolor" : "", k.c_str(), attr_loc.data());
            }

            if (!iter_attr->is_array()) {
                throw OptionLoadException(
                    "Option colorscheme%s/%s/%s is not "
                    "array",
                    truecolor ? "_truecolor" : "", k.c_str(), attr_loc.data());
            }

            int colors_cnt = 0;
            for (const auto& attr : iter_attr.value()) {
                const std::string& str = attr.get_ref<const std::string&>();
                if (kEffects.find(str) != kEffects.end()) {
                    if (attr_loc == "fg") {
                        colorscheme[type].fg |= kEffects.find(str)->second;
                    } else {
                        colorscheme[type].bg |= kEffects.find(str)->second;
                    }
                } else if (kBasedColors.find(str) != kBasedColors.end() &&
                           !truecolor) {
                    if (attr_loc == "fg") {
                        colorscheme[type].fg |= kBasedColors.find(str)->second;
                    } else {
                        colorscheme[type].bg |= kBasedColors.find(str)->second;
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
                            colorscheme[type].fg |= color;
                        } else {
                            colorscheme[type].bg |= color;
                        }
                    }
                }
            }
            if (colors_cnt != 1) {
                throw OptionLoadException(
                    "%s", "In colorscheme, Color cnt wrong, expect one");
            }
        }
    }
    if (colorscheme_type_cnt != __kColorSchemeTypeCount) {
        throw OptionLoadException("%s", "Colorscheme type cnt wrong");
    }
}

void GlobalOpts::TryApply(const Json& config, const Json& colorscheme_config) {
    for (const auto& [k, v] : config.items()) {
        auto filetype = IsFiletype(k);
        if (!filetype.empty()) {
            filetype_opts_.insert({filetype, {}});
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
                    filetype_opts_[filetype][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<bool>());
                } else if (opt_info.type == Type::kInteger &&
                           inner_v.is_number_integer()) {
                    filetype_opts_[filetype][opt_key] =
                        reinterpret_cast<void*>(inner_v.get<int64_t>());
                } else {
                    throw OptionLoadException(
                        "value type wrong: key: %s, v type: %d %d",
                        ("/" + std::string(filetype) + "/" + inner_k).c_str(),
                        opt_info.type,
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
            throw OptionLoadException("value type wrong: key %s", k.c_str());
        }
    }

    // Colorscheme
    const Json* colorscheme_json = nullptr;
    bool truecolor = GetOpt<bool>(kOptTrueColor);
    std::string colorscheme_str = config.at("colorscheme");
    if (colorscheme_str != "default") {
        colorscheme_json = &colorscheme_config.at(colorscheme_str);
    } else {
        colorscheme_json = &colorscheme_config.at(
            std::string("default") + (truecolor ? "_truecolor" : "8"));
    }

    auto colorscheme = new ColorSchemeElement[__kColorSchemeTypeCount];
    bzero(colorscheme, sizeof(ColorSchemeElement) * __kColorSchemeTypeCount);
    GetColorScheme(truecolor, *colorscheme_json, colorscheme);
    opts_[kOptColorScheme] = colorscheme;
}

// We try to first load users config and merge with default.
// If any exception is throwed, then we just use our default config.
// We don't wrap our default config loading in catch.
// If errs occur on out default config, just let it crash.
void GlobalOpts::LoadConfig() {
    std::string default_config_str =
        File(std::string(Path::GetAppRoot() + kDefaultConfigPath), "r", false)
            .ReadAll();
    std::string default_colorscheme_str =
        File(std::string(Path::GetAppRoot() + kDefaultColorschemePath), "r",
             false)
            .ReadAll();

    try {
        std::string user_config_str;
        if (File::FileReadable(kUserConfigPath)) {
            user_config_str = File(kUserConfigPath, "r", false).ReadAll();
        }
        std::string user_colorscheme_str;
        if (File::FileReadable(kUserColorschemePath)) {
            user_colorscheme_str =
                File(kUserColorschemePath, "r", false).ReadAll();
        }

        // We merge with user config
        Json config = Json::parse(default_config_str);
        Json colorscheme = Json::parse(default_colorscheme_str);
        if (!user_config_str.empty()) {
            Json user_config = Json::parse(user_config_str);
            config.update(user_config, true);
        }
        if (!user_colorscheme_str.empty()) {
            Json user_colorscheme = Json::parse(user_colorscheme_str);
            colorscheme.update(user_colorscheme, true);
        }
        TryApply(config, colorscheme);
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
    Json colorscheme = Json::parse(default_colorscheme_str);
    TryApply(config, colorscheme);
}

GlobalOpts::GlobalOpts() {
    OptStaticInit(opt_info_);
    LoadConfig();
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