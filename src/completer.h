#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "pos.h"
#include "utils.h"

namespace mango {

struct Cursor;

// A Completer is an object that offers some suggestions and encapsulates the
// cmp context in its object. Outer world can use it make suggestions. When the
// user makes a decision to accept or cancel the suggestion, it can do sth
// according to the behavior.
class Completer {
   public:
    virtual ~Completer() {};

    virtual void Suggest(const Pos& cursor_pos,
                         std::vector<std::string>& menu_entries) = 0;
    // return kOk, or kRetriggerCmp to hint the outside world to trigger again.
    virtual Result Accept(size_t index, Cursor* cursor) = 0;
    virtual void Cancel() = 0;
};

class MangoPeel;
class BufferManager;

class PeelCompleter : public Completer {
   public:
    PeelCompleter(MangoPeel* peel, BufferManager* buffer_manager);

    virtual void Suggest(const Pos& cursor_pos,
                         std::vector<std::string>& menu_entries);
    virtual Result Accept(size_t index, Cursor* cursor);
    virtual void Cancel();

   private:
    MangoPeel* peel_;
    BufferManager* buffer_manager_;
    std::vector<std::string> suggestions_;
    size_t this_arg_offset_;

    enum class SuggestType {
        kBuffer,
        kPath
    };
    SuggestType type_;
};

class Buffer;
struct BufferEdit;

// A simple basic word completer for buffer
class BufferBasicWordCompleter : public Completer {
   public:
    BufferBasicWordCompleter(const Buffer* buffer);
    MGO_DELETE_COPY(BufferBasicWordCompleter);
    MGO_DELETE_MOVE(BufferBasicWordCompleter);

    virtual void Suggest(const Pos& cursor_pos,
                         std::vector<std::string>& menu_entries) override;
    virtual Result Accept(size_t index, Cursor* cursor) override;
    virtual void Cancel() override;

    void Enable();
    void Disable();

   private:
    std::vector<std::string_view> GetWords(std::string_view str);

    const Buffer* buffer_;
    bool enabled_;

    std::vector<std::string> suggestions_;
    size_t bytes_of_word_before_cursor_;
};

}  // namespace mango