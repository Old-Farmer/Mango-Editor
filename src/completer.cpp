#include "completer.h"

#include <algorithm>
#include <unordered_set>

#include "buffer.h"
#include "character.h"
#include "cursor.h"
#include "window.h"

namespace mango {

BufferBasicWordCompleter::BufferBasicWordCompleter(const Buffer* buffer) {
    buffer_ = buffer;
}

void BufferBasicWordCompleter::Suggest(const Pos& cursor_pos,
                                       std::vector<std::string>& menu_entries) {
    std::string cur_word_prefix;
    int64_t byte_offset = cursor_pos.byte_offset;
    const std::string& cur_line = buffer_->GetLine(cursor_pos.line);
    Character character;
    int byte_len;
    while (byte_offset > 0) {
        Result res = PrevCharacter(cur_line, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            cur_word_prefix.push_back(c);
        } else {
            break;
        }
        byte_offset -= byte_len;
    }
    if (cur_word_prefix.empty()) {
        menu_entries = {};
        return;
    }

    std::reverse(cur_word_prefix.begin(), cur_word_prefix.end());

    // TODO: Performance: line cache.
    // TODO: If performance is really bad on big files, maybe we can use
    // codepoints instead of graphemes to do search(or organize index).
    // Then we expand to grapheme boundaries.
    // TODO: Maybe we should lock text before accpet or cancel?
    std::unordered_set<std::string> s;
    size_t lines = buffer_->LineCnt();
    for (size_t i = 0; i < lines; i++) {
        const std::string& line = buffer_->GetLine(i);
        auto words = GetWords(line);
        for (auto word : words) {
            if (word.end - word.begin <= cur_word_prefix.size()) {
                continue;
            }

            bool is_prefix = true;
            for (size_t i = 0; i < cur_word_prefix.size(); i++) {
                if (line[word.begin + i] != cur_word_prefix[i]) {
                    is_prefix = false;
                    break;
                }
            }
            if (is_prefix) {
                s.insert({line.c_str() + word.begin, word.end - word.begin});
            }
        }
    }

    menu_entries = std::vector<std::string>(s.begin(), s.end());
    suggestions_ = menu_entries;
    bytes_of_word_before_cursor_ = cur_word_prefix.size();
}

void BufferBasicWordCompleter::Accept(size_t index, Cursor* cursor) {
    MGO_ASSERT(cursor->in_window);
    MGO_ASSERT(index < suggestions_.size());
    Pos cursor_pos = cursor->ToPos();
    cursor->in_window->Replace(
        {{cursor_pos.line,
          cursor_pos.byte_offset - bytes_of_word_before_cursor_},
         cursor_pos},
        std::move(suggestions_[index]));  // Ignore ret
    suggestions_.clear();
}

void BufferBasicWordCompleter::Cancel() { suggestions_.clear(); }

void BufferBasicWordCompleter::Enable() { enabled_ = true; }

void BufferBasicWordCompleter::Disable() { enabled_ = false; }

std::vector<BufferBasicWordCompleter::WordRange>
BufferBasicWordCompleter::GetWords(std::string_view str) {
    std::vector<WordRange> words;

    size_t byte_offset = 0;
    size_t byte_offset_end = str.size();
    Character character;
    int byte_len;
    bool found_word_character = false;
    size_t word_begin, word_end;
    while (byte_offset < byte_offset_end) {
        Result res = ThisCharacter(str, byte_offset, character, byte_len);
        MGO_ASSERT(res == kOk);
        char c;
        if (character.Ascii(c) && IsWordCharacter(c)) {
            if (!found_word_character) {
                word_begin = byte_offset;
                found_word_character = true;
            }
        } else {
            if (found_word_character) {
                word_end = byte_offset;
                words.push_back({word_begin, word_end});
                found_word_character = false;
            }
        }
        byte_offset += byte_len;
    }
    if (found_word_character) {
        word_end = byte_offset;
        words.push_back({word_begin, word_end});
        found_word_character = false;
    }
    return words;
}

}  // namespace mango