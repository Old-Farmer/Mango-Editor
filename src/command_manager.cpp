#include "command_manager.h"

#include <charconv>

#include "str.h"

namespace mango {

constexpr std::string_view kBoolTrue = "true";
constexpr std::string_view kBoolFalse = "false";

void CommandManager::AddCommand(const Command& command) {
    if (commands_.count(command.name) == 1 ||
        commands_.count(command.short_name)) {
        throw CommandNameExistException("name: {}, short name: {}",
                                        command.name, command.short_name);
    }
    auto c = new Command(command);
    commands_[c->name] = c;
    if (!c->short_name.empty()) {
        commands_[c->short_name] = c;
    }
}
void CommandManager::RemoveCommand(const std::string& name) {
    auto iter = commands_.find(name);
    if (iter == commands_.end()) {
        return;
    }
    auto c = iter->second;
    if (!c->short_name.empty()) {
        commands_.erase(c->short_name);
    }
    delete c;
    commands_.erase(iter);
}
Result CommandManager::EvalCommand(std::string_view str, CommandArgs args,
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

    Command* c = iter->second;
    const auto real_argc = static_cast<int8_t>(splitted_str.size() - 1);
    if ((c->optional_argc == 0 && real_argc != c->argc) ||
        (real_argc + c->optional_argc < c->argc) || real_argc > c->argc) {
        command = c;
        return kCommandInvalidArgs;
    }

    for (int8_t i = 0; i < real_argc; i++) {
        std::string_view substr = splitted_str[i + 1];
        if (c->types[i] == Type::kBool) {
            if (substr == kBoolTrue) {
                args[i] = true;
            } else if (substr == kBoolFalse) {
                args[i] = false;
            } else {
                command = c;
                return kCommandInvalidArgs;
            }
        } else if (c->types[i] == Type::kInteger) {
            int64_t v;
            auto [ptr, errc] = std::from_chars(
                substr.data(), substr.data() + substr.size(), v);
            if (errc != std::errc() || ptr != substr.data() + substr.size()) {
                command = c;
                return kCommandInvalidArgs;
            }
            args[i] = v;
        } else if (c->types[i] == Type::kString) {
            args[i] = std::string(substr);
        } else {
            MGO_ASSERT(false);
        }
    }
    // Other optional args(if have) will be default init to std::nullopt
    command = c;
    return kOk;
}

}  // namespace mango
