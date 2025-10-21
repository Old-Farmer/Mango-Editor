#pragma once

#include <functional>
#include <string>
#include <variant>

#include "result.h"
#include "utils.h"

namespace mango {

constexpr int8_t kMaxCommandCnt = 6;

enum class Type {
    kBool,     // bool
    kInteger,  // int64_t
    kString,   // std::string
};

using CommandArg = std::variant<bool, int64_t, std::string>;
using CommandArgs = CommandArg[kMaxCommandCnt];
using CommandArgTypes = Type[kMaxCommandCnt];

struct Command {
    std::string name;
    std::string description;
    CommandArgTypes types;
    std::function<void(CommandArgs)> f;
    int8_t argc;
};

class CommandManager {
   public:
    MANGO_DEFAULT_CONSTRUCT_DESTRUCT(CommandManager);
    MANGO_DELETE_COPY(CommandManager);
    MANGO_DELETE_MOVE(CommandManager);

    void AddCommand(Command command);
    void RemoveCommand(const std::string& name);
    // return 
    // kOk
    // kCommandNotExist,
    // kCommandInvalidArgs,
    Result EvalCommand(const std::string& str, CommandArgs args,
                       Command*& command);

   private:
    std::unordered_map<std::string, Command> commands_;
};

}  // namespace mango