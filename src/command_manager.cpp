#include "command_manager.h"

#include <charconv>

#include "str.h"

namespace mango {

constexpr std::string_view kBoolTrue = "true";
constexpr std::string_view kBoolFalse = "false";

void CommandManager::AddCommand(const Command& command) {
    const std::string& name = command.name;
    commands_[name] = command;
}
void CommandManager::RemoveCommand(const std::string& name) {
    commands_.erase(name);
}
Result CommandManager::EvalCommand(const std::string& str, CommandArgs args,
                                   Command*& command) {
    auto splitted_str = StrSplit(str);
    if (splitted_str.empty()) {
        return kCommandEmpty;
    }
    // TODO: maybe use cpp-20 heterogeneous lookups for better performance
    auto iter = commands_.find(std::string(splitted_str[0]));
    if (iter == commands_.end()) {
        return kNotExist;
    }

    Command& c = iter->second;
    const auto real_argc = static_cast<int8_t>(splitted_str.size() - 1);
    if ((c.optional_argc == 0 && real_argc != c.argc) ||
        (real_argc + c.optional_argc < c.argc) || real_argc > c.argc) {
        command = &c;
        return kCommandInvalidArgs;
    }

    for (int8_t i = 0; i < real_argc; i++) {
        std::string_view substr = splitted_str[i + 1];
        if (c.types[i] == Type::kBool) {
            if (substr == kBoolTrue) {
                args[i] = true;
            } else if (substr == kBoolFalse) {
                args[i] = false;
            } else {
                command = &c;
                return kCommandInvalidArgs;
            }
        } else if (c.types[i] == Type::kInteger) {
            int64_t v;
            auto [ptr, errc] = std::from_chars(
                substr.data(), substr.data() + substr.size(), v);
            if (errc != std::errc() || ptr != substr.data() + substr.size()) {
                command = &c;
                return kCommandInvalidArgs;
            }
            args[i] = v;
        } else if (c.types[i] == Type::kString) {
            args[i] = std::string(substr);
        } else {
            MGO_ASSERT(false);
        }
    }
    // Other optional args(if have) will be default init to std::nullopt
    command = &c;
    return kOk;
}

}  // namespace mango
