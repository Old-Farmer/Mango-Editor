#pragma once

#include <functional>
#include <string>
#include <variant>

#include "options.h"
#include "result.h"
#include "utils.h"

namespace mango {

constexpr int8_t kMaxCommandCnt = 6;

using CommandArg = std::optional<std::variant<bool, int64_t, std::string>>;
using CommandArgs = CommandArg[kMaxCommandCnt];
using CommandArgTypes = Type[kMaxCommandCnt];

struct Command {
    std::string name;
    std::string description;
    CommandArgTypes types;  // Types of arguments
    std::function<void(CommandArgs)> f;
    int8_t argc;
    int8_t optional_argc = 0;  // optional argument count, optional args must
                               // all be the righmost ones.
};

class CommandManager {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(CommandManager);
    MGO_DELETE_COPY(CommandManager);
    MGO_DELETE_MOVE(CommandManager);

    void AddCommand(const Command& command);
    void RemoveCommand(const std::string& name);
    // return
    // kOk
    // kNotExist,
    // kCommandInvalidArgs,
    // kCommandEmpty
    Result EvalCommand(const std::string& str, CommandArgs args,
                       Command*& command);

   private:
    std::unordered_map<std::string, Command> commands_;
};

}  // namespace mango