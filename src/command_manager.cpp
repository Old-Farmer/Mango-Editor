#include "command_manager.h"

#include "str.h"

namespace mango {

constexpr std::string_view kBoolTrue = "true";
constexpr std::string_view kBoolFalse = "false";

void CommandManager::AddCommand(const Command& command) {
    std::string name = command.name;
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
    if (c.argc != static_cast<int8_t>(splitted_str.size() - 1)) {
        return kCommandInvalidArgs;
    }

    for (int i = 0; i < c.argc; i++) {
        if (c.types[i] == Type::kBool) {
            if (splitted_str[i + 1] == kBoolTrue) {
                args[i] = true;
            } else if (splitted_str[i + 1] == kBoolFalse) {
                args[i] = false;
            } else {
                return kCommandInvalidArgs;
            }
        } else if (c.types[i] == Type::kInteger) {
            errno = 0;
            char* endptr;
            int64_t n = strtoll(splitted_str[i + 1].data(), &endptr, 10);
            if (errno != 0) {
                return kCommandInvalidArgs;
            }
            // Linux strtoll doesn't set errno when encounter other character
            // at the beginning. Also, we need to ensure that between two spaces
            // is a valid number.
            if (endptr !=
                splitted_str[i + 1].data() + splitted_str[i + 1].size()) {
                return kCommandInvalidArgs;
            }
            args[i] = n;
        } else if (c.types[i] == Type::kString) {
            args[i] = std::string(splitted_str[i + 1]);
        } else {
            MGO_ASSERT(false);
        }
    }
    command = &c;
    return kOk;
}

}  // namespace mango