#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "pos.h"
#include "trie.h"
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
    virtual void Accept(size_t index, Cursor* cursor) = 0;
    virtual void Cancel() = 0;
};

class MangoPeel;

class PeelFilePathCompleter : Completer {
   public:
    PeelFilePathCompleter(std::string_view path_hint_, MangoPeel* peel);

    virtual void Suggest(const Pos& cursor_pos,
                         std::vector<std::string>& menu_entries);
    virtual void Accept(size_t index, Cursor* cursor);
    virtual void Cancel();

   private:
    std::vector<std::string> suggestions_;
    std::string path_hint_;
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
    virtual void Accept(size_t index, Cursor* cursor) override;
    virtual void Cancel() override;

    void Enable();
    void Disable();

    void BeforeAdd(const Pos& pos, const std::string& str);
    void BeforeDelete(const Range& range);

   private:
    void ChangeWordsBeforeAdd(const Range& range);
    void ChangeWordsBeforeDelete(const Range& range);

    void AddWordsOnRange(const Range& range);

    void AddWords(std::string_view str);

    enum class PosInWordRange { kNone, kBegin, kEnd, kMid };

    const Buffer* buffer_;
    Trie words_trie_;
    bool enabled_;

    std::vector<std::string> suggestions_;
    size_t bytes_of_word_before_cursor_;
};

}  // namespace mango