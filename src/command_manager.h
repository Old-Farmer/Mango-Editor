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
    std::string short_name;
    std::string description;
    CommandArgTypes types;  // Types of arguments
    std::function<void(CommandArgs)> f;
    int8_t argc = 0;
    int8_t optional_argc = 0;  // optional argument count, optional args must
                               // all be the righmost ones.
};

class CommandManager {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(CommandManager);
    MGO_DELETE_COPY(CommandManager);
    MGO_DELETE_MOVE(CommandManager);


    // throw CommandNameExistException if command name conflict
    void AddCommand(const Command& command);
    // should only be command name not short name.
    void RemoveCommand(const std::string& name);
    // return
    // kOk
    // kNotExist,
    // kCommandInvalidArgs,
    // kCommandEmpty
    // if return kOk or kCommandInvalidArgs, command will be set.
    // if return kOk, args will also be filled.
    Result EvalCommand(std::string_view str, CommandArgs args,
                       Command*& command);

   private:
    std::unordered_map<std::string, Command*> commands_;
};

}  // namespace mango
