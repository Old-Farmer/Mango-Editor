#pragma once

#include <functional>
#include <string>
#include <variant>

#include "options.h"
#include "result.h"
#include "utils.h"

namespace charxed {

constexpr int8_t kMaxCommandArgCnt = 6;

using CommandArg = std::optional<std::variant<bool, int64_t, std::string>>;
using CommandArgs = std::array<CommandArg, kMaxCommandArgCnt>;
using CommandArgTypes = std::array<Type, kMaxCommandArgCnt>;
using CommandHandler = std::function<void(const CommandArgs&)>;

struct Command {
    std::string name;
    std::string short_name;
    std::string description;
    CommandArgTypes types;  // Types of arguments
    CommandHandler f;
    int8_t argc = 0;
    int8_t optional_argc = 0;  // optional argument count, optional args must
                               // all be the righmost ones.
};

class CommandManager {
   public:
    CHX_DEFAULT_CONSTRUCT_DESTRUCT(CommandManager);
    CHX_DELETE_COPY(CommandManager);
    CHX_DELETE_MOVE(CommandManager);

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
    Result EvalCommand(std::string_view str, CommandArgs& args,
                       Command*& command);

    const std::vector<std::unique_ptr<Command>>& commands() {
        return commands_;
    }

   private:
    std::unordered_map<std::string, Command*> name_to_commands_;
    // Because remove is a very low-frequency op,
    // we use a vector to store commands and expose it to the outside world,
    // e.g. for command completion
    std::vector<std::unique_ptr<Command>> commands_;
};

}  // namespace charxed
