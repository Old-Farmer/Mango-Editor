#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "result.h"

namespace mango {

class Editor;

// A Completer is an object that offers some suggestions and encapsulates the
// cmp context in its object. Outer world can use it make suggestions. When the
// user makes a decision to accept or cancel the suggestion, it can do sth
// according to the behavior.
class Completer {
   public:
    friend Editor;

    virtual Result Suggest(std::vector<std::string>& menu_entries) = 0;
    virtual void Accept(size_t index) = 0;
    virtual void Cancel() = 0;
};

class MangoPeel;

class PeelFilePathCompleter : Completer {
   public:
    PeelFilePathCompleter(std::string_view path_hint_, MangoPeel* peel);

    virtual Result Suggest(std::vector<std::string>& menu_entries);
    virtual void Accept(size_t index);
    virtual void Cancel();

   private:
    std::vector<std::string> suggestions_;
    std::string path_hint_;
};

}  // namespace mango